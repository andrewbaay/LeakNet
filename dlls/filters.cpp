//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "filters.h"
#include "entitylist.h"

// ###################################################################
//	> BaseFilter
// ###################################################################
LINK_ENTITY_TO_CLASS(filter_base, CBaseFilter);

BEGIN_DATADESC( CBaseFilter )

	DEFINE_KEYFIELD(CBaseFilter, m_bNegated, FIELD_BOOLEAN, "Negated"),

	// Inputs
	DEFINE_INPUTFUNC( CBaseFilter, FIELD_INPUT, "TestActivator", InputTestActivator ),

	// Outputs
	DEFINE_OUTPUT( CBaseFilter, m_OnPass, "OnPass"),
	DEFINE_OUTPUT( CBaseFilter, m_OnFail, "OnFail"),

END_DATADESC()

//-----------------------------------------------------------------------------

bool CBaseFilter::PassesFilterImpl(CBaseEntity *pEntity)
{
	return true;
}


bool CBaseFilter::PassesFilter(CBaseEntity *pEntity)
{
	bool baseResult = PassesFilterImpl(pEntity);
	return (m_bNegated) ? !baseResult : baseResult;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for testing the activator. If the activator passes the
//			filter test, the OnPass output is fired. If not, the OnFail output is fired.
//-----------------------------------------------------------------------------
void CBaseFilter::InputTestActivator( inputdata_t &inputdata )
{
	if (PassesFilter( inputdata.pActivator ))
	{
		m_OnPass.FireOutput( inputdata.pActivator, this );
	}
	else
	{
		m_OnFail.FireOutput( inputdata.pActivator, this );
	}
}


// ###################################################################
//	> FilterMultiple
//
//   Allows one to filter through mutiple filters
// ###################################################################
#define MAX_FILTERS 5
enum filter_t
{
	FILTER_AND,
	FILTER_OR,
};

class CFilterMultiple : public CBaseFilter
{
	DECLARE_CLASS( CFilterMultiple, CBaseFilter );
	DECLARE_DATADESC();

	filter_t	m_nFilterType;
	string_t	m_iFilterName[MAX_FILTERS];
	EHANDLE		m_hFilter[MAX_FILTERS];

	bool PassesFilterImpl(CBaseEntity *pEntity);
	void Activate(void);
};

LINK_ENTITY_TO_CLASS(filter_multi, CFilterMultiple);

BEGIN_DATADESC( CFilterMultiple )


	// Keys
	DEFINE_KEYFIELD(CFilterMultiple, m_nFilterType, FIELD_INTEGER, "FilterType"),

	// Silence, Classcheck!
//	DEFINE_ARRAY( CFilterMultiple, m_iFilterName, FIELD_STRING, MAX_FILTERS ),

	DEFINE_KEYFIELD(CFilterMultiple, m_iFilterName[0], FIELD_STRING, "Filter01"),
	DEFINE_KEYFIELD(CFilterMultiple, m_iFilterName[1], FIELD_STRING, "Filter02"),
	DEFINE_KEYFIELD(CFilterMultiple, m_iFilterName[2], FIELD_STRING, "Filter03"),
	DEFINE_KEYFIELD(CFilterMultiple, m_iFilterName[3], FIELD_STRING, "Filter04"),
	DEFINE_KEYFIELD(CFilterMultiple, m_iFilterName[4], FIELD_STRING, "Filter05"),
	DEFINE_ARRAY( CFilterMultiple, m_hFilter, FIELD_EHANDLE, MAX_FILTERS ),

END_DATADESC()



//------------------------------------------------------------------------------
// Purpose : Called after all entities have been loaded
//------------------------------------------------------------------------------
void CFilterMultiple::Activate( void )
{
	BaseClass::Activate();
	
	// Get handles to my filter entities
	for (int i=0;i<MAX_FILTERS;i++)
	{
		if (m_iFilterName[i] != NULL_STRING)
		{
			m_hFilter[i] = gEntList.FindEntityByName( NULL, m_iFilterName[i], NULL );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the entity passes our filter, false if not.
// Input  : pEntity - Entity to test.
//-----------------------------------------------------------------------------
bool CFilterMultiple::PassesFilterImpl(CBaseEntity *pEntity)
{
	// Test against each filter
	if (m_nFilterType == FILTER_AND)
	{
		for (int i=0;i<MAX_FILTERS;i++)
		{
			if (m_hFilter[i] != NULL)
			{
				CBaseFilter* pFilter = (CBaseFilter*)(m_hFilter[i].Get());
				if (!pFilter->PassesFilter(pEntity))
				{
					return false;
				}
			}
		}
		return true;
	}
	else  // m_nFilterType == FILTER_OR
	{
		for (int i=0;i<MAX_FILTERS;i++)
		{
			if (m_hFilter[i] != NULL)
			{
				CBaseFilter* pFilter = (CBaseFilter*)(m_hFilter[i].Get());
				if (pFilter->PassesFilter(pEntity))
				{
					return true;
				}
			}
		}
		return false;
	}
}


// ###################################################################
//	> FilterName
// ###################################################################
class CFilterName : public CBaseFilter
{
	DECLARE_CLASS( CFilterName, CBaseFilter );
	DECLARE_DATADESC();

public:
	string_t m_iFilterName;

	bool PassesFilterImpl(CBaseEntity *pEntity)
	{
		// special check for !player as GetEntityName for player won't return "!player" as a name
		if(FStrEq(STRING(m_iFilterName), "!player"))
		{
			if(pEntity->IsPlayer())
			{
				CBasePlayer *pPlayer = ToBasePlayer( pEntity );
				// if player is in a vehicle, don't report back we're the player
				if ( pPlayer->IsInAVehicle() )
					return false;
				else
					return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return pEntity->NameMatches( STRING(m_iFilterName) );
		}
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_name, CFilterName );

BEGIN_DATADESC( CFilterName )

	// Keyfields
	DEFINE_KEYFIELD( CFilterName, m_iFilterName,	FIELD_STRING,	"filtername" ),

END_DATADESC()



// ###################################################################
//	> FilterClass
// ###################################################################
class CFilterClass : public CBaseFilter
{
	DECLARE_CLASS( CFilterClass, CBaseFilter );
	DECLARE_DATADESC();

public:
	string_t m_iFilterClass;

	bool PassesFilterImpl(CBaseEntity *pEntity)
	{
		return pEntity->ClassMatches( STRING(m_iFilterClass) );
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_class, CFilterClass );

BEGIN_DATADESC( CFilterClass )

	// Keyfields
	DEFINE_KEYFIELD( CFilterClass, m_iFilterClass,	FIELD_STRING,	"filterclass" ),

END_DATADESC()


// ###################################################################
//	> FilterTeam
// ###################################################################
class FilterTeam : public CBaseFilter
{
	DECLARE_CLASS( FilterTeam, CBaseFilter );
	DECLARE_DATADESC();

public:
	int		m_iFilterTeam;

	bool PassesFilterImpl(CBaseEntity *pEntity)
	{
	 	return ( pEntity->GetTeamNumber() == m_iFilterTeam );
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_team, FilterTeam );

BEGIN_DATADESC( FilterTeam )

	// Keyfields
	DEFINE_KEYFIELD( FilterTeam, m_iFilterTeam,	FIELD_INTEGER,	"filterteam" ),

END_DATADESC()

