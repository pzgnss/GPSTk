#include "SingleSolution.h"

#include"SimpleFilter.hpp"
#include"WinUtils.h"
#include"ComputeWeightSimple.h"

#include"PositionEquations.h"
#include"InterSystemBias.h"
#include"ClockBiasEquations.h"

#include"LICSDetector.hpp"
#include"OneFreqCSDetector.hpp"
#include"Decimate.hpp"
#include"BasicModel.hpp"
#include"NeillTropModel.hpp"
#include"PowerSum.hpp"
#include"ComputeMOPSWeights.hpp"
#include"ComputeTropModel.hpp"
#include"LinearCombinations.hpp"
#include"MWCSDetector.hpp"

#include<memory>

using namespace gpstk;

namespace pod
{

    SingleSolution::SingleSolution(GnssDataStore_sptr data_ptr)
        : GnssSolution(data_ptr,50.0), codeSmWindowSize(600)      
    { }

    //
    void SingleSolution::process()
    {
        updateRequaredObs();

        SimpleFilter PRFilter(codeL1);
        if (data->ionoCorrector.getType() == ComputeIonoModel::DualFreq)
            PRFilter.addFilteredType(TypeID::P2);

        SimpleFilter SNRFilter(TypeID::S1, 30, DBL_MAX);
		std::list<Position> nomPos;
        //nominalPos.asECEF();
        //int i = 0;
        //for (auto& it : confReader().getListValueAsDouble("nominalPosition"))
        //    nominalPos[i++] = it;

        // Object to decimate data
        Decimate decimateData(
            confReader().getValueAsDouble("decimationInterval"),
            confReader().getValueAsDouble("decimationTolerance"),
            data->SP3EphList.getInitialTime());

        // basic model object
        BasicModel model;
        model.setDefaultEphemeris(data->SP3EphList);
        model.setDefaultObservable(codeL1);
        model.setMinElev(confReader().getValueAsInt("ElMask"));

        //troposhere modeling object
		std::unique_ptr<NeillTropModel> uptrTropModel = std::make_unique<NeillTropModel>();
        ComputeTropModel computeTropo(*uptrTropModel);

        //
        ComputeWeightSimple w(2);
        KalmanSolver solver(Equations);

        KalmanSolverFB solverFb(Equations);

        if (forwardBackwardCycles > 0)
        {
            solverFb.setCyclesNumber(forwardBackwardCycles);
            solverFb.setLimits(confReader().getListValueAsDouble("codeLimList"), confReader().getListValueAsDouble("phaseLimList"));
        }

        bool firstTime = true;

        RinexEpoch gRin;

        for (auto &obsFile : data->getObsFiles(opts().SiteRover))
        {
			std::cout << obsFile << std::endl;

            //Input observation file stream
            Rinex3ObsStream rin;

            //Open Rinex observations file in read-only mode
            rin.open(obsFile, std::ios::in);

            rin.exceptions(std::ios::failbit);
            Rinex3ObsHeader roh;

            //read the header
            rin >> roh;
            gMap.header = roh;

            //update code smoothers sampling rate, according to current rinex file sampling rate
            codeSmoother.setInterval(codeSmWindowSize / roh.interval);

            //read all epochs
            while (rin >> gRin)
            {
                //work around for post header comments 
                if (gRin.getBody().size() == 0)
                {
                    printMsg(gRin.getHeader().epoch, "Empty epoch record in Rinex file");
                    continue;
                }

                auto& t = gRin.getHeader().epoch;

                //keep only satellites from satellites systems selected for processing
                gRin.keepOnlySatSystems(opts().systems);

                //keep only types used for processing
                gRin.keepOnlyTypeID(requireObs.getRequiredType());

                //compute approximate position

				if (apprPos().getPosition(gRin, nominalPos))
					continue;
				if (firstTime)
				{
					std::cout << std::setprecision(10) << nominalPos << std::endl;
					firstTime = false;
				}

                //update approximate position 
                data->ionoCorrector.setNominalPosition(nominalPos);
                uptrTropModel->setAllParameters(t, nominalPos);
                model.rxPos = nominalPos;

                //filter out satellites with incomplete observables set 
                gRin >> requireObs;
                gRin >> PRFilter;
                gRin >> SNRFilter;
                gRin >> computeLinear;

                // smooth pseudoranges if required
                if (opts().isSmoothCode)
                    gRin >> codeSmoother;

                if (gRin.getBody().size() == 0)
                {
                    printMsg(gRin.getHeader().epoch, "All SV has been rejected.");
                    continue;
                }

                if (decimateData.check(gRin))
                    continue;

                gRin >> model;
                gRin >> computeTropo;
                gRin >> data->ionoCorrector;
                gRin >> oMinusC;
                gRin >> w;
				int minSvNum = gRin.getBody().getSatSystems().size() + 3;
                if (forwardBackwardCycles > 0)
                {
					solverFb.setMinSatNumber(minSvNum);
                    gRin >> solverFb;
                }
                else
                {
					solver.setMinSatNumber(minSvNum);
                    gRin >> solver;
                    auto ep = opts().fullOutput ? GnssEpoch(gRin.getBody()) : GnssEpoch();
                    // updateNomPos(solverFB);
                    printSolution( solver, t, ep);
                    gMap.data.insert(std::make_pair(t, ep));
                }
            }
        }

        if (forwardBackwardCycles > 0)
        {
			std::cout << "Fw-Bw part started" << std::endl;
            solverFb.reProcess();
            RinexEpoch gRin;
			std::cout << "Last process part started" << std::endl;
            while (solverFb.lastProcess(gRin))
            {
                auto ep = opts().fullOutput ? GnssEpoch(gRin.getBody()) : GnssEpoch();
                //updateNomPos(solverFB);
                printSolution( solverFb, gRin.getHeader().epoch, ep);
                gMap.data.insert(std::make_pair(gRin.getHeader().epoch, ep));
            }
			std::cout << "measurments rejected: " << solverFb.rejectedMeasurements << std::endl;
        }
    }

