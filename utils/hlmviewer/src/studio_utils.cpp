/***
*
*	Copyright (c) 1998, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/
// updates:
// 1-4-99	fixed file texture load and file read bug

////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "StudioModel.h"
#include "vphysics/constraints.h"
#include "physmesh.h"
#include "materialsystem/imaterialsystem.h"
#include "ViewerSettings.h"
#include "bone_setup.h"
#include "UtlMemory.h"
#include "mx/mx.h"
#include "cmdlib.h"
#include "IStudioRender.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "MDLViewer.h"

extern IMaterialSystem *g_pMaterialSystem;
extern IMaterialSystemHardwareConfig *g_pMaterialSystemHardwareConfig;
extern char g_appTitle[];
IStudioRender	*StudioModel::m_pStudioRender;
Vector		    *StudioModel::m_AmbientLightColors;
Vector		    *StudioModel::m_TotalLightColors;

#pragma warning( disable : 4244 ) // double to float


static StudioModel g_studioModel;

// Expose it to the rest of the app
StudioModel *g_pStudioModel = &g_studioModel;

////////////////////////////////////////////////////////////////////////


void StudioModel::Init()
{
	// Load up the IStudioRender interface
	extern CreateInterfaceFn g_MaterialSystemClientFactory;
	extern CreateInterfaceFn g_MaterialSystemFactory;

	CSysModule *studioRenderDLL = NULL;
	char workingdir[ 256 ];
	workingdir[0] = 0;
	Q_getwd( workingdir );

	// If they didn't specify -game on the command line, use VPROJECT.
	CmdLib_InitFileSystem( workingdir, true );

	studioRenderDLL = Sys_LoadModule( "StudioRender.dll" );
	if( !studioRenderDLL )
	{
//		Msg( mwWarning, "Can't load StudioRender.dll\n" );
		assert( 0 ); // garymcthack
		return;
	}
	CreateInterfaceFn studioRenderFactory = Sys_GetFactory( "StudioRender.dll" );
	if (!studioRenderFactory )
	{
//		Msg( mwWarning, "Can't get studio render factory\n" );
		assert( 0 ); // garymcthack
		return;
	}
	m_pStudioRender = ( IStudioRender * )studioRenderFactory( STUDIO_RENDER_INTERFACE_VERSION, NULL );
	if (!m_pStudioRender)
	{
//		Msg( mwWarning, "Can't get version %s of StudioRender.dll\n", STUDIO_RENDER_INTERFACE_VERSION );
		assert( 0 ); // garymcthack
		return;
	}

	if( !m_pStudioRender->Init( g_MaterialSystemFactory, g_MaterialSystemClientFactory ) )
	{
//		Msg( mwWarning, "Can't initialize StudioRender.dll\n" );
		assert( 0 ); // garymcthack
		m_pStudioRender = NULL;
	}
	m_AmbientLightColors = new Vector[m_pStudioRender->GetNumAmbientLightSamples()];
	m_TotalLightColors = new Vector[m_pStudioRender->GetNumAmbientLightSamples()];
	UpdateStudioRenderConfig( g_viewerSettings.renderMode == RM_FLATSHADED ||
			g_viewerSettings.renderMode == RM_SMOOTHSHADED, 
			g_viewerSettings.renderMode == RM_WIREFRAME, 
			g_viewerSettings.showNormals ); // garymcthack - should really only do this once a frame and at init time.
}

void StudioModel::Shutdown( void )
{
	g_pStudioModel->FreeModel();
	if( m_pStudioRender )
	{
		m_pStudioRender->Shutdown();
		m_pStudioRender = NULL;
	}
	delete [] m_AmbientLightColors;
	delete [] m_TotalLightColors;
}


void StudioModel::ReleaseStudioModel()
{
	g_pStudioModel->FreeModel(); 
}

void StudioModel::RestoreStudioModel()
{
	if (g_pStudioModel->LoadModel(g_pStudioModel->m_pModelName))
	{
		g_pStudioModel->PostLoadModel( g_pStudioModel->m_pModelName );
	}
}



//-----------------------------------------------------------------------------
// Purpose: Frees the model data and releases textures from OpenGL.
//-----------------------------------------------------------------------------
void StudioModel::FreeModel ()
{
	if (!m_pStudioRender)
		return;

	m_pStudioRender->UnloadModel( &m_HardwareData );
	
	if (m_pstudiohdr)
		free (m_pstudiohdr);

	m_pstudiohdr = 0;

	int i;
	for (i = 0; i < 32; i++)
	{
		if (m_panimhdr[i])
		{
			free (m_panimhdr[i]);
			m_panimhdr[i] = 0;
		}
	}

#if 0
	// deleting textures
	g_texnum -= 3;
	int textures[MAXSTUDIOSKINS];
	for (i = 0; i < g_texnum; i++)
		textures[i] = i + 3;

	//glDeleteTextures (g_texnum, (const GLuint *) textures);
	g_texnum = 3;
#endif

	memset( &m_HardwareData, 0, sizeof( m_HardwareData ) );

	m_SurfaceProps.Purge();
	// BUG: Jay, when I call this it crashes
	// delete m_pPhysics;
}

void *StudioModel::operator new( size_t stAllocateBlock )
{
	// call into engine to get memory
	Assert( stAllocateBlock != 0 );
	return calloc( 1, stAllocateBlock );
}

void StudioModel::operator delete( void *pMem )
{
#ifdef _DEBUG
	// set the memory to a known value
	int size = _msize( pMem );
	memset( pMem, 0xcd, size );
#endif

	// get the engine to free the memory
	free( pMem );
}

bool StudioModel::LoadModel( const char *modelname )
{
	FILE *fp;
	long size;
	void *buffer;

	if (!modelname)
		return 0;

	// In the case of restore, m_pModelName == modelname
	if (m_pModelName != modelname)
	{
		// Copy over the model name; we'll need it later...
		if (m_pModelName)
			delete[] m_pModelName;
		m_pModelName = new char[strlen(modelname) + 1];
		strcpy( m_pModelName, modelname );
	}

	// load the model
	if( (fp = fopen( modelname, "rb" )) == NULL)
		return 0;

	fseek( fp, 0, SEEK_END );
	size = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	buffer = malloc( size );
	if (!buffer)
	{
		fclose (fp);
		return 0;
	}

	fread( buffer, size, 1, fp );
	fclose( fp );

	byte				*pin;
	studiohdr_t			*phdr;

	pin = (byte *)buffer;
	phdr = (studiohdr_t *)pin;

	if (strncmp ((const char *) buffer, "IDST", 4) &&
		strncmp ((const char *) buffer, "IDSQ", 4))
	{
		free (buffer);
		return 0;
	}

	if (!strncmp ((const char *) buffer, "IDSQ", 4) && !m_pstudiohdr)
	{
		free (buffer);
		return 0;
	}

	Studio_ConvertStudioHdrToNewVersion( phdr );
	
	if( phdr->version != STUDIO_VERSION )
	{
		free( buffer );
		return 0;
	}

	if (!m_pstudiohdr)
		m_pstudiohdr = (studiohdr_t *)buffer;

	// Load the VTX file.
	char* pExtension;
	if (g_pMaterialSystemHardwareConfig->SupportsVertexAndPixelShaders())
	{
		pExtension = ".dx80.vtx";
	}
	else
	{
		if( g_pMaterialSystemHardwareConfig->MaxBlendMatrices() > 2 )
		{
			pExtension = ".dx7_3bone.vtx";
		}
		else
		{
			pExtension = ".dx7_2bone.vtx";
		}
	}

	int vtxFileNameLen = strlen( modelname ) - strlen( ".mdl" ) + strlen( pExtension ) + 1;
	char *vtxFileName = ( char * )_alloca( vtxFileNameLen );
	strcpy( vtxFileName, modelname );
	strcpy( vtxFileName + strlen( vtxFileName ) - 4, pExtension );
	assert( ( int )strlen( vtxFileName ) == vtxFileNameLen - 1 );
	
	CUtlMemory<unsigned char> tmpVtxMem; // This goes away when we leave this scope.
	
	if( (fp = fopen( vtxFileName, "rb" )) == NULL)
	{
		// Fallback
		pExtension = ".dx7_2bone.vtx";
		vtxFileNameLen = strlen( modelname ) - strlen( ".mdl" ) + strlen( pExtension ) + 1;
		vtxFileName = ( char * )_alloca( vtxFileNameLen );
		strcpy( vtxFileName, modelname );
		strcpy( vtxFileName + strlen( vtxFileName ) - 4, pExtension );
		assert( ( int )strlen( vtxFileName ) == vtxFileNameLen - 1 );
		if( (fp = fopen( vtxFileName, "rb" )) == NULL)
		{
			// garymcthack - need to spew an error
			//		mxMessageBox (this, "Error reading vtx header.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
			free( buffer );
			m_pstudiohdr = NULL;
			return false;
		}
	}

	fseek( fp, 0, SEEK_END );
	size = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	tmpVtxMem.EnsureCapacity( size );
	
	fread( tmpVtxMem.Base(), size, 1, fp );
	fclose( fp );

	if( !m_pStudioRender->LoadModel( m_pstudiohdr, tmpVtxMem.Base(), &m_HardwareData ) )
	{
		// garymcthack - need to spew an error
//		Msg( mwWarning, "error loading model: %s\n", modelname );
		free( buffer );
		m_pstudiohdr = NULL;
		return false;
	}

	m_pPhysics = LoadPhysics( m_pstudiohdr, modelname );

	// Copy over all of the hitboxes; we may add and remove elements
	m_HitboxSets.RemoveAll();

	int i;
	int s;
	for ( s = 0; s < m_pstudiohdr->numhitboxsets; s++ )
	{
		mstudiohitboxset_t *set = m_pstudiohdr->pHitboxSet( s );
		if ( !set )
			continue;

		m_HitboxSets.AddToTail();

		for ( i = 0; i < set->numhitboxes; ++i )
		{
			mstudiobbox_t *pHit = set->pHitbox(i);
			m_HitboxSets[ s ].AddToTail( *set->pHitbox(i) );
		}

		// Set the name
		hbsetname_s *n = &m_HitboxSetNames[ m_HitboxSetNames.AddToTail() ];
		strcpy( n->name, set->pszName() );
	}

	// Copy over all of the surface props; we may change them...
	for ( i = 0; i < m_pstudiohdr->numbones; ++i )
	{
		mstudiobone_t* pBone = m_pstudiohdr->pBone(i);

		CUtlSymbol prop( pBone->pszSurfaceProp() );
		m_SurfaceProps.AddToTail( prop );
	}

	m_physPreviewBone = -1;
	return true;
}



bool StudioModel::PostLoadModel( const char *modelname )
{
	if (m_pstudiohdr == NULL)
	{
		return(false);
	}

	SetSequence (0);
	SetController (0, 0.0f);
	SetController (1, 0.0f);
	SetController (2, 0.0f);
	SetController (3, 0.0f);
	SetBlendTime( DEFAULT_BLEND_TIME );

	int n;
	for (n = 0; n < m_pstudiohdr->numbodyparts; n++)
		SetBodygroup (n, 0);

	SetSkin (0);

/*
	Vector mins, maxs;
	ExtractBbox (mins, maxs);
	if (mins[2] < 5.0f)
		m_origin[2] = -mins[2];
*/
	return true;
}



