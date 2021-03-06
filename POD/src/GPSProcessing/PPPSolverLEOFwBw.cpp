#include "PPPSolverLEOFwBw.h"
namespace pod
{
    // Returns a string identifying this object.
    std::string PPPSolverLEOFwBw::getClassName() const
    {
        return "PPPSolverLEOFwBw";
    }


    /* Common constructor.
    *
    * @param useNEU   If true, will compute dLat, dLon, dH coordinates;
    *                 if false (the default), will compute dx, dy, dz.
    */
    PPPSolverLEOFwBw::PPPSolverLEOFwBw(bool useNEU)
        : firstIteration(true)
    {

        // Initialize the counter of processed measurements
        processedMeasurements = 0;

        // Initialize the counter of rejected measurements
        rejectedMeasurements = 0;

        // Set the equation system structure
        PPPSolverLEO::setNEU(useNEU);

        // Indicate the TypeID's that we want to keep
        keepTypeSet.insert(TypeID::wetMap);

        if (useNEU)
        {
            keepTypeSet.insert(TypeID::dLat);
            keepTypeSet.insert(TypeID::dLon);
            keepTypeSet.insert(TypeID::dH);
        }
        else
        {
            keepTypeSet.insert(TypeID::dx);
            keepTypeSet.insert(TypeID::dy);
            keepTypeSet.insert(TypeID::dz);
        }

        keepTypeSet.insert(TypeID::cdt);
        keepTypeSet.insert(TypeID::prefitC);
        keepTypeSet.insert(TypeID::prefitL);
        keepTypeSet.insert(TypeID::weight);
        keepTypeSet.insert(TypeID::CSL1);
        keepTypeSet.insert(TypeID::satArc);


    }  // End of 'PPPSolverLEOFwBw::PPPSolverLEOFwBw()'



       /* Returns a reference to a gnnsRinex object after solving
       * the previously defined equation system.
       *
       * @param gData     Data object holding the data.
       */
	IRinex& PPPSolverLEOFwBw::Process(IRinex& gData)
        throw(ProcessingException)
    {

        try
        {

            PPPSolverLEO::Process(gData);


            // Before returning, store the results for a future iteration
            if (firstIteration)
            {

                // Create a new gnssRinex structure with just the data we need
                //gnssRinex gBak(gData.extractTypeID(keepTypeSet));

                // Store observation data
                ObsData.push_back(gData.clone());

                // Update the number of processed measurements
                processedMeasurements += gData.getBody().numSats();

            }

            return gData;

        }
        catch (Exception& u)
        {
            // Throw an exception if something unexpected happens
            ProcessingException e(getClassName() + ":"
                                  + u.what());

            GPSTK_THROW(e);

        }

    }  // End of method 'PPPSolverLEOFwBw::Process()'



       /* Reprocess the data stored during a previous 'Process()' call.
       *
       * @param cycles     Number of forward-backward cycles, 1 by default.
       *
       * \warning The minimum number of cycles allowed is "1". In fact, if
       * you introduce a smaller number, 'cycles' will be set to "1".
       */
    void PPPSolverLEOFwBw::ReProcess(int cycles)
        throw(ProcessingException)
    {

        // Check number of cycles. The minimum allowed is "1".
        if (cycles < 1)
        {
            cycles = 1;
        }

        // This will prevent further storage of input data when calling
        // method 'Process()'
        firstIteration = false;

        try
        {
            // Backwards iteration. We must do this at least once
            for (auto rpos = ObsData.rbegin(); rpos != ObsData.rend(); ++rpos)
            {

                PPPSolverLEO::Process(**rpos);

            }

            // If 'cycles > 1', let's do the other iterations
            for (int i = 0; i < (cycles - 1); i++)
            {

                // Forwards iteration
                for (auto pos = ObsData.begin(); pos != ObsData.end(); ++pos)
                {
                    PPPSolverLEO::Process(**pos);
                }

                // Backwards iteration.
                for (auto rpos = ObsData.rbegin(); rpos != ObsData.rend(); ++rpos)
                {
                    PPPSolverLEO::Process(**rpos);
                }

            }  // End of 'for (int i=0; i<(cycles-1), i++)'

            return;

        }
        catch (Exception& u)
        {
            // Throw an exception if something unexpected happens
            ProcessingException e(getClassName() + ":"
                                  + u.what());

            GPSTK_THROW(e);

        }

    }  // End of method 'PPPSolverLEOFwBw::ReProcess()'



