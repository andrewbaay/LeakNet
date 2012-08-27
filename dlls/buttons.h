//====== Copyright � 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef BUTTONS_H
#define BUTTONS_H
#ifdef _WIN32
#pragma once
#endif


class CBaseButton : public CBaseToggle
{
public:
	DECLARE_CLASS( CBaseButton, CBaseToggle );

	void Spawn( void );
	virtual void Precache( void );
	bool CreateVPhysics();
	void RotSpawn( void );
	bool KeyValue( const char *szKeyName, const char *szValue );

	void ButtonActivate( );
	void SparkSoundCache( void );

	void ButtonTouch( ::CBaseEntity *pOther );
	void ButtonSpark ( void );
	void TriggerAndWait( void );
	void ButtonReturn( void );
	void ButtonBackHome( void );
	void ButtonUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual void Lock();
	virtual void Unlock();

	// Input handlers
	void InputLock( inputdata_t &inputdata );
	void InputUnlock( inputdata_t &inputdata );
	void InputPress( inputdata_t &inputdata );

	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	
	enum BUTTON_CODE { BUTTON_NOTHING, BUTTON_ACTIVATE, BUTTON_RETURN };
	BUTTON_CODE	ButtonResponseToTouch( void );
	
	bool	ShouldSavePhysics()	{ return false; }
	
	DECLARE_DATADESC();

	virtual int	ObjectCaps(void);

	Vector m_vecMoveDir;

	bool	m_fStayPushed;		// button stays pushed in until touched again?
	bool	m_fRotating;		// a rotating button?  default is a sliding button.

	locksound_t m_ls;			// door lock sounds
	
	byte	m_bLockedSound;		// ordinals from entity selection
	byte	m_bLockedSentence;	
	byte	m_bUnlockedSound;	
	byte	m_bUnlockedSentence;
	bool	m_bLocked;
	int		m_sounds;

	string_t	m_sNoise;			// The actual WAV file name of the sound.

	COutputEvent m_OnDamaged;
	COutputEvent m_OnPressed;
	COutputEvent m_OnIn;
	COutputEvent m_OnOut;

	int		m_nState;
};


//
// Rotating button (aka "lever")
//
class CRotButton : public CBaseButton
{
public:
	DECLARE_CLASS( CRotButton, CBaseButton );

	void Spawn( void );
	bool CreateVPhysics( void );

};


class CMomentaryRotButton : public CRotButton
{
	DECLARE_CLASS( CMomentaryRotButton, CRotButton );

public:
	void	Spawn ( void );
	bool	CreateVPhysics( void );
	virtual int	ObjectCaps( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	UseMoveDone( void );
	void	ReturnMoveDone( void );
	void	OutputMovementComplete(void);
	void	SetPositionMoveDone(void);
	void	UpdateSelf( float value, bool bPlaySound );

	void	PlaySound( void );
	void	UpdateTarget( float value, CBaseEntity *pActivator );

	int		DrawDebugTextOverlays(void);

	static CMomentaryRotButton *Instance( edict_t *pent ) { return (CMomentaryRotButton *)GetContainingEntity(pent); }

	float GetPos(const QAngle &vecAngles);

	DECLARE_DATADESC();

	virtual void Lock();
	virtual void Unlock();

	// Input handlers
	void InputSetPosition( inputdata_t &inputdata );

	COutputFloat m_Position;
	COutputEvent m_OnPressed;
	COutputEvent m_OnUnpressed;
	COutputEvent m_OnFullyOpen;
	COutputEvent m_OnFullyClosed;

	int			m_lastUsed;
	QAngle		m_start;
	QAngle		m_end;
	float		m_IdealYaw;
	float		m_flTimeDelta;			// Think interval while handling a SetPosition input.
	string_t	m_sNoise;

	int			m_direction;
	float		m_returnSpeed;
	int			m_sounds;
	float		m_flStartPosition;

protected:

	void UpdateThink( void );
};


#endif // BUTTONS_H
