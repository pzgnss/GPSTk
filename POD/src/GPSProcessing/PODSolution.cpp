#include"PODSolution.h"
#include"PPPSolverLEO.h"
#include"PPPSolverLEOFwBw.h"

#include"XYZ2NEU.hpp"
#include"RequireObservables.hpp"
//
#include"SimpleFilter.hpp"
// Class to detect cycle slips using LI combination
#include "LICSDetector2.hpp"

// Class to detect cycle slips using the Melbourne-Wubbena combination
#include "MWCSDetector.hpp"

// Class to compute the effect of solid tides
#include "SolidTides.hpp"

// Class to compute the effect of ocean loading
#include "OceanLoading.hpp"

// Class to compute the effect of pole tides
#include "PoleTides.hpp"

// Class to correct observables
#include "CorrectObservables.hpp"

// Class to compute the effect of wind-up
#include "ComputeWindUp.hpp"

// Class to compute the effect of satellite antenna phase center
#include "ComputeSatPCenter.hpp"

// Class to compute the tropospheric data
#include "ComputeTropModel.hpp"

// Class to compute linear combinations
#include "ComputeLinear.hpp"

// This class pre-defines several handy linear combinations
#include "LinearCombinations.hpp"

// Class to compute Dilution Of Precision values
#include "ComputeDOP.hpp"

// Class to keep track of satellite arcs
#include "SatArcMarker.hpp"

// Class to compute gravitational delays
#include "GravitationalDelay.hpp"

// Class to align phases with code measurements
#include "PhaseCodeAlignment.hpp"

// Compute statistical data
#include "PowerSum.hpp"

// Used to delete satellites in eclipse
#include "EclipsedSatFilter.hpp"

// Used to decimate data. This is important because RINEX observation
// data is provided with a 30 s sample rate, whereas SP3 files provide
// satellite clock information with a 900 s sample rate.
#include "Decimate.hpp"

#include "ConfDataReader.hpp"
#include"BasicModel.hpp"

using namespace gpstk;
namespace pod
{
    PODSolution::PODSolution(GnssDataStore_sptr confData):
        PPPSolutionBase (confData)
    {
       solverPR  = std::unique_ptr<CodeSolverBase>( new CodeSolverLEO(data));
    }

