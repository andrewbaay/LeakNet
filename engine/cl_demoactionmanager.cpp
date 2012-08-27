//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "glquake.h"
#include <vgui/VGUI.h>
#include <vgui_controls/Controls.h>
#include "cl_demoaction.h"
#include "cl_demoactionmanager.h"
#include "cl_demouipanel.h"

#include <KeyValues.h>
#include "UtlBuffer.h"
#include "filesystem_engine.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDemoActionManager : public IDemoActionManager
{
public:

	CDemoActionManager();
	~CDemoActionManager();

// Public interface
public:

	virtual void		Init( void );
	virtual void		Shutdown( void );

	virtual void		StartPlaying( char const *demfilename );
	virtual void		StopPlaying();

	virtual void		Update( bool newframe, int demoframe, float demotime );

	virtual void		SaveToBuffer( CUtlBuffer& buf );
	virtual void		SaveToFile( void );

	virtual void		InstallDemoUI( vgui::Panel *parent );
	virtual char const	*GetCurrentDemoFile( void );

	virtual int			GetActionCount( void );
	virtual CBaseDemoAction *GetAction( int index );
	virtual void		AddAction( CBaseDemoAction *action );
	virtual void		RemoveAction( CBaseDemoAction *action );

	virtual bool		IsDirty( void ) const;
	virtual void		SetDirty( bool dirty );

	virtual void		ReloadFromDisk( void );

	virtual float		GetPlaybackScale();

	virtual void		DispatchEvents();
	virtual void		InsertFireEvent( CBaseDemoAction *action );

	virtual bool		OverrideView( democmdinfo_t& info, int frame, float elapsed );
	virtual void		DrawDebuggingInfo( int frame, float elapsed );
private:
	void				OnVDMLoaded( char const *demfilename );
	void				ClearAll();

	CUtlVector< CBaseDemoAction * >	m_ActionStack;
	CUtlVector< CBaseDemoAction * >	m_PendingFireActionStack;


	int					m_nPrevFrame;
	float				m_flPrevTime;


	bool				m_bDirty;
	char				m_szCurrentFile[ MAX_OSPATH ];
	long				m_lFileTime;
};

