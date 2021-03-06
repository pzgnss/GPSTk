// This is a SWIG macro that creates helper methods and the read/write methods:
// It creates:
//
//  C++ methods: (all of these are hidden to the end user since the streams are hidden)
//   - static factory method for the stream to create an input stream
//   - static factory method for the stream to create an output stream
//   - readHeader method for the stream
//   - readData method for the stream
//   - writeHeader method for the stream
//   - writeData method for the stream
//   - _remove method to delete the object (called from python helpers below)
//
//  gpstk python functions:
//   - readX, where X is the file type
//   - writeX, where X is the file type
//

%define STREAM_HELPER(FORMATNAME)
%extend gpstk:: ## FORMATNAME ## Stream { 

   // methods for the stream itself:
   static gpstk:: ## FORMATNAME ## Stream* in ## FORMATNAME ## Stream(const std::string fileName) {
      FORMATNAME ## Stream * s = new FORMATNAME ## Stream (fileName.c_str());
      return s;
   }

   static gpstk:: ## FORMATNAME ## Stream* out ## FORMATNAME ## Stream(const std::string fileName) {
      FORMATNAME ## Stream * s = new FORMATNAME ## Stream (fileName.c_str(), std::ios::out|std::ios::trunc);
      return s;
   }

   static void _remove(gpstk:: ## FORMATNAME ## Stream * ptr) {
      ptr->close();
      delete ptr;
   }

   // reader functions:
   gpstk:: ## FORMATNAME ## Header readHeader() {
      gpstk:: ##FORMATNAME ## Header head;
      (*($self)) >> head;
      return head;
   }

   gpstk:: ## FORMATNAME ## Data readData() {
      gpstk:: ## FORMATNAME ##Data data;
      if( (*($self)) >> data ) {
         return data;
      } else {
         gpstk::EndOfFile e(" FORMATNAME ## Stream reached an EOF.");
         GPSTK_THROW(e);
      }
   }

   // write functions:
   void writeHeader(const gpstk:: ## FORMATNAME ## Header & head) {
      (*($self)) << head;
   }

   void writeData(const gpstk:: ## FORMATNAME ## Data & data) {
      (*($self)) << data;
   }
   
    bool  read ## FORMATNAME ## Data (const std::string & fileName, 
                                       gpstk:: ## FORMATNAME ## Header & header,
                                       gpstk:: ## FORMATNAME ## Data & data)
   {
       
	   try
        {
            gpstk::## FORMATNAME ## Stream stream(fileName.c_str());
            stream >> header;
            while (stream >> data)
            {
            }
        }
         catch (gpstk::EndOfFile& e)
         {
             
         }
        catch(...)
        {
            throw std::exception(("Cannot read from file " + fileName).c_str());
        } 
        return true;

    }
}
%enddef

STREAM_HELPER(Rinex3Clock)
STREAM_HELPER(Rinex3Nav)
STREAM_HELPER(Rinex3Obs)
STREAM_HELPER(RinexMet)
STREAM_HELPER(RinexNav)
STREAM_HELPER(RinexObs)
STREAM_HELPER(SEM)
STREAM_HELPER(SP3)
STREAM_HELPER(Yuma)


/* class FORMATNAME ## Dataset(list): */
/*     def __init__(self, times, typecodes, data=None): */
/*         list.__init__(self,data) */

/*         self.startTime = times[0] */
/*         self.__header = FORMATNAME ## Header() */
/*         for gnssSys in typecodes: */
/*             self.__header.mapObsTypes[gnssSys] = [FORMATNAME ## ID("%s" % tc) for tc in typecodes[gnssSys])] */
        
/*         for i,t in enumerate(times): */
/*             datum = FORMATNAME ## Data() */
/*             datum.time = t */
/*             datum.epochFlag = 0 */
/*             datum.clockOffset = 0. */
/*             datum.numSVs = 0 */
/*             self.append(datum) */

/*     def writeEpoch(self,satID,index,typecode,data,ssi=0,lli=0): */
/*         sid = FORMATNAME ## SatID(satID) */
/*         if sid not in self[index].obs: self[index].obs = [] */
/*         smap = self[index].obs[sid] */
/*         rd = vrd[self.__header.getObsIndex(typecode)] */
/*         rd.data, rd.ssi, rd.lli = data, ssi, lli */

%inline %{
    void writeEpochs(std::vector<gpstk::Rinex3ObsData>& rodarr
                     , const gpstk::Rinex3ObsHeader& roh
                     , const gpstk::RinexSatID& svid
                     , const std::vector<double>& data
                     , const std::vector<int>& where
                     , const int rotidx) {

      int i = 0;
      // gpstk::RinexDatum rd;
      for(std::vector<int>::const_iterator it=where.begin(); it != where.end(); ++it, ++i) {

        // check if there is any sv entries at this epoch and create one if necessary
        if (rodarr[*it].obs.find(svid) == rodarr[*it].obs.end()) {
          std::map<std::string , std::vector<RinexObsID> >::const_iterator kt = roh.mapObsTypes.find("G");
          if (kt == roh.mapObsTypes.end()) { return; }
          rodarr[*it].obs[svid] = gpstk::Rinex3ObsData::DataMap::mapped_type (kt->second.size());
          rodarr[*it].numSVs++;
        }
        
        // rd.data = data[i];
        rodarr[*it].obs[svid][rotidx].data = data[i];
      }
    }
%}


