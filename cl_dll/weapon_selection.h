//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Weapon selection handling
//
// $NoKeywords: $
//=============================================================================
#if !defined( WEAPON_SELECTION_H )
#define WEAPON_SELECTION_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"

class C_BaseCombatWeapon;
class C_BasePlayer;

//-----------------------------------------------------------------------------
// Purpose: Base class for tf2 & hl2 weapon selection hud elements
//-----------------------------------------------------------------------------
class CBaseHudWeaponSelection : public CHudElement
{
	DECLARE_CLASS( CBaseHudWeaponSelection, CHudElement );

public:
	CBaseHudWeaponSelection( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void ProcessInput();
	virtual void Reset(void);
	virtual void OnThink(void);

	virtual void OpenSelection( void );
	virtual void HideSelection( void );

	virtual void				CancelWeaponSelection( void );

	// Game specific overrides
	virtual void CycleToNextWeapon( void ) = 0;
	virtual void CycleToPrevWeapon( void ) = 0;
	virtual void SwitchToLastWeapon( void );
	virtual C_BaseCombatWeapon *GetWeaponInSlot( int iSlot, int iSlotPos ) = 0;
	virtual void SelectWeaponSlot( int iSlot ) = 0;
	virtual C_BaseCombatWeapon	*GetFirstPos( int iSlot );
	virtual C_BaseCombatWeapon	*GetNextActivePos( int iSlot, int iSlotPos );
	virtual void				SetWeaponSelected( void );
	virtual void				SelectWeapon( void );

	virtual C_BaseCombatWeapon	*GetSelectedWeapon( void ) = 0;

	virtual void OnWeaponPickup( C_BaseCombatWeapon *pWeapon );
	virtual bool IsInSelectionMode();

	void _cdecl UserCmd_Slot1( void );
	void _cdecl UserCmd_Slot2( void );
	void _cdecl UserCmd_Slot3( void );
	void _cdecl UserCmd_Slot4( void );
	void _cdecl UserCmd_Slot5( void );
	void _cdecl UserCmd_Slot6( void );
	void _cdecl UserCmd_Slot7( void );
	void _cdecl UserCmd_Slot8( void );
	void _cdecl UserCmd_Slot9( void );
	void _cdecl UserCmd_Slot0( void );
	void _cdecl UserCmd_Close( void );
	void _cdecl UserCmd_NextWeapon( void );
	void _cdecl UserCmd_PrevWeapon( void );
	void _cdecl UserCmd_LastWeapon( void );
	void _cdecl UserCmd_DropPrimary( void );

	virtual void		SelectSlot( int iSlot );

	virtual bool ShouldDraw();

	static CBaseHudWeaponSelection *GetInstance();

	// these functions are exposed as virtual so that the tf_hints system can redraw the weapon selection
	virtual void DrawWList( C_BasePlayer *pPlayer, C_BaseCombatWeapon *pSelectedWeapon, bool drawOutline = false, int or = 0, int og = 0, int ob = 0, int oa = 0 ) {}
	virtual bool ComputeRect( C_BasePlayer *pPlayer, C_BaseCombatWeapon *pSelectedWeapon, wrect_t *outrect ) { return false; }
	
	virtual int	KeyInput( int down, int keynum, const char *pszCurrentBinding ) { return 1; }

protected:
	static CBaseHudWeaponSelection *s_pInstance;

	bool	m_bSelectionVisible;

	CHandle< C_BaseCombatWeapon >	m_hSelectedWeapon;
};

// accessor
CBaseHudWeaponSelection *GetHudWeaponSelection();

#endif // WEAPON_SELECTION_H