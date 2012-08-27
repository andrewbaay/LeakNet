#include "glquake.h"
#include "sys.h"
#include "filesystem_engine.h"
#include "utldict.h"
#include "demo.h"

// Imported from other .cpp files
void Host_Map_f (void);

//-----------------------------------------------------------------------------
// Purpose: For each map, stores when the map last changed on disk and whether
//  it is a valid map
//-----------------------------------------------------------------------------
class CMapListItem
{
public:
	enum
	{
		INVALID = 0,
		PENDING,
		VALID,
	};

					CMapListItem( void );
	
	void			SetValid( int valid );
	int				GetValid( void ) const;

	void			SetFileTimestamp( long ts );
	long			GetFileTimestamp( void ) const;

	bool			IsSameTime( long ts ) const;

	static long		GetFSTimeStamp( char const *name );
	static int		CheckFSHeaderVersion( char const *name );

private:
	int				m_nValid;
	long			m_lFileTimestamp;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapListItem::CMapListItem( void )
{
	m_nValid = PENDING;
	m_lFileTimestamp = 0L;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : valid - 
//-----------------------------------------------------------------------------
void CMapListItem::SetValid( int valid )
{
	m_nValid = valid;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CMapListItem::GetValid( void ) const
{
	return m_nValid;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ts - 
//-----------------------------------------------------------------------------
void CMapListItem::SetFileTimestamp( long ts )
{
	m_lFileTimestamp = ts;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : long
//-----------------------------------------------------------------------------
long CMapListItem::GetFileTimestamp( void ) const
{
	return m_lFileTimestamp;
}

//-----------------------------------------------------------------------------
// Purpose: Check whether this map file has changed related to the passed in timestamp
// Input  : ts - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapListItem::IsSameTime( long ts ) const
{
	return ( m_lFileTimestamp == ts ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the timestamp for the file from the file system
// Input  : *name - 
// Output : long
//-----------------------------------------------------------------------------
long CMapListItem::GetFSTimeStamp( char const *name )
{
	long ts = g_pFileSystem->GetFileTime( name );
	return ts;
}

//-----------------------------------------------------------------------------
// Purpose: Check whether the specified map header version is up-to-date
// Input  : *name - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CMapListItem::CheckFSHeaderVersion( char const *name )
{
	dheader_t header;
	memset( &header, 0, sizeof( header ) );

	FileHandle_t fp = g_pFileSystem->Open ( name, "rb");
	if ( fp )
	{
		g_pFileSystem->Read( &header, sizeof( header ), fp );
		g_pFileSystem->Close( fp );
	}

	return ( header.version == BSPVERSION ) ? VALID : INVALID;
}

// How often to check the filesystem for updated map info
#define MIN_REFRESH_INTERVAL 60.0f

//-----------------------------------------------------------------------------
// Purpose: Stores the current list of maps for the engine
//-----------------------------------------------------------------------------
class CMapListManager
{
public:
	CMapListManager( void );
	~CMapListManager( void );

	// See if it's time to revisit the items in the list
	void			RefreshList( void );

	// Get item count, etc
	int				GetMapCount( void ) const;
	int				IsMapValid( int index ) const;
	char const 		*GetMapName( int index ) const;

	void			Think( void );

private:
	// Clear list
	void			ClearList( void );
	// Rebuild list from scratch
	void			BuildList( void );

private:
	// Dictionary of items
	CUtlDict< CMapListItem, int > m_Items;

	// Time of last update
	float			m_flLastRefreshTime;

	bool			m_bDirty;
};

// Singleton manager object
static CMapListManager g_MapListMgr;

void Host_UpdateMapList( void )
{
	g_MapListMgr.Think();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapListManager::CMapListManager( void )
{
	m_flLastRefreshTime = -1.0f;
	m_bDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapListManager::~CMapListManager( void )
{
	ClearList();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapListManager::Think( void )
{
	if ( !m_bDirty )
		return;

	int i;

	m_bDirty = false;
		
	for ( i = m_Items.Count() - 1; i >= 0 ; i-- )
	{
		CMapListItem *item = &m_Items[ i ];
		if ( item->GetValid() != CMapListItem::PENDING )
		{
			continue;
		}

		char const *filename = m_Items.GetElementName( i );

		item->SetValid( CMapListItem::CheckFSHeaderVersion( filename ) );

		// Keep fixing things up next frame
		m_bDirty = true;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: FIXME:  Refresh doesn't notice maps that have been deleted... oh well
//-----------------------------------------------------------------------------
void CMapListManager::RefreshList( void )
{
	if ( m_flLastRefreshTime == -1.0f )
	{
		BuildList();
		return;
	}

	if ( realtime < m_flLastRefreshTime + MIN_REFRESH_INTERVAL )
		return;

	Con_DPrintf( "Refreshing map list...\n" );

	// Search the directory structure.
	char mapwild[MAX_QPATH];
	strcpy(mapwild,"maps/*.bsp");
	char const *findfn = Sys_FindFirst( mapwild, NULL );
	while ( findfn )
	{
		char sz[ MAX_QPATH ];
		Q_snprintf( sz, sizeof( sz ), "maps/%s", findfn );
	
		int idx = m_Items.Find( sz );
		if ( idx == m_Items.InvalidIndex() )
		{
			CMapListItem item;
			item.SetFileTimestamp( item.GetFSTimeStamp( sz ) );
			item.SetValid( CMapListItem::PENDING );
			// Insert into dictionary
			m_Items.Insert( sz, item );

			m_bDirty = true;
		}
		else
		{
			CMapListItem *item = &m_Items[ idx ];
			Assert( item );

			// Make sure data is up to date
			long timestamp = g_pFileSystem->GetFileTime( sz );
			if ( !item->IsSameTime( timestamp ) )
			{
				item->SetFileTimestamp( timestamp );
				item->SetValid( CMapListItem::PENDING );

				m_bDirty = true;
			}
		}

		findfn = Sys_FindNext( NULL );
	}

	Sys_FindClose();

	m_flLastRefreshTime = realtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CMapListManager::GetMapCount( void ) const
{
	return m_Items.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CMapListManager::IsMapValid( int index ) const
{
	if ( !m_Items.IsValidIndex( index ) )
		return false;

	CMapListItem const *item = &m_Items[ index ];
	Assert( item );
	return item->GetValid();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CMapListManager::GetMapName( int index ) const
{
	if ( !m_Items.IsValidIndex( index ) )
		return "Invalid!!!";

	return m_Items.GetElementName( index );
}

//-----------------------------------------------------------------------------
// Purpose: Wipe the list
//-----------------------------------------------------------------------------
void CMapListManager::ClearList( void )
{
	m_Items.Purge();
	m_bDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: Rebuild the entire list
//-----------------------------------------------------------------------------
void CMapListManager::BuildList( void )
{
	ClearList();

	// Search the directory structure.
	char mapwild[MAX_QPATH];
	strcpy(mapwild,"maps/*.bsp");
	char const *findfn = Sys_FindFirst( mapwild, NULL );
	while ( findfn )
	{
		char sz[ MAX_QPATH ];
		Q_snprintf( sz, sizeof( sz ), "maps/%s", findfn );
	
		CMapListItem item;
		item.SetFileTimestamp( item.GetFSTimeStamp( sz ) );
		item.SetValid( CMapListItem::PENDING );

		// Insert into dictionary
		m_Items.Insert( sz, item );

		findfn = Sys_FindNext( NULL );
	}

	Sys_FindClose();

	// Remember time we build the list
	m_flLastRefreshTime = realtime;

	m_bDirty = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pakorfilesys - 
//			*mapname - 
// Output : static void
//-----------------------------------------------------------------------------
static bool MapList_CheckPrintMap( const char *pakorfilesys, const char *mapname, int valid,
							   bool showoutdated, bool verbose )
{
	bool validorpending = ( valid != CMapListItem::INVALID ) ? true : false;

	if ( !verbose )
	{
		return validorpending;
	}

	char prefix[ 32 ];
	prefix[ 0 ] = 0;

	switch ( valid )
	{
	default:
	case CMapListItem::VALID:
		break;
	case CMapListItem::PENDING:
		Q_strncpy( prefix, "PENDING:  ", sizeof( prefix ) );
		break;
	case CMapListItem::INVALID:
		Q_strncpy( prefix, "OUTDATED:  ", sizeof( prefix ) );
		break;
	}

	if ( validorpending ^ showoutdated )
	{
		Con_Printf( "%s %s %s\n", prefix, pakorfilesys, mapname );
	}

	return validorpending;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszSubString - 
//			listobsolete - 
//			maxitemlength - 
// Output : static int
//-----------------------------------------------------------------------------
static int MapList_CountMaps( const char *pszSubString, bool listobsolete, int& maxitemlength )
{
	g_MapListMgr.RefreshList();

	maxitemlength = 0;

	int substringlength = 0;
	if ( pszSubString && pszSubString[0] )
	{
		substringlength = strlen(pszSubString);
	}
	
	//
	// search through the path, one element at a time
	//
	int count = 0;
	int showOutdated;
	for( showOutdated = listobsolete ? 1 : 0; showOutdated >= 0; showOutdated-- )
	{
		for ( int i = 0; i < g_MapListMgr.GetMapCount(); i++ )
		{
			char const *mapname = g_MapListMgr.GetMapName( i );
			int valid = g_MapListMgr.IsMapValid( i );

			if ( !substringlength || !strnicmp( &mapname[ 5 ], pszSubString, substringlength ) )
			{
				if ( MapList_CheckPrintMap( "(fs)", &mapname[ 5 ], valid, showOutdated ? true : false, false ) )
				{
					maxitemlength = max( maxitemlength, (int)( strlen( &mapname[ 5 ] ) + 1 ) );
					count++;
				}
			}
		}
	}

	return count;
}	

//-----------------------------------------------------------------------------
// Purpose: 
//  Lists all maps matching the substring
//  If the substring is empty, or "*", then lists all maps
// Input  : *pszSubString - 
//-----------------------------------------------------------------------------
static int MapList_ListMaps( const char *pszSubString, bool listobsolete, bool verbose, int maxcount, int maxitemlength, char maplist[][ 64 ] )
{
	int substringlength = 0;
	if (pszSubString && pszSubString[0])
	{
		substringlength = strlen(pszSubString);
	}

	//
	// search through the path, one element at a time
	//

	if ( verbose )
	{
		Con_Printf( "-------------\n");
	}

	int count = 0;
	int showOutdated;
	for( showOutdated = listobsolete ? 1 : 0; showOutdated >= 0; showOutdated-- )
	{
		if ( count >= maxcount )
			break;

		//search the directory structure.
		for ( int i = 0; i < g_MapListMgr.GetMapCount(); i++ )
		{
			if ( count >= maxcount )
				break;

			char const *mapname = g_MapListMgr.GetMapName( i );
			int valid = g_MapListMgr.IsMapValid( i );

			if ( !substringlength || !strnicmp( &mapname[ 5 ], pszSubString, substringlength ) )
			{
				if ( MapList_CheckPrintMap( "(fs)", &mapname[ 5 ], valid, showOutdated ? true : false, verbose ) )
				{
					if ( maxitemlength != 0 )
					{
						Q_strncpy( maplist[ count ], &mapname[ 5 ], maxitemlength );
					}
					count++;
				}
			}
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *partial - 
//			context - 
//			longest - 
//			maxcommands - 
//			**commands - 
// Output : int
//-----------------------------------------------------------------------------
static int _Host_Map_f_CompletionFunc( char const *cmdname, char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char *substring = (char *)partial;
	if ( Q_strstr( partial, cmdname ) )
	{
		substring = (char *)partial + strlen( cmdname );
	}

	int longest = 0;
	int count = min( MapList_CountMaps( substring, false, longest ), COMMAND_COMPLETION_MAXITEMS );
	if ( count > 0 )
	{
		MapList_ListMaps( substring, false, false, COMMAND_COMPLETION_MAXITEMS, longest, commands );

		// Now prepend maps * in front of all of the options
		int i;
		for ( i = 0; i < count ; i++ )
		{
			char old[ COMMAND_COMPLETION_ITEM_LENGTH ];
			Q_strncpy( old, commands[ i ], sizeof( old ) );
			Q_snprintf( commands[ i ], sizeof( commands[ i ] ), "%s%s", cmdname, old );
			commands[ i ][ strlen( commands[ i ] ) - 4 ] = 0;
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *partial - 
//			context - 
//			longest - 
//			maxcommands - 
//			**commands - 
// Output : int
//-----------------------------------------------------------------------------
static int Host_Map_f_CompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = "map ";
	return _Host_Map_f_CompletionFunc( cmdname, partial, commands );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *partial - 
//			context - 
//			longest - 
//			maxcommands - 
//			**commands - 
// Output : int
//-----------------------------------------------------------------------------
static int Host_DemoMap_f_CompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = "demomap ";
	return _Host_Map_f_CompletionFunc( cmdname, partial, commands );
}


//-----------------------------------------------------------------------------
// Purpose: do a dir of the maps dir
//-----------------------------------------------------------------------------
static void Host_Maps_f (void)
{
	char *pszSubString = NULL;

	if ( Cmd_Argc() != 2 && Cmd_Argc() != 3 )
	{
		Msg("Usage:  maps <substring>\nmaps * for full listing\n");
		return;
	}

	if ( Cmd_Argc() == 2 )
	{
		pszSubString = Cmd_Argv(1);
		if (!pszSubString || !pszSubString[0])
			return;
	}

	if ( pszSubString && ( pszSubString[0] == '*' ))
		pszSubString = NULL;

	int longest = 0;
	int count = MapList_CountMaps( pszSubString, true, longest );
	if ( count > 0 )
	{
		MapList_ListMaps( pszSubString, true, true, count, 0, NULL );
	}
}

#ifndef SWDS
//-----------------------------------------------------------------------------
// Purpose: 
// Output : static void
//-----------------------------------------------------------------------------
static void Host_DemoMap_f( void )
{
	if ( Cmd_Argc() != 3 )
	{
		Msg("Usage:  demomap mapname demoname\n");
		return;
	}

	char mapname[ 512 ];
	char demoname[ 512 ];
	Q_snprintf( mapname, sizeof( mapname ), "%s", Cmd_Argv(1) );
	Q_snprintf( demoname, sizeof( demoname ), "%s", Cmd_Argv(2) );

	Cbuf_AddText( va( "map %s\n", mapname ) );
	demo->SetAutoRecord( mapname, demoname );
}
static ConCommand demomap("demomap", Host_DemoMap_f, "demomap mapname demoname:  start map and start recording .demo", 0, Host_DemoMap_f_CompletionFunc );
#endif

#ifndef BENCHMARK
static ConCommand maps("maps", Host_Maps_f, "Displays list of maps." );
static ConCommand map("map", Host_Map_f, "Start playing on specified map.", 0, Host_Map_f_CompletionFunc );
#endif