bool StudioModel::SaveModel ( const char *modelname )
{
	if (!modelname)
		return false;

	if (!m_pstudiohdr)
		return false;

	FILE *file;
	
	file = fopen (modelname, "wb");
	if (!file)
		return false;

	fwrite (m_pstudiohdr, sizeof (byte), m_pstudiohdr->length, file);
	fclose (file);

	// write seq groups
	if (m_pstudiohdr->numseqgroups > 1)
	{
		for (int i = 1; i < m_pstudiohdr->numseqgroups; i++)
		{
			char seqgroupname[256];

			strcpy( seqgroupname, modelname );
			sprintf( &seqgroupname[strlen(seqgroupname) - 4], "%02d.mdl", i );

			file = fopen (seqgroupname, "wb");
			if (file)
			{
				fwrite (m_panimhdr[i], sizeof (byte), m_panimhdr[i]->length, file);
				fclose (file);
			}
		}
	}

	return true;
}


////////////////////////////////////////////////////////////////////////


int StudioModel::GetSequence( )
{
	return m_sequence;
}

int StudioModel::SetSequence( int iSequence )
{
	if ( !m_pstudiohdr )
		return 0;

	if (iSequence < 0)
		return 0;

	if (iSequence > m_pstudiohdr->numseq)
		return m_sequence;

	m_prevsequence = m_sequence;
	m_sequence = iSequence;
	m_cycle = 0;
	m_sequencetime = 0.0;

	return m_sequence;
}

