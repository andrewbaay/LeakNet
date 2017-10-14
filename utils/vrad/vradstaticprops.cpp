//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Revision: $
// $NoKeywords: $
//
// This file contains code to allow us to associate client data with bsp leaves.
//
//=============================================================================

#include "vrad.h"
#include "Vector.h"
#include "UtlBuffer.h"
#include "UtlVector.h"
#include "GameBSPFile.h"
#include "BSPTreeData.h"
#include "VPhysics_Interface.h"
#include "Studio.h"
#include "Optimize.h"
#include "Bsplib.h"
#include "CModel.h"
#include "PhysDll.h"
#include "phyfile.h"
#include "collisionutils.h"
//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

// DON'T USE THIS FROM WITHIN A THREAD.  THERE IS A THREAD CONTEXT CREATED 
// INSIDE PropTested_t.  USE THAT INSTEAD.
IPhysicsCollision *s_pPhysCollision = NULL;

//-----------------------------------------------------------------------------
// Vrad's static prop manager
//-----------------------------------------------------------------------------

class CVradStaticPropMgr : public IVradStaticPropMgr, public ISpatialLeafEnumerator,
							public IBSPTreeDataEnumerator
{
public:
	// constructor, destructor
	CVradStaticPropMgr();
	virtual ~CVradStaticPropMgr();

	// methods of IStaticPropMgr
	void Init();
	void Shutdown();
	bool ClipRayToStaticProps( PropTested_t& propTested, Ray_t const& ray );
	bool ClipRayToStaticPropsInLeaf( PropTested_t& propTested, Ray_t const& ray, int leaf );
	void StartRayTest( PropTested_t& propTested );

	// ISpatialLeafEnumerator
	bool EnumerateLeaf( int leaf, int context );

	// IBSPTreeDataEnumerator
	bool FASTCALL EnumerateElement( int userId, int context );

private:
	// Methods associated with unserializing static props
	void UnserializeModelDict( CUtlBuffer& buf );
	void UnserializeModels( CUtlBuffer& buf );
	void UnserializeStaticProps();

	// For raycasting against props
	void InsertPropIntoTree( int propIndex );
	void RemovePropFromTree( int propIndex );

	// Load studio model data from a file...
	bool LoadStudioModelVtx( char const* pModelName, CUtlBuffer& buf );

	// Creates a collision model
	void CreateCollisionModel( char const* pModelName );

private:
	// Unique static prop models
	struct StaticPropDict_t
	{
		vcollide_t	m_loadedModel;
		CPhysCollide* m_pModel;
		Vector	m_Mins;		// Bounding box is in local coordinates
		Vector	m_Maxs;
	};

	// A static prop
	struct CStaticProp
	{
		Vector	m_Origin;
		QAngle	m_Angles;
		Vector	m_mins;
		Vector	m_maxs;
		int		m_ModelIdx;
		BSPTreeDataHandle_t	m_Handle;
	};

	// Enumeration context
	struct EnumContext_t
	{
		PropTested_t* m_pPropTested;
		Ray_t const* m_pRay;
	};

	// The list of all static props
	CUtlVector <StaticPropDict_t>	m_StaticPropDict;
	CUtlVector <CStaticProp>		m_StaticProps;

	IBSPTreeData*	m_pBSPTreeData;
};


//-----------------------------------------------------------------------------
// Expose IVradStaticPropMgr to vrad
//-----------------------------------------------------------------------------