    bool PODSolution::processCore()
    {
        int outInt(confReader().getValueAsInt("outputInterval"));

        updateRequaredObs();

        // This object will check that code observations are within
        // reasonable limits
        SimpleFilter PRFilter;
        PRFilter.addFilteredType(TypeID::P1);
        PRFilter.setFilteredType(TypeID::P2);

        SimpleFilter SNRFilter(TypeID::S1, (double)opts().maskSNR, 1e7);

        // This object defines several handy linear combinations
        LinearCombinations comb;

        // Object to compute linear combinations for cycle slip detection
        ComputeLinear linear1;

        linear1.addLinear(comb.pdeltaCombination);
        linear1.addLinear(comb.mwubbenaCombination);

        linear1.addLinear(comb.ldeltaCombination);
        linear1.addLinear(comb.liCombination);

        // Objects to mark cycle slips
        LICSDetector2 markCSLI2;         // Checks LI cycle slips
                                         // markCSLI2.setSatThreshold(1);
       // Checks Merbourne-Wubbena cycle slips
        MWCSDetector  markCSMW(confReader().getValueAsDouble("MWNumLambdas"));          

        // Object to keep track of satellite arcs
        SatArcMarker markArc;
        markArc.setDeleteUnstableSats(true);
        markArc.setUnstablePeriod(61.0);

        // Object to decimate data
        double newSampling(confReader().getValueAsDouble("decimationInterval"));

        Decimate decimateData(
            newSampling,
            confReader().getValueAsDouble("decimationTolerance"),
            data->SP3EphList.getInitialTime());

        // Declare a basic modeler
        //BasicModel basic(Position(0.0, 0.0, 0.0), SP3EphList);
        BasicModel basic(nominalPos, data->SP3EphList);
        // Set the minimum elevation
        basic.setMinElev(opts().maskEl);

        basic.setDefaultObservable(TypeID::P1);

        // Object to remove eclipsed satellites
        EclipsedSatFilter eclipsedSV;

        // Object to compute gravitational delay effects
        GravitationalDelay grDelay(nominalPos);

        // Vector from monument to antenna ARP [UEN], in meters
        Triple offsetARP;
        int i = 0;
        for (auto &it : confReader().getListValueAsDouble("offsetARP"))
            offsetARP[i++] = it;

        // Declare an object to correct observables to monument
        CorrectObservables corr(data->SP3EphList);
        corr.setMonument(offsetARP);

        // Feed Antex reader object with Antex file
        AntexReader antexReader;
        std::string afile = opts().genericFilesDirectory;
        afile += confReader().getValue("antexFile");
        antexReader.open(afile);

        // Object to compute satellite antenna phase center effect
        ComputeSatPCenter svPcenter(nominalPos);
        // Feed 'ComputeSatPCenter' object with 'AntexReader' object
        svPcenter.setAntexReader(antexReader);

#pragma region receiver antenna parameters

        bool useAntex(confReader().getValueAsBoolean("useRcvAntennaModel"));
        if (useAntex)
        {
            Antenna receiverAntenna;
            // Get receiver antenna parameters
            std::string aModel(confReader().getValue("antennaModel", opts().SiteRover));
            receiverAntenna = antexReader.getAntenna(aModel);

            // Check if we want to use Antex patterns
            bool usepatterns(confReader().getValueAsBoolean("usePCPatterns", opts().SiteRover));
            if (usepatterns)
            {
                corr.setAntenna(receiverAntenna);
                // Should we use elevation/azimuth patterns or just elevation?
                corr.setUseAzimuth(confReader().getValueAsBoolean("useAzim", opts().SiteRover));
            }
        }
        else
        {
            Triple ofstL1(0.0, 0.0, 0.0), ofstL2(0.0, 0.0, 0.0); 
            int i = 0;
            for (auto& it: confReader().getListValueAsDouble("offsetL1"))
                ofstL1[i++] = it;
            i = 0;
            for (auto& it : confReader().getListValueAsDouble("offsetL2"))
                ofstL2[i++] = it;

            corr.setL1pc(ofstL1);
            corr.setL2pc(ofstL2);
        }

#pragma endregion

        // Object to compute wind-up effect
        ComputeWindUp windup(data->SP3EphList, nominalPos, opts().genericFilesDirectory + confReader().getValue("satDataFile"));

        // Object to compute ionosphere-free combinations to be used
        // as observables in the PPP processing
        ComputeLinear linear2;
        linear2.addLinear(comb.pcCombination);
        linear2.addLinear(comb.lcCombination);

        // Add to processing list
        // Declare a simple filter object to screen PC
        SimpleFilter pcFilter;
        pcFilter.setFilteredType(TypeID::PC);

        // IMPORTANT NOTE:
        // Like in the "filterCode" case, the "filterPC" option allows you to
        // deactivate the "SimpleFilter" object that filters out PC, in case
        // you need to.

        // Object to align phase with code measurements
        PhaseCodeAlignment phaseAlign;

        // Object to compute prefit-residuals
        ComputeLinear linear3(comb.pcPrefit);
        linear3.addLinear(comb.lcPrefit);

        // Object to compute DOP values
        ComputeDOP cDOP;

        // White noise stochastic model
        WhiteNoiseModel wnM(1000.0);      // 100 m of sigma
                                          // Declare solver objects
        PPPSolverLEO   pppSolver(false);
        pppSolver.setCoordinatesModel(&wnM);
        PPPSolverLEOFwBw fbpppSolver(false);
        fbpppSolver.setCoordinatesModel(&wnM);

        // Get if we want 'forwards-backwards' or 'forwards' processing only
        int cycles(confReader().getValueAsInt("forwardBackwardCycles"));

        std::list<double> phaseLimits, codeLimits;
        for (double val: confReader().getListValueAsDouble("codeLimits"))
        if(val != 0.0) codeLimits.push_back(val);

        for (double val : confReader().getListValueAsDouble("phaseLimits"))
        if (val != 0.0) phaseLimits.push_back(val);

        fbpppSolver.setPhaseList(phaseLimits);
        fbpppSolver.setCodeList(codeLimits);

        // This is the GNSS data structure that will hold all the
        // GNSS-related information
        RinexEpoch gRin;

#pragma region Output streams

        // Prepare for printing
        int precision(4);

        std::ofstream outfile;
        outfile.open(opts().workingDir +"\\"+fileName(), std::ios::out);

#pragma endregion

        //statistics for coorinates and tropo delay
        std::vector<PowerSum> stats(4);
        CommonTime time0;
        bool b = true;

        //// *** Now comes the REAL forwards processing part *** ////
        for (auto obsFile : data->getObsFiles(opts().SiteRover))
        {

            std::cout << obsFile << std::endl;
            //Input observation file stream
            Rinex3ObsStream rin;
            // Open Rinex observations file in read-only mode
            rin.open(obsFile, std::ios::in);

            rin.exceptions(std::ios::failbit);
            Rinex3ObsHeader roh;
            Rinex3ObsData rod;

          
            //read the header
            rin >> roh;
            gMap.header = roh;

            // Loop over all data epochs
            while (rin >> gRin)
            {

                gRin.keepOnlySatSystems(opts().systems);

                PPPSolutionBase::mapSNR(gRin);

                // Store current epoch
                CommonTime time(gRin.getHeader().epoch);
				if (apprPos().getPosition(gRin, nominalPos))
					continue;

                ///update the nominal position in processing objects
                XYZ2NEU baseChange(nominalPos);
                basic.rxPos = nominalPos;
                grDelay.setNominalPosition(nominalPos);
                svPcenter.setNominalPosition(nominalPos);
                windup.setNominalPosition(nominalPos);
                corr.setNominalPosition(nominalPos);

                int csnum(0);
                try
                {
                    //  std::cout <<(YDSTime)time<< " "<< gRin.numSats();
                    gRin >> requireObs
                        >> PRFilter
                        >> SNRFilter
                        >> linear1;
                    //  gRin >> markCSLI2;

                    gRin >> markCSMW;
                    //csnum = getNumCS(gRin);

                    gRin >> markArc;
                    //std::cout  <<" "<<csnum<<  " " << gRin.numSats();
                    gRin >> decimateData
                        >> basic
                        >> eclipsedSV
                        >> grDelay
                        >> svPcenter;
                    //std::cout <<  " " << gRin.numSats() << " ";
                    gRin >> requireObs
                        >> corr
                        >> windup
                        >> linear2
                        >> pcFilter
                        >> phaseAlign
                        >> linear3
                        >> baseChange
                        >> cDOP;

                    if (cycles < 1)
                        gRin >> pppSolver;
                    else
                        gRin >> fbpppSolver;
                    
                    //   std::cout /*<<  " " << gRin.numSats()*/ << std::endl;
                }
                catch (DecimateEpoch& d)
                {
                    continue;
                }
                catch (Exception& e)
                {
                    std::cout << e.getText() << std::endl;
                    return false;
                }
                catch (...)
                {
                    return false;
                }

                // Check what type of solver we are using
                if (cycles < 1)
                {
                    GnssEpoch ep(gRin.getBody());
                    CommonTime time(gRin.getHeader().epoch);
                    if (b)
                    {
                        time0 = time;
                        b = false;
                    }

                    // This is a 'forwards-only' filter. Let's print to output
                    // file the results of this epoch
                    double fm = fmod(((GPSWeekSecond)time).getSOW(), outInt);

                    if (fm < 0.1)
                        pppSolver.printSolution(outfile, time0, time, cDOP, ep,  0.0, stats, nominalPos);
                    gMap.data.insert(std::pair<CommonTime, GnssEpoch>(time, ep));
                }  // End of 'if ( cycles < 1 )'
             
            }  // End of 'while(rin >> gRin)'

            rin.close();
        }

        //// *** Forwards processing part is over *** ////

        // Now decide what to do: If solver was a 'forwards-only' version,
        // then we are done and should continue with next station.
        if (cycles < 1)
        {

            // Close output file for this station
            outfile.close();

            return true;
        }

        //// *** If we got here, it is a 'forwards-backwards' solver *** ////
        int i_c = 0;
        // Now, let's do 'forwards-backwards' cycles
        try
        {
            std::cout << "cycle # " << ++i_c;
            if (codeLimits.size() > 0 || phaseLimits.size() > 0)
                fbpppSolver.ReProcess();
            else
                fbpppSolver.ReProcess(cycles);

            std::cout << " rej. meas: " << fbpppSolver.getRejectedMeasurements() << std::endl;
        }
        catch (Exception& e)
        {
            // Close output file for this station
            outfile.close();

            return false;
        }

        // Reprocess is over. Let's finish with the last processing		
        // Loop over all data epochs, again, and print results
    
        while (fbpppSolver.LastProcess(gRin))
        {
            GnssEpoch ep(gRin.getBody());
            CommonTime time(gRin.getHeader().epoch);

            if (b)
            {
                time0 = time;
                b = false;
            }
			if (apprPos().getPosition(gRin, nominalPos))
				continue;
            double fm = fmod(((GPSWeekSecond)time).getSOW(), outInt);
            if (fm < 0.1)
                fbpppSolver.printSolution(outfile, time0, time, cDOP, ep, 0.0, stats, nominalPos);
            //add epoch to results
            gMap.data.insert(std::pair<CommonTime, GnssEpoch>(time, ep));
        }  // End of 'while( fbpppSolver.LastProcess(gRin) )'

        // Close output file for this station
        outfile.close();
        
        return true;
    }