void StudioModel::ClearOverlaysSequences( void )
{
	ClearAnimationLayers( );
	memset( m_Layer, 0, sizeof( m_Layer ) );
}

void StudioModel::ClearAnimationLayers( void )
{
	m_iActiveLayers = 0;
}

int	StudioModel::GetNewAnimationLayer( int iPriority )
{
	if ( !m_pstudiohdr )
		return 0;

	if ( m_iActiveLayers >= MAXSTUDIOANIMLAYERS )
	{
		Assert( 0 );
		return MAXSTUDIOANIMLAYERS - 1;
	}

	m_Layer[m_iActiveLayers].m_priority = iPriority;

	return m_iActiveLayers++;
}

int StudioModel::SetOverlaySequence( int iLayer, int iSequence, float flWeight )
{
	if ( !m_pstudiohdr )
		return 0;

	if (iSequence < 0)
		return 0;

	if (iLayer < 0 || iLayer >= MAXSTUDIOANIMLAYERS)
	{
		Assert(0);
		return 0;
	}

	if (iSequence > m_pstudiohdr->numseq)
		return m_Layer[iLayer].m_sequence;

	m_Layer[iLayer].m_sequence = iSequence;
	m_Layer[iLayer].m_weight = flWeight;
	m_Layer[iLayer].m_playbackrate = 1.0;

	return iSequence;
}