       /* Reprocess the data stored during a previous 'Process()' call.
       *
       * This method will reprocess data trimming satellites whose postfit
       * residual is bigger than the limits indicated by limitsCodeList and
       * limitsPhaseList.
       */
    void PPPSolverLEOFwBw::ReProcess(void)
        throw(ProcessingException)
    {

        // Let's use a copy of the lists
        std::list<double> codeList(limitsCodeList);
        std::list<double> phaseList(limitsPhaseList);

        // Get maximum size
        size_t maxSize(codeList.size());
        if (maxSize < phaseList.size()) maxSize = phaseList.size();

        // This will prevent further storage of input data when calling
        // method 'Process()'
        firstIteration = false;

        try
        {


            // Backwards iteration. We must do this at least once
            for (auto rpos = ObsData.rbegin(); rpos != ObsData.rend(); ++rpos)
            {

                PPPSolverLEO::Process(**rpos);

            }

            // If both sizes are '0', let's return
            if (maxSize == 0)
            {
                return;
            }

            // We will store the limits here. By default we use very big values
            double codeLimit(1000000.0);
            double phaseLimit(1000000.0);

            // If 'maxSize > 0', let's do the other iterations
            for (size_t i = 0; i < maxSize; i++)
            {

                // Update current limits, if available
                if (codeList.size() > 0)
                {
                    // Get the first element from the list
                    codeLimit = codeList.front();

                    // Delete the first element from the list
                    codeList.pop_front();
                }

                if (phaseList.size() > 0)
                {
                    // Get the first element from the list
                    phaseLimit = phaseList.front();

                    // Delete the first element from the list
                    phaseList.pop_front();
                }


                // Forwards iteration
                for (auto pos = ObsData.begin(); pos != ObsData.end(); ++pos)
                {
                    // Let's check limits
					checkLimits(**pos, codeLimit, phaseLimit);

                    // Process data
                    PPPSolverLEO::Process( **pos );
                }

                // Backwards iteration.
                for (auto rpos = ObsData.rbegin(); rpos != ObsData.rend(); ++rpos)
                {
                    // Let's check limits
					checkLimits(**rpos, codeLimit, phaseLimit);

                    // Process data
					PPPSolverLEO::Process(**rpos);
                }

            }  // End of 'for (int i=0; i<(cycles-1), i++)'

            return;

        }
        catch (Exception& u)
        {
            // Throw an exception if something unexpected happens
            ProcessingException e(getClassName() + ":"
                                  + u.what());

            GPSTK_THROW(e);

        }

    }  // End of method 'PPPSolverLEOFwBw::ReProcess()'



       /* Process the data stored during a previous 'ReProcess()' call, one
       * item at a time, and always in forward mode.
       *
       * @param gData      Data object that will hold the resulting data.
       *
       * @return FALSE when all data is processed, TRUE otherwise.
       */
    bool PPPSolverLEOFwBw::LastProcess(IRinex& gData)
        throw(ProcessingException)
    {

        try
        {
            // Keep processing while 'ObsData' is not empty
            if (!(ObsData.empty()))
            {

                // Get the first data epoch in 'ObsData' and process it. The
                // result will be stored in 'gData'
                gData = PPPSolverLEO::Process(*ObsData.front());

                // Remove the first data epoch in 'ObsData', freeing some
                // memory and preparing for next epoch
                ObsData.pop_front();


                // Update some inherited fields
                solution = PPPSolverLEO::solution;
                covMatrix = PPPSolverLEO::covMatrix;
                postfitResiduals = PPPSolverLEO::postfitResiduals;

                // If everything is fine so far, then results should be valid
                valid = true;

                return true;

            }
            else
            {

                // There are no more data
                return false;

            }  // End of 'if( !(ObsData.empty()) )'

        }
        catch (Exception& u)
        {
            // Throw an exception if something unexpected happens
            ProcessingException e(getClassName() + ":"
                                  + u.what());

            GPSTK_THROW(e);

        }

    }  // End of method 'PPPSolverLEOFwBw::LastProcess()'



       // This method checks the limits and modifies 'gData' accordingly.
    void PPPSolverLEOFwBw::checkLimits(IRinex& gData,
                                       double codeLimit,
                                       double phaseLimit)
    {

        // Set to store rejected satellites
        SatIDSet satRejectedSet;

        // Let's check limits
        for (auto && it: gData.getBody())
        {

            // Check postfit values and mark satellites as rejected
            if (std::abs(it.second->get_value()(TypeID::postfitC)) > codeLimit)
                satRejectedSet.insert(it.first);
            

            if (std::abs(it.second->get_value()(TypeID::postfitL)) > phaseLimit)
                satRejectedSet.insert(it.first);

        }  // End of 'for( satTypeValueMap::iterator it = gds.body.begin();...'


           // Update the number of rejected measurements
        rejectedMeasurements += satRejectedSet.size();

        // Remove satellites with missing data
        gData.getBody().removeSatID(satRejectedSet);

        return;

    }  // End of method 'PPPSolverLEOFwBw::checkLimits()'



       /* Sets if a NEU system will be used.
       *
       * @param useNEU  Boolean value indicating if a NEU system will
       *                be used
       *
       */
    PPPSolverLEOFwBw& PPPSolverLEOFwBw::setNEU(bool useNEU)
    {

        // Set the PPPSolverLEO filter
        PPPSolverLEO::setNEU(useNEU);


        // Clear current 'keepTypeSet' and indicate the TypeID's that
        // we want to keep
        keepTypeSet.clear();

        keepTypeSet.insert(TypeID::wetMap);

        if (useNEU)
        {
            keepTypeSet.insert(TypeID::dLat);
            keepTypeSet.insert(TypeID::dLon);
            keepTypeSet.insert(TypeID::dH);
        }
        else
        {
            keepTypeSet.insert(TypeID::dx);
            keepTypeSet.insert(TypeID::dy);
            keepTypeSet.insert(TypeID::dz);
        }

        keepTypeSet.insert(TypeID::cdt);
        keepTypeSet.insert(TypeID::prefitC);
        keepTypeSet.insert(TypeID::prefitL);
        keepTypeSet.insert(TypeID::weight);
        keepTypeSet.insert(TypeID::CSL1);
        keepTypeSet.insert(TypeID::satArc);


        // Return this object
        return (*this);

    }  // End of method 'PPPSolverLEOFwBw::setNEU()'
}
