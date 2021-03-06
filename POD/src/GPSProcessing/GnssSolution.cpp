#include "GnssSolution.h"
#include"PRSolution2.hpp"
#include"Bancroft.hpp"
#include"PowerSum.hpp"
#include"KalmanSolver.h"

using namespace gpstk;

namespace pod
{
     std::ostream& GnssSolution:: printMsg(const gpstk::CommonTime& time, const char* msg)
     {
        return(std::cout << "Epoch: " << CivilTime(time).asString() << " "<< msg << std::endl);
     }
   
     GnssSolution::GnssSolution(GnssDataStore_sptr gnssData, double sigma = 50.0)
         :data(gnssData), Equations(std::make_shared<EquationComposer>()), maxSigma(sigma)
     {
     }

    GnssSolution::~GnssSolution() {}

	void GnssSolution::printSolution(const KalmanSolver& solver, const gpstk::CommonTime& time, GnssEpoch& gEpoch)
	{

		for (auto && it : Equations->currentUnknowns())
		{
			if (it.type == TypeID::dx || it.type == TypeID::dy || it.type == TypeID::dz)
				continue;

			if (it.sv == SatID::dummy)
			{
				if (it.type == TypeID::cdt)
					gEpoch.slnData[TypeID::recCdt] = solver.getSolution(it);
				else if (it.type == TypeID::wetMap)
					gEpoch.slnData[TypeID::recZTropo] = solver.getSolution(it);
				else if (it.type == TypeID::wetMapNorth)
					gEpoch.slnData[TypeID::recTropoNorth] = solver.getSolution(it);
				else if (it.type == TypeID::wetMapEast)
					gEpoch.slnData[TypeID::recTropoEast] = solver.getSolution(it);
				else
					gEpoch.slnData[it.type] = solver.getSolution(it);
			}
			else
			{
				double amb = solver.getSolution(it);
				if (amb != 0)
					gEpoch.satData[it.sv][it.type] = amb;
			}
				
		}
		Position newPos;
		double stDev3D(NAN);

		newPos[0] = nominalPos.X() + solver.getSolution(FilterParameter(TypeID::dx));    // dx    - #4
		newPos[1] = nominalPos.Y() + solver.getSolution(FilterParameter(TypeID::dy));    // dy    - #5
		newPos[2] = nominalPos.Z() + solver.getSolution(FilterParameter(TypeID::dz));    // dz    - #6

		double varX = solver.getVariance(FilterParameter(TypeID::dx));     // Cov dx    - #8
		double varY = solver.getVariance(FilterParameter(TypeID::dy));     // Cov dy    - #9
		double varZ = solver.getVariance(FilterParameter(TypeID::dz));     // Cov dz    - #10
		stDev3D = sqrt(varX + varY + varZ);

		gEpoch.slnData.insert(std::make_pair(TypeID::recX, newPos.X()));
		gEpoch.slnData.insert(std::make_pair(TypeID::recY, newPos.Y()));
		gEpoch.slnData.insert(std::make_pair(TypeID::recZ, newPos.Z()));
		gEpoch.slnData.insert(std::make_pair(TypeID::recStDev3D, stDev3D));

		//number of used sats = number of residuals/number of measurement types
		int numUsedSats = solver.PostfitResiduals().size() / Equations->measTypes().size();
		gEpoch.slnData.insert(std::make_pair(TypeID::recUsedSV, numUsedSats));

		SlnType slnType = solver.getValid() ? desiredSlnType() : SlnType::NONE_SOLUTION;

		gEpoch.slnData.insert(std::make_pair(TypeID::recSlnType, slnType));

		gEpoch.slnData.insert(std::make_pair(TypeID::sigma, solver.getPhaseSigma()));

	};
}