float StudioModel::SetOverlayRate( int iLayer, float flCycle, float flPlaybackRate )
{
	if (iLayer >= 0 && iLayer < MAXSTUDIOANIMLAYERS)
	{
		m_Layer[iLayer].m_cycle = flCycle;
		m_Layer[iLayer].m_playbackrate = flPlaybackRate;
	}
	return flCycle;
}

int StudioModel::LookupSequence( const char *szSequence )
{
	int i;

	if ( !m_pstudiohdr )
		return -1;

	for (i = 0; i < m_pstudiohdr->numseq; i++)
	{
		if (!stricmp( szSequence, m_pstudiohdr->pSeqdesc( i )->pszLabel() ))
		{
			return i;
		}
	}
	return -1;
}

int StudioModel::SetSequence( const char *szSequence )
{
	return SetSequence( LookupSequence( szSequence ) );
}

void StudioModel::StartBlending( void )
{
	// Switch back to old sequence ( this will oscillate between this one and the last one )
	SetSequence( m_prevsequence );
}

void StudioModel::SetBlendTime( float blendtime )
{
	if ( blendtime > 0.0f )
	{
		m_blendtime = blendtime;
	}
}

float StudioModel::GetTransitionAmount( void )
{
	if ( g_viewerSettings.blendSequenceChanges &&
		m_sequencetime < m_blendtime && m_prevsequence != m_sequence )
	{
		float s;
		s = ( m_sequencetime / m_blendtime );
		return s;
	}

	return 0.0f;
}

int StudioModel::LookupFlexController( char *szName )
{
	if (!m_pstudiohdr)
		return false;

	for (int iFlex = 0; iFlex < m_pstudiohdr->numflexcontrollers; iFlex++)
	{
		if (stricmp( szName, m_pstudiohdr->pFlexcontroller( iFlex )->pszName() ) == 0)
		{
			return iFlex;
		}
	}
	return -1;
}


void StudioModel::SetFlexController( char *szName, float flValue )
{
	SetFlexController( LookupFlexController( szName ), flValue );
}

void StudioModel::SetFlexController( int iFlex, float flValue )
{
	if ( !m_pstudiohdr )
		return;

	if (iFlex >= 0 && iFlex < m_pstudiohdr->numflexdesc)
	{
		mstudioflexcontroller_t *pflex = m_pstudiohdr->pFlexcontroller(iFlex);

		if (pflex->min != pflex->max)
		{
			flValue = (flValue - pflex->min) / (pflex->max - pflex->min);
		}
		m_flexweight[iFlex] = clamp( flValue, 0.0f, 1.0f );
	}
}

