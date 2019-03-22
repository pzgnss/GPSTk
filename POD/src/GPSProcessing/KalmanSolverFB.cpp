#include "KalmanSolverFB.h"
#include"PowerSum.hpp"
#include"WinUtils.h"

using namespace gpstk;

namespace pod
{

    KalmanSolverFB::KalmanSolverFB()
        :currCycle(0), processedMeasurements(0), rejectedMeasurements(0)
    {}

    KalmanSolverFB::KalmanSolverFB(eqComposer_sptr eqs)
        : currCycle(0), processedMeasurements(0), rejectedMeasurements(0)
    {
        solver = KalmanSolver(eqs);
    }

    KalmanSolverFB::~KalmanSolverFB()
    {}

    gpstk::IRinex & KalmanSolverFB::Process(gpstk::IRinex & gRin)
    {
        solver.Process(gRin);

        // Before returning, store the results for a future iteration
        if (currCycle==0)
        {
            // Create a new gnssRinex structure with just the data we need
            //gnssRinex gBak(gData.extractTypeID(keepTypeSet));

            // Store observation data
            ObsData.push_back(gRin.clone());

            // Update the number of processed measurements
            processedMeasurements += gRin.getBody().numSats();
        }

        return gRin;
    }


    bool KalmanSolverFB::lastProcess(gpstk::IRinex & gRin)
    {

        // Keep processing while 'ObsData' is not empty
        if (!(ObsData.empty()))
        {
            // Get the first data epoch in 'ObsData' and process it. 
            // The result will be stored in 'gData'
			gRin = ReProcessOneEpoch(*ObsData.front());
            // gData = ObsData.front();
            // Remove the first data epoch in 'ObsData', freeing some
            // memory and preparing for next epoch
            ObsData.pop_front();
            return true;
        }
        else
        {
            return false;
        }
    }

	void KalmanSolverFB::reProcess()
	{
		// Backwards iteration. We must do this at least once
		for (auto rpos = ObsData.rbegin(); rpos != ObsData.rend(); ++rpos)
			ReProcessOneEpoch(**rpos);

		for ( currCycle = 0; currCycle < cyclesNumber - 1; ++currCycle)
		{
			for (auto &it : ObsData)
			{
				
				ReProcessOneEpoch(*it);
			}

			for (auto rpos = ObsData.rbegin(); rpos != ObsData.rend(); ++rpos)
				ReProcessOneEpoch(**rpos);
		}
	}

	gpstk::IRinex & KalmanSolverFB::ReProcessOneEpoch(gpstk::IRinex & gRin)
	{
		gRin.resetCurrData();
		usedSvMarker.keepOnlyUsed(gRin.getBody());
		usedSvMarker.CleanSatArcFlags(gRin.getBody());
		usedSvMarker.CleanScFlags(gRin.getBody());
		checkLimits(gRin, currCycle);
		//usedSvMarker.updateLastEpoch(gRin);

		gRin >> reProcList;
		gRin.getBody().removeTypeID(eqComposer().residTypes());
		return	solver.Process(gRin);
	}

	KalmanSolverFB& KalmanSolverFB::setLimits(const std::list<double>& codeLims, const std::list<double>& phaseLims)
    {
        size_t i = 0;
        tresholds.codeLimits.resize(codeLims.size());
        for (auto val : codeLims)
            tresholds.codeLimits[i] = val;

        i = 0;
        tresholds.phaseLimits.resize(phaseLims.size());
        for (auto val : phaseLims)
            tresholds.phaseLimits[i] = val;

        return *this;
    }

    double KalmanSolverFB::getLimit(const gpstk::TypeID& type, size_t cycleNumber)
    {
        if (codeResTypes.find(type) != codeResTypes.end())
            if (cycleNumber < tresholds.codeLimits.size())
                return tresholds.codeLimits[cycleNumber];
        if (phaseResTypes.find(type) != phaseResTypes.end())
            if (cycleNumber < tresholds.phaseLimits.size())
                return tresholds.phaseLimits[cycleNumber];

		std::string msg = "Can't get observables treshold for type: '"
            + TypeID::tStrings[type.type] +
            "' with reprocess cycle number: '"
            + StringUtils::asString(cycleNumber) + "'.";

        InvalidRequest e(msg);
        GPSTK_THROW(e);

    }

    void KalmanSolverFB::checkLimits(IRinex& gData, size_t cycleNumber)
    {
        // Set to store rejected satellites
        SatIDSet satRejectedSet;

        // Let's check limits
        for (auto&& type : solver.eqComposer().residTypes())
        {
            double limit = getLimit(type, cycleNumber);
			for (auto&& it : gData.getBody())
			{
				// Check postfit values and mark satellites as rejected
				auto itRes = it.second->get_value().find(type);
				if (itRes != it.second->get_value().end() && std::abs(itRes->second) > limit)
				{
					it.second->get_value().erase(type);
					satRejectedSet.insert(it.first);
				}
			}
        }

        // Update the number of rejected measurements
        rejectedMeasurements += satRejectedSet.size();

        // Remove satellites with missing data
        gData.getBody().removeSatID(satRejectedSet);
		
    }
}