IVradStaticPropMgr* StaticPropMgr()
{
	static CVradStaticPropMgr	s_StaticPropMgr;
	return &s_StaticPropMgr;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

CVradStaticPropMgr::CVradStaticPropMgr()
{
	m_pBSPTreeData = CreateBSPTreeData();
}

CVradStaticPropMgr::~CVradStaticPropMgr()
{
	DestroyBSPTreeData( m_pBSPTreeData );
}


//-----------------------------------------------------------------------------
// Insert, remove a static prop from the tree for collision
//-----------------------------------------------------------------------------

void CVradStaticPropMgr::InsertPropIntoTree( int propIndex )
{
	CStaticProp& prop = m_StaticProps[propIndex];

	StaticPropDict_t& dict = m_StaticPropDict[prop.m_ModelIdx];

	// Compute the bbox of the prop
	if ( dict.m_pModel )
	{
		s_pPhysCollision->CollideGetAABB( prop.m_mins, prop.m_maxs, dict.m_pModel, prop.m_Origin, prop.m_Angles );
	}
	else
	{
		VectorAdd( dict.m_Mins, prop.m_Origin, prop.m_mins );
		VectorAdd( dict.m_Maxs, prop.m_Origin, prop.m_maxs );
	}

	// add the entity to the tree so we will collide against it
	prop.m_Handle = m_pBSPTreeData->Insert( propIndex, prop.m_mins, prop.m_maxs );
}

void CVradStaticPropMgr::RemovePropFromTree( int propIndex )
{
	// Release the tree handle
	if (m_StaticProps[propIndex].m_Handle != TREEDATA_INVALID_HANDLE)
	{
		m_pBSPTreeData->Remove( m_StaticProps[propIndex].m_Handle );
		m_StaticProps[propIndex].m_Handle = TREEDATA_INVALID_HANDLE;
	}
}


//-----------------------------------------------------------------------------
// Makes sure the studio model is a static prop
//-----------------------------------------------------------------------------

bool IsStaticProp( studiohdr_t* pHdr )
{
 	if ( (pHdr->numbones > 1) || (pHdr->numanim > 1) || (pHdr->numflexrules > 0) ||
		 (pHdr->nummouths > 0) )
		return false;

	mstudiobone_t* pBone = pHdr->pBone(0);
	if ( pBone->bonecontroller[0] != -1 )
		return false;
	
	// Static props must have identity pose to bone
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			if (i == j)
			{
				if ( fabs( pBone->poseToBone[i][j] - 1.0f ) > 1e-3 )
					return false;
			}
			else
			{
				if ( fabs( pBone->poseToBone[i][j] ) > 1e-3 )
					return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Load a file into a Utlbuf
//-----------------------------------------------------------------------------

static bool LoadFile( char const* pFileName, CUtlBuffer& buf )
{
	FileHandle_t fp;

	// load the model
	if( (fp = g_pFileSystem->Open(pFileName, "rb" )) == NULL)
		return false;

	// Get the file size
	int size = g_pFileSystem->Size( fp );
	if (size == 0)
	{
		g_pFileSystem->Close( fp );
		return false;
	}

	buf.EnsureCapacity( size );
	g_pFileSystem->Read( buf.PeekPut(), size, fp );
	g_pFileSystem->Close( fp );

	buf.SeekPut( CUtlBuffer::SEEK_HEAD, size );
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	return true;
}

//-----------------------------------------------------------------------------
// Constructs the file name from the model name
//-----------------------------------------------------------------------------

static char const* ConstructFileName( char const* pModelName )
{
	static char buf[1024];

	// VXP: Dirty hack: check if model is present in mod folder. If not - search at basegamedir
	sprintf( buf, "%s%s", gamedir, pModelName );

	if ( Q_strcmp( gamedir, basegamedir ) != 0 ) // VXP: If gamedir is not equal with basegamedir
	{
		FILE *fp;
		fp = fopen( buf, "rb" );
		if ( !fp )
		{
			// VXP: Try to open this model from basegamedir then
			sprintf( buf, "%s%s", basegamedir, pModelName );
			fp = fopen( buf, "rb" );
			if ( !fp )
			{
				// VXP: Can't find a model - leave a string as it should be
				sprintf( buf, "%s%s", gamedir, pModelName );
			}
		}

		if ( fp )
			fclose( fp );
	}


	return buf;
}


//-----------------------------------------------------------------------------
// Computes a convex hull from a studio mesh
//-----------------------------------------------------------------------------
static CPhysConvex* ComputeConvexHull( mstudiomesh_t* pMesh )
{
	// Generate a list of all verts in the mesh
	Vector** ppVerts = (Vector**)_alloca(pMesh->numvertices * sizeof(Vector*) );
	for (int i = 0; i < pMesh->numvertices; ++i)
	{
		ppVerts[i] = pMesh->Position(i);
	}

	// Generate a convex hull from the verts
	return s_pPhysCollision->ConvexFromVerts( ppVerts, pMesh->numvertices );
}


//-----------------------------------------------------------------------------
// Computes a convex hull from the studio model
//-----------------------------------------------------------------------------
CPhysCollide* ComputeConvexHull( studiohdr_t* pStudioHdr )
{
	CUtlVector<CPhysConvex*>	convexHulls;
	for (int body = 0; body < pStudioHdr->numbodyparts; ++body )
	{
		mstudiobodyparts_t *pBodyPart = pStudioHdr->pBodypart( body );
		for( int model = 0; model < pBodyPart->nummodels; ++model )
		{
			mstudiomodel_t *pStudioModel = pBodyPart->pModel( model );
			for( int mesh = 0; mesh < pStudioModel->nummeshes; ++mesh )
			{
				// Make a convex hull for each mesh
				// NOTE: This won't work unless the model has been compiled
				// with $staticprop
				mstudiomesh_t *pStudioMesh = pStudioModel->pMesh( mesh );
				convexHulls.AddToTail( ComputeConvexHull( pStudioMesh ) );
			}
		}
	}

	// Convert an array of convex elements to a compiled collision model
	// (this deletes the convex elements)
	return s_pPhysCollision->ConvertConvexToCollide( convexHulls.Base(), convexHulls.Size() );
}


//-----------------------------------------------------------------------------
// The triangle indices
//-----------------------------------------------------------------------------

bool CVradStaticPropMgr::LoadStudioModelVtx( char const* pModelName, CUtlBuffer& buf )
{
	char tempFileName[256];

	char const* pFileName = ConstructFileName( pModelName );

	strcpy( tempFileName, pFileName );
	char* pTex = strrchr( tempFileName, '.' );
	Assert( pTex );
	strcpy( pTex, ".dx80.vtx" );
	
	if (!LoadFile( tempFileName, buf ))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Load studio model vertex data from a file...
//-----------------------------------------------------------------------------

bool LoadStudioModel( char const* pModelName, CUtlBuffer& buf )
{
	// No luck, gotta build it	
	// Construct the file name...
	char const* pFileName = ConstructFileName( pModelName );

	if (!LoadFile( pFileName, buf ))
	{
		Warning("Error! Unable to load model \"%s\"\n", pFileName );
		return false;
	}

	// Check that it's valid
	if (strncmp ((const char *) buf.PeekGet(), "IDST", 4) &&
		strncmp ((const char *) buf.PeekGet(), "IDSQ", 4))
	{
		Warning("Error! Invalid model file \"%s\"\n", pFileName );
		return false;
	}

	studiohdr_t* pHdr = (studiohdr_t*)buf.PeekGet();
//	Studio_ConvertStudioHdrToNewVersion( pHdr );
	if (pHdr->version != STUDIO_VERSION)
	{
		Warning("Error! Invalid model version \"%s\"\n", pFileName );
		return false;
	}

	if (!IsStaticProp(pHdr))
	{
		Warning("Error! To use model \"%s\"\n"
			"      as a static prop, it must be compiled with $staticprop!\n", pFileName );
		return false;
	}

	return true;
}

bool LoadStudioCollisionModel( char const* pModelName, CUtlBuffer& buf )
{
	char tmp[1024];
	SetExtension( tmp, pModelName, ".phy" );
	// No luck, gotta build it	
	// Construct the file name...
	char const* pFileName = ConstructFileName( tmp );
	if (!LoadFile( pFileName, buf ))
	{
		// this is not an error, the model simply has no PHY file
		return false;
	}

	phyheader_t *header = (phyheader_t *)buf.PeekGet();

	if ( header->size != sizeof(*header) || header->solidCount <= 0 )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Gets a vertex position from a strip index
//-----------------------------------------------------------------------------
inline static Vector* PositionFromIndex( mstudiomesh_t* pMesh, OptimizedModel::StripGroupHeader_t* pStripGroup, int i )
{
	OptimizedModel::Vertex_t* pVert = pStripGroup->pVertex( i );
	return pMesh->Position( pVert->origMeshVertID );
}


//-----------------------------------------------------------------------------
// Computes a convex hull from a studio mesh
//-----------------------------------------------------------------------------
static void AddMeshToPolysoup( CPhysPolysoup* pSoup, mstudiomesh_t* pMesh, OptimizedModel::MeshHeader_t* pVtxMesh )
{
	Vector v[3];

	for (int i = 0; i < pVtxMesh->numStripGroups; ++i )
	{
		OptimizedModel::StripGroupHeader_t* pStripGroup = pVtxMesh->pStripGroup(i);

		if (pStripGroup->flags & OptimizedModel::STRIP_IS_TRILIST)
		{
			for (int j = 0; j < pStripGroup->numStrips; ++j )
			{
				OptimizedModel::StripHeader_t* pStrip = pStripGroup->pStrip(i);

				int numTri = pStrip->numIndices / 3;
				unsigned short* pIdx = pStripGroup->pIndex(pStrip->indexOffset);
				for (int k = 0; k < numTri; ++k)
				{
					v[0] = *PositionFromIndex( pMesh, pStripGroup, pIdx[3*k] );
					v[1] = *PositionFromIndex( pMesh, pStripGroup, pIdx[3*k+1] );
					v[2] = *PositionFromIndex( pMesh, pStripGroup, pIdx[3*k+2] );
					s_pPhysCollision->PolysoupAddTriangle( pSoup, v[0], v[1], v[2], 0 );
				}
			}
		}
		else
		{
			for (int j = 0; j < pStripGroup->numStrips; ++j )
			{
				OptimizedModel::StripHeader_t* pStrip = pStripGroup->pStrip(i);

				int numTri = pStrip->numIndices - 2;
				unsigned short* pIdx = pStripGroup->pIndex(pStrip->indexOffset);
				for (int k = 0; k < numTri; ++k)
				{
					v[0] = *PositionFromIndex( pMesh, pStripGroup, pIdx[k] );
					bool winding = ( (k & 0x1) == 0 );
					v[2 - winding] = *PositionFromIndex( pMesh, pStripGroup, pIdx[k+1] );
					v[1 + winding] = *PositionFromIndex( pMesh, pStripGroup, pIdx[k+2] );
					s_pPhysCollision->PolysoupAddTriangle( pSoup, v[0], v[1], v[2], 0 );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Computes a collision model
//-----------------------------------------------------------------------------

CPhysCollide* CreatePhysCollide( studiohdr_t* pStudioHdr, OptimizedModel::FileHeader_t* pVtxHdr )
{
	CPhysPolysoup* pPhysPolysoup = s_pPhysCollision->PolysoupCreate( );
	Assert( pPhysPolysoup );

	for (int body = 0; body < pStudioHdr->numbodyparts; ++body )
	{
		mstudiobodyparts_t *pBodyPart = pStudioHdr->pBodypart( body );
		OptimizedModel::BodyPartHeader_t* pVtxBodyPart = pVtxHdr->pBodyPart(body);

		for( int model = 0; model < pBodyPart->nummodels; ++model )
		{
			mstudiomodel_t *pStudioModel = pBodyPart->pModel( model );
			OptimizedModel::ModelHeader_t* pVtxModel = pVtxBodyPart->pModel(model);
			OptimizedModel::ModelLODHeader_t *pVtxLOD = pVtxModel->pLOD( 0 );

			for( int mesh = 0; mesh < pStudioModel->nummeshes; ++mesh )
			{
				// Add each polygon to the concave object
				// NOTE: This won't work unless the model has been compiled
				// with $staticprop
				mstudiomesh_t *pStudioMesh = pStudioModel->pMesh( mesh );
				OptimizedModel::MeshHeader_t* pVtxMesh = pVtxLOD->pMesh(mesh);

				AddMeshToPolysoup( pPhysPolysoup, pStudioMesh, pVtxMesh );
			}
		}
	}

	// Convert an array of convex elements to a compiled collision model
	// (this deletes the convex elements)
	CPhysCollide* pCollide = s_pPhysCollision->ConvertPolysoupToCollide( pPhysPolysoup );
	s_pPhysCollision->PolysoupDestroy( pPhysPolysoup );
	return pCollide;
}


//-----------------------------------------------------------------------------
// Purpose: Writes a glview text file containing the collision surface in question
// Input  : *pCollide - 
//			*pFilename - 
//-----------------------------------------------------------------------------
void DumpCollideToGlView( vcollide_t *pCollide, const char *pFilename )
{
	if ( !pCollide )
		return;

	Msg("Writing %s...\n", pFilename );

	FILE *fp = fopen( pFilename, "w" );
	for (int i = 0; i < pCollide->solidCount; ++i)
	{
		Vector *outVerts;
		int vertCount = s_pPhysCollision->CreateDebugMesh( pCollide->solids[i], &outVerts );
		int triCount = vertCount / 3;
		int vert = 0;

		unsigned char r = (i & 1) * 64 + 64;
		unsigned char g = (i & 2) * 64 + 64;
		unsigned char b = (i & 4) * 64 + 64;

		float fr = r / 255.0f;
		float fg = g / 255.0f;
		float fb = b / 255.0f;

		for ( int i = 0; i < triCount; i++ )
		{
			fprintf( fp, "3\n" );
			fprintf( fp, "%6.3f %6.3f %6.3f %.2f %.3f %.3f\n", 
				outVerts[vert].x, outVerts[vert].y, outVerts[vert].z, fr, fg, fb );
			vert++;
			fprintf( fp, "%6.3f %6.3f %6.3f %.2f %.3f %.3f\n", 
				outVerts[vert].x, outVerts[vert].y, outVerts[vert].z, fr, fg, fb );
			vert++;
			fprintf( fp, "%6.3f %6.3f %6.3f %.2f %.3f %.3f\n", 
				outVerts[vert].x, outVerts[vert].y, outVerts[vert].z, fr, fg, fb );
			vert++;
		}
		s_pPhysCollision->DestroyDebugMesh( vertCount, outVerts );
	}
	fclose( fp );
}


//-----------------------------------------------------------------------------
// Creates a collision model (based on the render geometry!)
//-----------------------------------------------------------------------------
void CVradStaticPropMgr::CreateCollisionModel( char const* pModelName )
{
	CUtlBuffer buf;
	CUtlBuffer bufvtx;
	CUtlBuffer bufphy;

	int i = m_StaticPropDict.AddToTail( );
	m_StaticPropDict[i].m_pModel = 0;

	if (!LoadStudioModel( pModelName, buf ))
	{
		VectorCopy( vec3_origin, m_StaticPropDict[i].m_Mins );
		VectorCopy( vec3_origin, m_StaticPropDict[i].m_Maxs );
		return;
	}

//	if (!LoadStudioModelVtx( pModelName, bufvtx ))
//		return;

	studiohdr_t* pHdr = (studiohdr_t*)buf.Base();
//	OptimizedModel::FileHeader_t* pVtxHdr = (OptimizedModel::FileHeader_t*)bufvtx.Base();

	VectorCopy( pHdr->hull_min, m_StaticPropDict[i].m_Mins );
	VectorCopy( pHdr->hull_max, m_StaticPropDict[i].m_Maxs );

	if ( LoadStudioCollisionModel( pModelName, bufphy ) )
	{
		phyheader_t header;
		bufphy.Get( &header, sizeof(header) );

		vcollide_t *pCollide = &m_StaticPropDict[i].m_loadedModel;
		s_pPhysCollision->VCollideLoad( pCollide, header.solidCount, (const char *)bufphy.PeekGet(), bufphy.TellPut() - bufphy.TellGet() );
		m_StaticPropDict[i].m_pModel = m_StaticPropDict[i].m_loadedModel.solids[0];

		/*
		static int propNum = 0;
		char tmp[128];
		sprintf( tmp, "staticprop%03d.txt", propNum );
		DumpCollideToGlView( pCollide, tmp );
		++propNum;
		*/
	}
	else
	{
		// mark this as unused
		m_StaticPropDict[i].m_loadedModel.solidCount = 0;

	//	CPhysCollide* pPhys = CreatePhysCollide( pHdr, pVtxHdr );
		m_StaticPropDict[i].m_pModel = ComputeConvexHull( pHdr );
	}
}


//-----------------------------------------------------------------------------
// Unserialize static prop model dictionary
//-----------------------------------------------------------------------------

void CVradStaticPropMgr::UnserializeModelDict( CUtlBuffer& buf )
{
	int count = buf.GetInt();
	while ( --count >= 0 )
	{
		StaticPropDictLump_t lump;
		buf.Get( &lump, sizeof(StaticPropDictLump_t) );
		
		CreateCollisionModel( lump.m_Name );
	}
}

void CVradStaticPropMgr::UnserializeModels( CUtlBuffer& buf )
{
	int count = buf.GetInt();

	m_StaticProps.AddMultipleToTail(count);
	for ( int i = 0; i < count; ++i )
	{
		StaticPropLump_t lump;
		buf.Get( &lump, sizeof(StaticPropLump_t) );
		
		VectorCopy( lump.m_Origin, m_StaticProps[i].m_Origin );
		VectorCopy( lump.m_Angles, m_StaticProps[i].m_Angles );
		m_StaticProps[i].m_ModelIdx = lump.m_PropType;
		m_StaticProps[i].m_Handle = TREEDATA_INVALID_HANDLE;

		// Add the prop to the tree for collision, but only if it isn't
		// marked as not casting a shadow
		if ((lump.m_Flags & STATIC_PROP_NO_SHADOW) == 0)
			InsertPropIntoTree( i );
	}
}

//-----------------------------------------------------------------------------
// Unserialize static props
//-----------------------------------------------------------------------------

void CVradStaticPropMgr::UnserializeStaticProps()
{
	// Unserialize static props, insert them into the appropriate leaves
	GameLumpHandle_t handle = GetGameLumpHandle( GAMELUMP_STATIC_PROPS );
	int size = GameLumpSize( handle );
	if (!size)
		return;

	if (GetGameLump( handle ))
	{
		CUtlBuffer buf( GetGameLump(handle), size );
		UnserializeModelDict( buf );

		// Skip the leaf list data
		int count = buf.GetInt();
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, count * sizeof(StaticPropLeafLump_t) );

		UnserializeModels( buf );
	}
}

//-----------------------------------------------------------------------------
// Level init, shutdown
//-----------------------------------------------------------------------------

void CVradStaticPropMgr::Init()
{
	CreateInterfaceFn physicsFactory = GetPhysicsFactory();
	if ( !physicsFactory )
		Error( "Unable to load vphysics DLL." );
		
	s_pPhysCollision = (IPhysicsCollision *)physicsFactory( VPHYSICS_COLLISION_INTERFACE_VERSION, NULL );
	if( !s_pPhysCollision )
	{
		Error( "Unable to get '%s' for physics interface.", VPHYSICS_COLLISION_INTERFACE_VERSION );
		return;
	}

	m_pBSPTreeData->Init( ToolBSPTree() );

	// Read in static props that have been compiled into the bsp file
	UnserializeStaticProps();
}

void CVradStaticPropMgr::Shutdown()
{
	// Remove all static props from the tree
	for (int i = m_StaticProps.Size(); --i >= 0; )
	{
		RemovePropFromTree( i );
	}

	m_pBSPTreeData->Shutdown();

	m_StaticProps.Purge();
	m_StaticPropDict.Purge();
}

//-----------------------------------------------------------------------------
// Do the collision test
//-----------------------------------------------------------------------------

// IBSPTreeDataEnumerator
bool FASTCALL CVradStaticPropMgr::EnumerateElement( int userId, int context )
{
	CStaticProp& prop = m_StaticProps[userId];

	EnumContext_t* pCtx = (EnumContext_t*)context;

	// Don't test twice
	if (pCtx->m_pPropTested->m_pTested[ userId ] == pCtx->m_pPropTested->m_Enum )
		return true;

	pCtx->m_pPropTested->m_pTested[ userId ] = pCtx->m_pPropTested->m_Enum;

	StaticPropDict_t& dict = m_StaticPropDict[prop.m_ModelIdx];
	
	if ( !IsBoxIntersectingRay( prop.m_mins, prop.m_maxs, pCtx->m_pRay->m_Start, pCtx->m_pRay->m_Delta ) )
		return true;

	// If there is an invalid model file, it has a null entry here.
	if( !dict.m_pModel )
		return false;

	CGameTrace trace;
	pCtx->m_pPropTested->pThreadedCollision->TraceBox( *pCtx->m_pRay, dict.m_pModel, prop.m_Origin, prop.m_Angles, &trace );

	// False means stop iterating. Return false if we hit!
	return (trace.fraction == 1.0);
}


// ISpatialLeafEnumerator
bool CVradStaticPropMgr::EnumerateLeaf( int leaf, int context )
{
	return m_pBSPTreeData->EnumerateElementsInLeaf( leaf, this, context );
}

bool CVradStaticPropMgr::ClipRayToStaticProps( PropTested_t& propTested, Ray_t const& ray )
{
	StartRayTest( propTested );

	EnumContext_t ctx;
	ctx.m_pRay = &ray;
	ctx.m_pPropTested = &propTested;

	// If it got through without a hit, it returns true
	return !m_pBSPTreeData->EnumerateLeavesAlongRay( ray, this, (int)&ctx );
}

bool CVradStaticPropMgr::ClipRayToStaticPropsInLeaf( PropTested_t& propTested, Ray_t const& ray, int leaf )
{
	EnumContext_t ctx;
	ctx.m_pRay = &ray;
	ctx.m_pPropTested = &propTested;

	return !m_pBSPTreeData->EnumerateElementsInLeaf( leaf, this, (int)&ctx );
}

void CVradStaticPropMgr::StartRayTest( PropTested_t& propTested )
{
	if (m_StaticProps.Size() > 0)
	{
		if (propTested.m_pTested == 0)
		{
			propTested.m_pTested = new int[m_StaticProps.Size()];
			memset( propTested.m_pTested, 0, m_StaticProps.Size() * sizeof(int) );
			propTested.m_Enum = 0;
			propTested.pThreadedCollision = s_pPhysCollision->ThreadContextCreate();
		}
		++propTested.m_Enum;
	}
}