float StudioModel::GetFlexController( char *szName )
{
	return GetFlexController( LookupFlexController( szName ) );
}

float StudioModel::GetFlexController( int iFlex )
{
	if ( !m_pstudiohdr )
		return 0.0f;

	if (iFlex >= 0 && iFlex < m_pstudiohdr->numflexdesc)
	{
		mstudioflexcontroller_t *pflex = m_pstudiohdr->pFlexcontroller(iFlex);

		float flValue = m_flexweight[iFlex];

		if (pflex->min != pflex->max)
		{
			flValue = flValue * (pflex->max - pflex->min) + pflex->min;
		}
		return flValue;
	}
	return 0.0;
}

int StudioModel::GetNumLODs() const
{
	return m_pStudioRender->GetNumLODs( m_HardwareData );
}

float StudioModel::GetLODSwitchValue( int lod ) const
{
	return m_pStudioRender->GetLODSwitchValue( m_HardwareData, lod );
}

void StudioModel::SetLODSwitchValue( int lod, float switchValue )
{
	m_pStudioRender->SetLODSwitchValue( m_HardwareData, lod, switchValue );
}

void StudioModel::ExtractBbox( Vector &mins, Vector &maxs )
{
	if ( !m_pstudiohdr )
		return;

	// look for hull
	if (m_pstudiohdr->hull_min.Length() != 0)
	{
		mins = m_pstudiohdr->hull_min;
		maxs = m_pstudiohdr->hull_max;
	}
	// look for view clip
	else if (m_pstudiohdr->view_bbmin.Length() != 0)
	{
		mins = m_pstudiohdr->view_bbmin;
		maxs = m_pstudiohdr->view_bbmax;
	}
	else
	{
		mstudioseqdesc_t *pseqdesc = m_pstudiohdr->pSeqdesc( 0 );

		mins = pseqdesc[ m_sequence ].bbmin;
		maxs = pseqdesc[ m_sequence ].bbmax;
	}
}



void StudioModel::GetSequenceInfo( int iSequence, float *pflFrameRate, float *pflGroundSpeed )
{
	float t = GetDuration( iSequence );

	if (t > 0)
	{
		*pflFrameRate = 1.0 / t;
		*pflGroundSpeed = 0; // sqrt( pseqdesc->linearmovement[0]*pseqdesc->linearmovement[0]+ pseqdesc->linearmovement[1]*pseqdesc->linearmovement[1]+ pseqdesc->linearmovement[2]*pseqdesc->linearmovement[2] );
		// *pflGroundSpeed = *pflGroundSpeed * pseqdesc->fps / (pseqdesc->numframes - 1);
	}
	else
	{
		*pflFrameRate = 1.0;
		*pflGroundSpeed = 0.0;
	}
}

void StudioModel::GetSequenceInfo( float *pflFrameRate, float *pflGroundSpeed )
{
	GetSequenceInfo( m_sequence, pflFrameRate, pflGroundSpeed );
}

float StudioModel::GetFPS( int iSequence )
{
	if ( !m_pstudiohdr )
		return 0.0f;

	return Studio_FPS( m_pstudiohdr, iSequence, m_poseparameter );
}

float StudioModel::GetFPS( void )
{
	return GetFPS( m_sequence );
}

float StudioModel::GetDuration( int iSequence )
{
	if ( !m_pstudiohdr )
		return 0.0f;

	return Studio_Duration( m_pstudiohdr, iSequence, m_poseparameter );
}