static CDemoActionManager g_DemoActionManager;
IDemoActionManager *demoaction = ( IDemoActionManager * )&g_DemoActionManager;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDemoActionManager::CDemoActionManager()
{
	m_nPrevFrame = 0;
	m_flPrevTime = 0.0f;
	m_szCurrentFile[ 0 ] = 0;
	m_bDirty = false;
	m_lFileTime = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDemoActionManager::~CDemoActionManager()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoActionManager::Init( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoActionManager::Shutdown( void )
{
	StopPlaying();
	ClearAll();
	m_ActionStack.Purge();
	m_PendingFireActionStack.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Reload without saving
//-----------------------------------------------------------------------------
void CDemoActionManager::ReloadFromDisk( void )
{
	char metafile[ 512 ];
	COM_StripExtension( m_szCurrentFile, metafile, sizeof( metafile ) );
	COM_DefaultExtension( metafile, ".vdm", sizeof( metafile ) );

	ClearAll();

	//const char *buffer = NULL;
	//int sz = 0;
	//buffer = (const char *)COM_LoadFile( metafile, 5, &sz );
//	if ( buffer )
//	{
		m_lFileTime = g_pFileSystem->GetFileTime( metafile );

		KeyValues *kv = new KeyValues( metafile );
		Assert( kv );
		if ( kv )
		{
			if ( kv->LoadFromFile( filesystem(), metafile ) )
			{
				// Iterate over all metaclasses...
				KeyValues* pIter = kv->GetFirstSubKey();
				while( pIter )
				{
					char factorytouse[ 512 ];
					
					strcpy( factorytouse, pIter->GetName() );

					// New format is to put numbers in here
					if ( atoi( factorytouse ) > 0 )
					{
						strcpy( factorytouse, pIter->GetString( "factory", "" ) );
					}

					CBaseDemoAction *action = CBaseDemoAction::CreateDemoAction( CBaseDemoAction::TypeForName( factorytouse ) );
					if ( action )
					{
						if ( !action->Init( pIter ) )
						{
							delete action;
						}
						else
						{
							m_ActionStack.AddToTail( action );
						}
					}

					pIter = pIter->GetNextKey();
				}
			}
			else
			{
				SaveToFile();
			}
		}

	//	COM_FreeFile( (byte *)buffer );
//	}
//	else
//	{
		// This will save out an empty .vdm of the proper name
//		SaveToFile();
//	}

	OnVDMLoaded( m_szCurrentFile );

	m_bDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *demfilename - 
//-----------------------------------------------------------------------------
void CDemoActionManager::StartPlaying( char const *demfilename )
{
	Assert( demfilename );

	// Clear anything currently pending
	StopPlaying();

	Q_strncpy( m_szCurrentFile, demfilename, sizeof( m_szCurrentFile ) );

	char metafile[ 512 ];
	COM_StripExtension( demfilename, metafile, sizeof( metafile ) );
	COM_DefaultExtension( metafile, ".vdm", sizeof( metafile ) );

	long filetime = g_pFileSystem->GetFileTime( metafile );

	// Same file?
	if ( !Q_strcasecmp( demfilename, m_szCurrentFile ) &&
		m_lFileTime == filetime )
	{
		return;
	}

	if ( m_bDirty )
	{
		SaveToFile();
	}

	ReloadFromDisk();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoActionManager::ClearAll()
{
	m_PendingFireActionStack.RemoveAll();

	while ( m_ActionStack.Count() > 0 )
	{
		CBaseDemoAction *a = m_ActionStack[ 0 ];
		delete a;
		m_ActionStack.Remove( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoActionManager::StopPlaying()
{
	int count = m_ActionStack.Count();
	for ( int i = 0; i < count; i++ )
	{
		CBaseDemoAction *a = m_ActionStack[ i ];
		a->Reset();
	}

	// Reset counters
	m_nPrevFrame = 0;
	m_flPrevTime = 0.0f;

	m_PendingFireActionStack.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : demoframe - 
//			demotime - 
//-----------------------------------------------------------------------------
void CDemoActionManager::Update(  bool newframe, int demoframe, float demotime )
{
	// Nothing to do?
	int count = m_ActionStack.Count();
	if ( count <= 0 )
		return;

	// Setup timing context
	DemoActionTimingContext ctx;
	ctx.prevframe = m_nPrevFrame;
	ctx.curframe = demoframe;
	ctx.prevtime = m_flPrevTime;
	ctx.curtime = demotime;

	int i;
	for ( i = 0; i < count; i++ )
	{
		CBaseDemoAction *action = m_ActionStack[ i ];
		Assert( action );
		if ( !action )
			continue;

		action->Update( ctx );
	}

	m_nPrevFrame = demoframe;
	m_flPrevTime = demotime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//-----------------------------------------------------------------------------
void CDemoActionManager::SaveToBuffer( CUtlBuffer& buf )
{
	buf.Printf( "demoactions\n" );
	buf.Printf( "{\n" );

	int count = m_ActionStack.Count();
	int i;
	for ( i = 0; i < count; i++ )
	{
		CBaseDemoAction *action = m_ActionStack[ i ];
		Assert( action );
		if ( !action )
			continue;

		action->SaveToBuffer( 1, i + 1, buf );
	}

	buf.Printf( "}\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *demfilename - 
//-----------------------------------------------------------------------------
void CDemoActionManager::SaveToFile( void )
{
	// Nothing loaded
	if ( m_szCurrentFile[ 0 ] == 0 )
		return;

	// It's not dirty
	if ( !m_bDirty )
		return;

	char metafile[ 512 ];
	COM_StripExtension( m_szCurrentFile, metafile, sizeof( metafile ) );
	COM_DefaultExtension( metafile, ".vdm", sizeof( metafile ) );

	// Save data
	CUtlBuffer buf( 0, 0, true );
	SaveToBuffer( buf );

	// Write to file
	FileHandle_t fh;
	fh = g_pFileSystem->Open( metafile, "w" );
	if ( fh != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Write( buf.Base(), buf.TellPut(), fh );
		g_pFileSystem->Close( fh );
	}

	m_bDirty = false;

	// Make sure filetime is up to date
	m_lFileTime = g_pFileSystem->GetFileTime( metafile );
}

static CDemoUIPanel *g_pDemoUI = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *demfilename - 
//-----------------------------------------------------------------------------
void CDemoActionManager::OnVDMLoaded( char const *demfilename )
{
	// Notify UI?
	if ( !g_pDemoUI )
		return;

	g_pDemoUI->OnVDMChanged();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
void CDemoActionManager::InstallDemoUI( vgui::Panel *parent )
{
	if ( g_pDemoUI )
		return;

	g_pDemoUI = new CDemoUIPanel( parent );
	Assert( g_pDemoUI );
}

char const *CDemoActionManager::GetCurrentDemoFile( void )
{
	return m_szCurrentFile;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CDemoActionManager::GetActionCount( void )
{
	int count = m_ActionStack.Count();
	return count;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : CBaseDemoAction
//-----------------------------------------------------------------------------
CBaseDemoAction *CDemoActionManager::GetAction( int index )
{
	int count = m_ActionStack.Count();
	if ( index < 0 || index >= count )
		return NULL;

	return m_ActionStack[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *action - 
//-----------------------------------------------------------------------------
void CDemoActionManager::AddAction( CBaseDemoAction *action )
{
	m_bDirty = true;
	Assert( action );
	m_ActionStack.AddToTail( action );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *action - 
//-----------------------------------------------------------------------------
void CDemoActionManager::RemoveAction( CBaseDemoAction *action )
{
	Assert( action );
	m_bDirty = true;

	m_ActionStack.FindAndRemove( action );
	delete action;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoActionManager::IsDirty( void ) const
{
	return m_bDirty;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dirty - 
//-----------------------------------------------------------------------------
void CDemoActionManager::SetDirty( bool dirty )
{
	m_bDirty = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CDemoActionManager::GetPlaybackScale()
{
	if ( !g_pDemoUI )
		return 1.0f;

	return g_pDemoUI->GetPlaybackScale();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *action - 
//-----------------------------------------------------------------------------
void CDemoActionManager::InsertFireEvent( CBaseDemoAction *action )
{
	m_PendingFireActionStack.AddToTail( action );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoActionManager::DispatchEvents()
{
	int c = m_PendingFireActionStack.Count();
	int i;
	for ( i = 0; i < c; i++ )
	{
		CBaseDemoAction *action = m_PendingFireActionStack[ i ];
		Assert( action );
		action->FireAction();
		action->SetActionFired( true );
	}

	m_PendingFireActionStack.RemoveAll();
}

bool CDemoActionManager::OverrideView( democmdinfo_t& info, int frame, float elapsed )
{
	if ( !g_pDemoUI )
		return false;

	return g_pDemoUI->OverrideView( info, frame, elapsed );
}

void CDemoActionManager::DrawDebuggingInfo( int frame, float elapsed )
{
	if ( !g_pDemoUI )
		return;

	g_pDemoUI->DrawDebuggingInfo( frame, elapsed );
}