    double PODSolution::mapSNR(double value)
    {
       return 20.0*log10(value);
    }
    void PODSolution::updateRequaredObs()
    {
        requireObs.addRequiredType(TypeID::P1);
        requireObs.addRequiredType(TypeID::P2);
        requireObs.addRequiredType(TypeID::L1);
        requireObs.addRequiredType(TypeID::L2);
        requireObs.addRequiredType(TypeID::S1);
    }
    void PODSolution::printSolution(std::ofstream& outfile, const SolverLMS& solver, const CommonTime& time, GnssEpoch& gEpoch)
    {
        // Prepare for printing
        outfile << std::fixed << std::setprecision(outputPrec);

        // Print results
        outfile << static_cast<YDSTime>(time).year << "-";   // Year           - #1
        outfile << static_cast<YDSTime>(time).doy << "-";    // DayOfYear      - #2
        outfile << static_cast<YDSTime>(time).sod << "  ";   // SecondsOfDay   - #3
        outfile << std::setprecision(6) << (static_cast<YDSTime>(time).doy + static_cast<YDSTime>(time).sod / 86400.0) << "  " << std::setprecision(outputPrec);

        double x = nominalPos.X() + solver.getSolution(TypeID::dx);    // dx    - #4
        double y = nominalPos.Y() + solver.getSolution(TypeID::dy);    // dy    - #5
        double z = nominalPos.Z() + solver.getSolution(TypeID::dz);    // dz    - #6

        gEpoch.slnData.insert(std::pair<TypeID, double>(TypeID::recX, x));
        gEpoch.slnData.insert(std::pair<TypeID, double>(TypeID::recY, y));
        gEpoch.slnData.insert(std::pair<TypeID, double>(TypeID::recZ, z));

        double varX = solver.getVariance(TypeID::dx);     // Cov dx    - #8
        double varY = solver.getVariance(TypeID::dy);     // Cov dy    - #9
        double varZ = solver.getVariance(TypeID::dz);     // Cov dz    - #10
        double sigma = sqrt(varX + varY + varZ);

        double cdt = solver.getSolution(TypeID::cdt);
        gEpoch.slnData.insert(std::pair<TypeID, double>(TypeID::recCdt, cdt));

        //
        outfile << x << "  " << y << "  " << z << "  " << cdt << " ";

        auto defeq = solver.getDefaultEqDefinition();

        auto itcdtGLO = defeq.body.find(TypeID::recISB_GLN);
        if (defeq.body.find(TypeID::recISB_GLN) != defeq.body.end())
        {
            double cdtGLO = solver.getSolution(TypeID::recISB_GLN);
            gEpoch.slnData.insert(std::pair<TypeID, double>(TypeID::recISB_GLN, cdtGLO));

            outfile << cdtGLO << " ";
        }

        if (defeq.body.find(TypeID::recCdtdot) != defeq.body.end())
        {
            double recCdtdot = solver.getSolution(TypeID::recISB_GLN);
            gEpoch.slnData.insert(std::pair<TypeID, double>(TypeID::recISB_GLN, recCdtdot));

            outfile << std::setprecision(12) << recCdtdot << " ";
        }

        gEpoch.slnData.insert(std::pair<TypeID, double>(TypeID::sigma, sigma));
        outfile << std::setprecision(4) << sigma << "  ";

        gEpoch.slnData.insert(std::pair<TypeID, double>(TypeID::recSlnType, 16));

        outfile << gEpoch.satData.size() << std::endl;    // Number of satellites - #12

    }
  
    //
    void PODSolution::process()
    {
        //if (opts().isComputeApprPos)
        //{
        //    PRProcess();
        //}
        //else
        //{
        //    std::cout << "Approximate Positions loading from \n" + opts().workingDir + "\\" + data->apprPosFile + "\n... ";

        //    std::cout << "\nComplete." << std::endl;
        //}
        try
        {
            processCore();

            gMap.updateMetadata();
        }
        catch (ConfigurationException &conf_exp)
        {
			std::cerr << conf_exp.what() << std::endl;
            throw;
        }
        catch (Exception &gpstk_e)
        {
            GPSTK_RETHROW(gpstk_e);
        }
        catch (std::exception &std_e)
        {
			std::cerr << std_e.what() << std::endl;
            throw;
        }
    }
}