static int GetSequenceFlags( studiohdr_t *pstudiohdr, int sequence )
{
	if ( !pstudiohdr || 
		sequence < 0 || 
		sequence >= pstudiohdr->numseq )
	{
		return 0;
	}

	mstudioseqdesc_t *pseqdesc = pstudiohdr->pSeqdesc( sequence );

	return pseqdesc->flags;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iSequence - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool StudioModel::GetSequenceLoops( int iSequence )
{
	if ( !m_pstudiohdr )
		return false;

	int flags = GetSequenceFlags( m_pstudiohdr, iSequence );
	bool looping = flags & STUDIO_LOOPING ? true : false;
	return looping;
}

float StudioModel::GetDuration( )
{
	return GetDuration( m_sequence );
}


void StudioModel::GetMovement( float dt, Vector &vecPos, QAngle &vecAngles )
{
	float t = GetDuration( );

	float flNext = m_cycle + dt / t;

	GetMovement( m_cycle, flNext, vecPos, vecAngles );

	return;
}


void StudioModel::GetMovement( float prevCycle, float nextCycle, Vector &vecPos, QAngle &vecAngles )
{
	if ( !m_pstudiohdr )
	{
		vecPos.Init();
		vecAngles.Init();
		return;
	}

	// FIXME: this doesn't consider layers
	Studio_SeqMovement( m_pstudiohdr, m_sequence, prevCycle, nextCycle, m_poseparameter, vecPos, vecAngles );

	return;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the ground speed of the current sequence.
//-----------------------------------------------------------------------------
float StudioModel::GetGroundSpeed( void )
{
	Vector vecMove;
	QAngle vecAngles;
	GetMovement( 0, 1, vecMove, vecAngles );

	float t = GetDuration();

	float flGroundSpeed = 0;
	if (t > 0)
	{
		flGroundSpeed = vecMove.Length() / t;
	}

	return flGroundSpeed;
}


void StudioModel::GetSeqAnims( int iSequence, mstudioanimdesc_t *panim[4], float *weight )
{
	if (!m_pstudiohdr)
		return;

	Studio_SeqAnims( m_pstudiohdr, iSequence, m_poseparameter, panim, weight );
}

void StudioModel::GetSeqAnims( mstudioanimdesc_t *panim[4], float *weight )
{
	GetSeqAnims( m_sequence, panim, weight );
}


float StudioModel::SetController( int iController, float flValue )
{
	if (!m_pstudiohdr)
		return 0.0f;

	return Studio_SetController( m_pstudiohdr, iController, flValue, m_controller[iController] );
}



int	StudioModel::LookupPoseParameter( char const *szName )
{
	if (!m_pstudiohdr)
		return false;

	for (int iParameter = 0; iParameter < m_pstudiohdr->numposeparameters; iParameter++)
	{
		if (stricmp( szName, m_pstudiohdr->pPoseParameter( iParameter )->pszName() ) == 0)
		{
			return iParameter;
		}
	}
	return -1;
}

float StudioModel::SetPoseParameter( char const *szName, float flValue )
{
	return SetPoseParameter( LookupPoseParameter( szName ), flValue );
}

float StudioModel::SetPoseParameter( int iParameter, float flValue )
{
	if (!m_pstudiohdr)
		return 0.0f;

	return Studio_SetPoseParameter( m_pstudiohdr, iParameter, flValue, m_poseparameter[iParameter] );
}

float StudioModel::GetPoseParameter( char const *szName )
{
	return GetPoseParameter( LookupPoseParameter( szName ) );
}

float StudioModel::GetPoseParameter( int iParameter )
{
	if (!m_pstudiohdr)
		return 0.0f;

	return Studio_GetPoseParameter( m_pstudiohdr, iParameter, m_poseparameter[iParameter] );
}

bool StudioModel::GetPoseParameterRange( int iParameter, float *pflMin, float *pflMax )
{
	*pflMin = 0;
	*pflMax = 0;

	if (!m_pstudiohdr)
		return false;

	if (iParameter >= m_pstudiohdr->numposeparameters)
		return false;

	mstudioposeparamdesc_t *pPose = m_pstudiohdr->pPoseParameter( iParameter );

	*pflMin = pPose->start;
	*pflMax = pPose->end;

	return true;
}

int StudioModel::LookupAttachment( char const *szName )
{
	if ( !m_pstudiohdr )
		return -1;

	for (int i = 0; i < m_pstudiohdr->numattachments; i++)
	{
		if (stricmp( m_pstudiohdr->pAttachment( i )->pszName(), szName ) == 0)
		{
			return i;
		}
	}
	return -1;
}



int StudioModel::SetBodygroup( int iGroup, int iValue )
{
	if (!m_pstudiohdr)
		return 0;

	if (iGroup > m_pstudiohdr->numbodyparts)
		return -1;

	mstudiobodyparts_t *pbodypart = m_pstudiohdr->pBodypart( iGroup );

	int iCurrent = (m_bodynum / pbodypart->base) % pbodypart->nummodels;

	if (iValue >= pbodypart->nummodels)
		return iCurrent;

	m_bodynum = (m_bodynum - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));

	return iValue;
}


int StudioModel::SetSkin( int iValue )
{
	if (!m_pstudiohdr)
		return 0;

	if (iValue >= m_pstudiohdr->numskinfamilies)
	{
		return m_skinnum;
	}

	m_skinnum = iValue;

	return iValue;
}



void StudioModel::scaleMeshes (float scale)
{
	if (!m_pstudiohdr)
		return;

	int i, j, k;

	// scale verts
	int tmp = m_bodynum;
	for (i = 0; i < m_pstudiohdr->numbodyparts; i++)
	{
		mstudiobodyparts_t *pbodypart = m_pstudiohdr->pBodypart( i );
		for (j = 0; j < pbodypart->nummodels; j++)
		{
			SetBodygroup (i, j);
			SetupModel (i);

			for (k = 0; k < m_pmodel->numvertices; k++)
			{
				*m_pmodel->Position(k) *= scale;
			}
		}
	}

	m_bodynum = tmp;

	// scale complex hitboxes
	int hitboxset = g_MDLViewer->GetCurrentHitboxSet();

	mstudiobbox_t *pbboxes = m_pstudiohdr->pHitbox( 0, hitboxset );
	for (i = 0; i < m_pstudiohdr->iHitboxCount( hitboxset ); i++)
	{
		VectorScale (pbboxes[i].bbmin, scale, pbboxes[i].bbmin);
		VectorScale (pbboxes[i].bbmax, scale, pbboxes[i].bbmax);
	}

	// scale bounding boxes
	mstudioseqdesc_t *pseqdesc = m_pstudiohdr->pSeqdesc( 0 );
	for (i = 0; i < m_pstudiohdr->numseq; i++)
	{
		VectorScale (pseqdesc[i].bbmin, scale, pseqdesc[i].bbmin);
		VectorScale (pseqdesc[i].bbmax, scale, pseqdesc[i].bbmax);
	}

	// maybe scale exeposition, pivots, attachments
}



void StudioModel::scaleBones (float scale)
{
	if (!m_pstudiohdr)
		return;

	mstudiobone_t *pbones = m_pstudiohdr->pBone( 0 );
	for (int i = 0; i < m_pstudiohdr->numbones; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			pbones[i].value[j] *= scale;
			pbones[i].scale[j] *= scale;
		}
	}	
}

