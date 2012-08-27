//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef C_RECIPIENTFILTER_H
#define C_RECIPIENTFILTER_H
#ifdef _WIN32
#pragma once
#endif

#include "irecipientfilter.h"
#include "utlvector.h"
#include "c_baseentity.h"
#include "soundflags.h"

class C_BasePlayer;
class C_Team;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_RecipientFilter : public IRecipientFilter
{
public:
	enum
	{
		NORMAL = 0,
		ENTITY,
	};

					C_RecipientFilter();
	virtual			~C_RecipientFilter();

	virtual bool	IsReliable( void ) const;

	virtual int		GetRecipientCount( void ) const;
	virtual int		GetRecipientIndex( int slot ) const;

	virtual bool	IsEntityMessage( void ) const;
	virtual const char *GetEntityMessageName( void ) const;

	virtual int		GetEntityIndex( void ) const;

	virtual bool	IsBroadcastMessage( void );

	virtual bool	IsInitMessage( void ) const;

public:

	void			Reset( void );

	void			MakeInitMessage( void );

	void			MakeReliable( void );
	void			SetEntityMessage( int entity, char const *msgname );
	
	void			AddAllPlayers( void );
	void			AddRecipientsByPVS( const Vector& origin );
	void			AddRecipientsByPAS( const Vector& origin );
	void			AddRecipient( C_BasePlayer *player );
	void			RemoveRecipient( C_BasePlayer *player );
	void			AddRecipientsByTeam( C_Team *team );
	void			RemoveRecipientsByTeam( C_Team *team );

	void			UsePredictionRules( void );
	bool			IsUsingPredictionRules( void ) const;

	bool			IgnorePredictionCull( void ) const;
	void			SetIgnorePredictionCull( bool ignore );

private:
	void			EnsureTruePlayerCount( void );
	void			AddPlayersFromBitMask( unsigned int playerbits );

	bool				m_bReliable;
	bool				m_bInitMessage;
	CUtlVector< int >	m_Recipients;
	int					m_Type;
	int					m_nEntityIndex;
	char				*m_pEntityMessageName;
	// i.e., being sent to all players
	int					m_nTruePlayerCount;
	// If using prediction rules, the filter itself suppresses local player
	bool				m_bUsingPredictionRules;
	// If ignoring prediction cull, then external systems can determine
	//  whether this is a special case where culling should not occur
	bool				m_bIgnorePredictionCull;
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for a single player
//-----------------------------------------------------------------------------
class CSingleUserRecipientFilter : public C_RecipientFilter
{
public:
	CSingleUserRecipientFilter( C_BasePlayer *player )
	{
		AddRecipient( player );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players, unreliable
//-----------------------------------------------------------------------------
class CBroadcastRecipientFilter : public C_RecipientFilter
{
public:
	CBroadcastRecipientFilter( void )
	{
		AddAllPlayers();
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players, reliable
//-----------------------------------------------------------------------------
class CRelieableBroadcastRecipientFilter : public CBroadcastRecipientFilter
{
public:
	CRelieableBroadcastRecipientFilter( void )
	{
		MakeReliable();
	}
};

class CEntityMessageFilter : public C_RecipientFilter
{
public:
	CEntityMessageFilter( C_BaseEntity *entity, const char *msgname )
	{
		Assert( msgname );
		AddAllPlayers();
		SetEntityMessage( entity->index, msgname );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for a single player
//-----------------------------------------------------------------------------
class CPASFilter : public C_RecipientFilter
{
public:
	CPASFilter( const Vector& origin )
	{
		AddRecipientsByPAS( origin );
	}
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPASAttenuationFilter : public CPASFilter
{
public:
	CPASAttenuationFilter( C_BaseEntity *entity, float attenuation = ATTN_NORM ) :
		CPASFilter( entity->GetAbsOrigin() )
	{
	}

	CPASAttenuationFilter( const Vector& origin, float attenuation = ATTN_NORM ) :
		CPASFilter( origin )
	{
	}

	CPASAttenuationFilter( C_BaseEntity *entity, const char *lookupSound ) :
		CPASFilter( entity->GetAbsOrigin() )
	{
	}

	CPASAttenuationFilter( const Vector& origin, const char *lookupSound ) :
		CPASFilter( origin )
	{
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for a single player
//-----------------------------------------------------------------------------
class CPVSFilter : public C_RecipientFilter
{
public:
	CPVSFilter( const Vector& origin )
	{
		AddRecipientsByPVS( origin );
	}
};

class CLocalPlayerFilter : public C_RecipientFilter
{
public:
	CLocalPlayerFilter( void );
};

#endif // C_RECIPIENTFILTER_H
