#include "RinexEpoch.h"
using namespace std;
namespace gpstk
{

    RinexEpoch::
        RinexEpoch()
    { }

	RinexEpoch::
		RinexEpoch(const RinexEpoch & other)
		:rinex(other.rinex)
	{
		resetCurrData();
	}
	
    RinexEpoch::
        RinexEpoch(const gpstk::gnssRinex & gRin)
        :rinex(gRin)
    {
		resetCurrData();
	}

    RinexEpoch::
        ~RinexEpoch()
    { }

	IRinex& RinexEpoch::
		operator=(const RinexEpoch & other)
	{
		this->rinex = other.rinex;
		resetCurrData();

		return	*this;
	}

	IRinex& RinexEpoch::
		operator=(const IRinex & other)
	{
		const RinexEpoch& ep = dynamic_cast<const RinexEpoch&>(other);

		this->rinex = ep.rinex;
		resetCurrData();

		return	*this;
	}
	void RinexEpoch:: 
		addSv(const SatID& sv, const typeValueMap& data)
	{
		rinex.body.emplace(sv, data);
	}

    void RinexEpoch::
        resetCurrData()
    {
        currData.clear();
        for (auto && it : rinex.body)
            currData.emplace(it.first, make_unique<TypeValueMapPtr>(it.second));
    }

	std::istream& RinexEpoch::
		read(std::istream& i)
	{
		i >> rinex;
		resetCurrData();
		return i;
	}

    RinexEpoch RinexEpoch::
        extractSatID(const gpstk::SatID& satellite) const
    {
        return RinexEpoch(rinex.extractSatID(satellite));
    }

    RinexEpoch RinexEpoch::
        extractSatID(const int& p, const GpstkSatSystem& s) const
    {
        return RinexEpoch(rinex.extractSatID(p,s));
    }

    RinexEpoch RinexEpoch::
        extractSatID(const gpstk::SatIDSet& satSet) const
    {
        return RinexEpoch(rinex.extractSatID(satSet));
    }

    RinexEpoch RinexEpoch::
        extractSatSyst(const gpstk::SatSystSet& satSet) const
    {
        return RinexEpoch(rinex.extractSatSyst(satSet));
    }

    RinexEpoch& RinexEpoch::
        keepOnlySatID(const gpstk::SatID& sv)
    {
		return keepOnlySatID(SatIDSet{ sv });
    }

    RinexEpoch& RinexEpoch::
		keepOnlySatID(const int& p, const GpstkSatSystem& s)
	{
		return keepOnlySatID(SatID(p, s));
	}

    RinexEpoch& RinexEpoch::
        keepOnlySatID(const gpstk::SatIDSet& satSet)
    {
        rinex.keepOnlySatID(satSet);
		resetCurrData();
        return *this;
    }

    RinexEpoch& RinexEpoch::
        keepOnlySatSystems(GpstkSatSystem satSyst)
    {
		SatSystSet sss{ satSyst };
        return keepOnlySatSystems(sss);
    }

    RinexEpoch& RinexEpoch::
        keepOnlySatSystems(const gpstk::SatSystSet& satSet)
    {
        rinex.keepOnlySatSystems(satSet);
		resetCurrData();
        return *this;
    }

    RinexEpoch RinexEpoch::
        extractTypeID(const gpstk::TypeID& type) const
    {
        return RinexEpoch(rinex.extractTypeID(type));
    }

    RinexEpoch RinexEpoch::
        extractTypeID(const gpstk::TypeIDSet& typeSet) const
    {
        return RinexEpoch(rinex.extractTypeID(typeSet));
    }

    RinexEpoch& RinexEpoch::
        keepOnlyTypeID(const gpstk::TypeID& type)
    {
		return keepOnlyTypeID(TypeIDSet{ type });
    }

    RinexEpoch& RinexEpoch::
        keepOnlyTypeID(const gpstk::TypeIDSet& typeSet)
    {
        rinex.keepOnlyTypeID(typeSet);
		resetCurrData();
        return *this;
    }

	RinexEpoch& RinexEpoch::
		removeSatID(int id, SatID::SatelliteSystem system)
	{
		SatID sv(id, system);
		(*this).rinex.removeSatID(sv);

		return (*this);
	}

	RinexEpoch& RinexEpoch::
		removeSatID(const SatIDSet& satSet)
	{
		(*this).rinex.removeSatID(satSet);

		return (*this);
	}

	RinexEpoch& RinexEpoch::
		removeSatID(const SatID & sv)
	{
		(*this).rinex.removeSatID(sv);

		return (*this);
	}
}