int	StudioModel::Physics_GetBoneCount( void )
{
	return m_pPhysics->Count();
}


const char *StudioModel::Physics_GetBoneName( int index ) 
{ 
	CPhysmesh *pmesh = m_pPhysics->GetMesh( index );

	if ( !pmesh )
		return NULL;

	return pmesh->m_boneName;
}


void StudioModel::Physics_GetData( int boneIndex, hlmvsolid_t *psolid, constraint_ragdollparams_t *pConstraint ) const
{
	CPhysmesh *pMesh = m_pPhysics->GetMesh( boneIndex );
	
	if ( !pMesh )
		return;

	if ( psolid )
	{
		memcpy( psolid, &pMesh->m_solid, sizeof(*psolid) );
	}

	if ( pConstraint )
	{
		*pConstraint = pMesh->m_constraint;
	}
}

void StudioModel::Physics_SetData( int boneIndex, const hlmvsolid_t *psolid, const constraint_ragdollparams_t *pConstraint )
{
	CPhysmesh *pMesh = m_pPhysics->GetMesh( boneIndex );
	
	if ( !pMesh )
		return;

	if ( psolid )
	{
		memcpy( &pMesh->m_solid, psolid, sizeof(*psolid) );
	}

	if ( pConstraint )
	{
		pMesh->m_constraint = *pConstraint;
	}
}


float StudioModel::Physics_GetMass( void )
{
	return m_pPhysics->GetMass();
}

void StudioModel::Physics_SetMass( float mass )
{
	m_physMass = mass;
}


char *StudioModel::Physics_DumpQC( void )
{
	return m_pPhysics->DumpQC();
}

