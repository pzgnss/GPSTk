#pragma once
#include"DataStructures.hpp"
#include"DataHeaders.hpp"
#include"SatTypePtrMap.h"
#include<memory>

namespace gpstk
{
	class IRinex;
	typedef SatID::SatelliteSystem GpstkSatSystem;
	typedef std::unique_ptr<IRinex> irinex_uptr;
    class IRinex
    {
    public:
		friend std::istream&  operator>>(std::istream& i, IRinex& f)
		{
			return f.read(i);
		}
		
		friend std::ostream&  operator<<(std::ostream& i, IRinex& f)
		{
			return f.print(i);
		}
		virtual IRinex& operator=(const IRinex & other) { return *this; };

		virtual irinex_uptr clone() const =0;
		
		virtual std::istream& read(std::istream&) = 0;
		virtual std::ostream& print(std::ostream&) = 0;
        
		virtual sourceEpochRinexHeader& getHeader() = 0;
        virtual const sourceEpochRinexHeader& getHeader() const = 0;

        virtual SatTypePtrMap& getBody() = 0;
		virtual const SatTypePtrMap& getBody() const = 0;
		virtual void addSv(const SatID& sv, const typeValueMap& data) = 0;
		virtual  void resetCurrData() = 0;
    };

    class RinexEpoch : public IRinex
    {

    public:

        RinexEpoch( ); 
		RinexEpoch(const RinexEpoch & other);
        RinexEpoch(const gnssRinex & gRin);

        virtual ~RinexEpoch();

        void resetCurrData();

        SatTypePtrMap& getBody()
        {
            return currData;
        };

		virtual IRinex& operator=(const RinexEpoch & other);

		virtual IRinex& operator=(const IRinex & other) override;
		
		virtual irinex_uptr clone() const override
		{
			return std::make_unique<RinexEpoch>(*this);
		}

        const SatTypePtrMap& getBody() const
        {
            return currData;
        };

        sourceEpochRinexHeader& getHeader()
        {
            return rinex.header;
        }

		const sourceEpochRinexHeader& getHeader() const 
        {
            return rinex.header;
        }

		std::istream& read(std::istream& i);


		std::ostream& print(std::ostream& i)
		{
			return i << rinex;
		}
		void addSv(const SatID& sv, const typeValueMap& data) override;

        RinexEpoch extractSatID(const SatID& satellite) const;

        RinexEpoch extractSatID(const int& p, const GpstkSatSystem& s) const;

        RinexEpoch extractSatID(const SatIDSet& satSet) const;

        RinexEpoch extractSatSyst(const SatSystSet& satSet) const;

        RinexEpoch& keepOnlySatID(const SatID& satellite);

        RinexEpoch& keepOnlySatID(const int& p, const GpstkSatSystem& s);

        RinexEpoch& keepOnlySatID(const SatIDSet& satSet);

        RinexEpoch& keepOnlySatSystems(GpstkSatSystem satSyst);

        RinexEpoch& keepOnlySatSystems(const SatSystSet& satSet);

        RinexEpoch extractTypeID(const TypeID& type) const;

        RinexEpoch extractTypeID(const TypeIDSet& typeSet) const;

        RinexEpoch& keepOnlyTypeID(const TypeID& type);

        RinexEpoch& keepOnlyTypeID(const TypeIDSet& typeSet);


		RinexEpoch& removeSatID(int id, SatID::SatelliteSystem system);

		RinexEpoch& removeSatID(const SatID & satSet);

		RinexEpoch& removeSatID(const SatIDSet& satSet);

		//RinexEpoch& removeSatSyst(const SatSystSet& satSet);

		//RinexEpoch& removeSatSyst(SatID::SatelliteSystem syst);

    protected:
        SatTypePtrMap currData;

        gnssRinex rinex;

    };
}