    void SingleSolution::updateNomPos(KalmanSolver &solver)
    {
        PowerSum psum;
        for (auto it : solver.PostfitResiduals())
            psum.add(it);
        double sigma = sqrt(psum.variance());

        int numSats = solver.PostfitResiduals().size();
        Position newPos;
        if (numSats >= 4 && sigma < getMaxSigma())
        {
            newPos[0] = nominalPos.X() + solver.getSolution(FilterParameter( TypeID::dx));   // dx    - #4
            newPos[1] = nominalPos.Y() + solver.getSolution(FilterParameter(TypeID::dy));    // dy    - #5
            newPos[2] = nominalPos.Z() + solver.getSolution(FilterParameter(TypeID::dz));    // dz    - #6

            nominalPos = newPos;
        }
    }

    void SingleSolution::configureSolver()
    {
        Equations->clearEquations();
        // White noise stochastic models
        auto  coord = std::make_unique<PositionEquations>();

        double sigma = confReader().getValueAsDouble("posSigma");
        if (opts().dynamics == GnssDataStore::Dynamics::Static)
        {
            coord->setStochasicModel(std::make_shared<ConstantModel>());
        }
        else  if (opts().dynamics == GnssDataStore::Dynamics::Kinematic)
        {
            coord->setStochasicModel(std::make_shared<WhiteNoiseModel>(sigma));
        }
        else if (opts().dynamics == GnssDataStore::Dynamics::RandomWalk)
        {
            
            for (const auto& it : coord->getParameters())
            {
                coord->setStochasicModel(it, std::make_shared<RandomWalkModel>(sigma));
            }
        }

        //add position equations
        Equations->addEquation(std::move(coord));

        Equations->addEquation(std::make_unique<ClockBiasEquations>());

        if (opts().systems.size() > 1)
            Equations->addEquation(/*std::move(bias)*/std::make_unique<InterSystemBias>());

        Equations->residTypes() = TypeIDSet{ TypeID::postfitC };
        forwardBackwardCycles = confReader().getValueAsInt("forwardBackwardCycles");
    }

    void SingleSolution::updateRequaredObs()
    {
        LinearCombinations comm;
        bool useC1 = confReader().getValueAsBoolean("useC1");
        computeLinear.setUseC1(useC1);
        
        configureSolver();

        if (useC1)
        {
            codeL1 = TypeID::C1;
            oMinusC.add(std::make_unique<PrefitC1>(false));
            Equations->measTypes() = TypeIDSet{ TypeID::prefitC };
        }
        else
        {
            codeL1 = TypeID::P1;
            Equations->measTypes() = TypeIDSet{ TypeID::prefitP1 };
            oMinusC.add(std::make_unique<PrefitP1>(false));
        }

        requireObs.addRequiredType(codeL1);

        requireObs.addRequiredType(codeL1);
        requireObs.addRequiredType(TypeID::C1);
        requireObs.addRequiredType(TypeID::P2);
        requireObs.addRequiredType(TypeID::L1);
        requireObs.addRequiredType(TypeID::L2);
        requireObs.addRequiredType(TypeID::LLI1);
        requireObs.addRequiredType(TypeID::LLI2);
        requireObs.addRequiredType(TypeID::S1);

        if (opts().isSmoothCode)
        {
            codeSmoother.addSmoother(CodeSmoother(codeL1));
            codeSmoother.addSmoother(CodeSmoother(TypeID::P2));

            // add linear combinations, requared  for CS detections 
            computeLinear.add(std::make_unique<LICombimnation>());
            computeLinear.add(std::make_unique<MWoubenna>());

            //define and add  CS markers
            codeSmoother.addScMarker(std::make_unique<LICSDetector>());
            codeSmoother.addScMarker(std::make_unique<MWCSDetector>());
        }
    }




}