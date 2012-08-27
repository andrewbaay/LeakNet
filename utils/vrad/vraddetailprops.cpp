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
#include "Bsplib.h"
#include "GameBSPFile.h"
#include "UtlBuffer.h"
#include "UtlVector.h"
#include "CModel.h"
#include "studio.h"
#include "pacifier.h"

#define MAX_LIGHTSTYLES 256

#define NUMVERTEXNORMALS	162
static Vector	s_raddir[NUMVERTEXNORMALS] = 
{
#include "anorms.h"
};

void Vec3toColorRGBExp32( Vector& v, colorRGBExp32 *c );

bool LoadStudioModel( char const* pModelName, CUtlBuffer& buf );


//-----------------------------------------------------------------------------
// Purpose: Writes a glview text file containing the collision surface in question
// Input  : *pCollide - 
//			*pFilename - 
//-----------------------------------------------------------------------------
void DumpRayToGlView( Ray_t const& ray, float dist, Vector* pColor, const char *pFilename )
{
	Vector dir =  ray.m_Delta;
	float len = VectorNormalize(dir);
	if (len < 1e-3)
		return;

	Vector up( 0, 0, 1 );
	Vector crossDir;
	if (fabs(DotProduct(up, dir)) - 1.0f < -1e-3 )
	{
		CrossProduct( dir, up, crossDir );
		VectorNormalize(crossDir);
	}
	else
	{
		up.Init( 0, 1, 0 );
		CrossProduct( dir, up, crossDir );
		VectorNormalize(crossDir);
	}

	Vector end;
	Vector start1, start2;
	VectorMA( ray.m_Start, dist, ray.m_Delta, end );
	VectorMA( ray.m_Start, -2, crossDir, start1 );
	VectorMA( ray.m_Start, 2, crossDir, start2 );

	FileHandle_t fp = g_pFileSystem->Open( pFilename, "a" );
	int vert = 0;
	CmdLib_FPrintf( fp, "3\n" );
	CmdLib_FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", start1.x, start1.y, start1.z,
		pColor->x, pColor->y, pColor->z );
	vert++;
	CmdLib_FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", start2.x, start2.y, start2.z,
		pColor->x, pColor->y, pColor->z );
	vert++;
	CmdLib_FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", end.x, end.y, end.z,
		pColor->x, pColor->y, pColor->z );
	vert++;
	g_pFileSystem->Close( fp );
}


//-----------------------------------------------------------------------------
// This puppy is used to construct the game lumps
//-----------------------------------------------------------------------------
static CUtlVector<DetailPropLightstylesLump_t>	s_DetailPropLightStyleLump;


//-----------------------------------------------------------------------------
// An amount to add to each model to get to the model center
//-----------------------------------------------------------------------------
CUtlVector<Vector> g_ModelCenterOffset;
CUtlVector<Vector> g_SpriteCenterOffset;


//-----------------------------------------------------------------------------
// Finds ambient lights
//-----------------------------------------------------------------------------
static directlight_t* FindAmbientLight()
{
	// find any ambient lights
	directlight_t* dl;
	for (dl = activelights; dl != 0; dl = dl->next)
	{
		if (dl->light.type == emit_skyambient)
		{
			return dl;
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Compute world center of a prop
//-----------------------------------------------------------------------------
static void ComputeWorldCenter( DetailObjectLump_t& prop, Vector& center, Vector& normal )
{
	// Transform the offset into world space
	Vector forward, right;
	AngleVectors( prop.m_Angles, &forward, &right, &normal );
	VectorCopy( prop.m_Origin, center );

	// FIXME: Take orientation into account?
	switch (prop.m_Type )
	{
	case DETAIL_PROP_TYPE_MODEL:
		VectorMA( center, g_ModelCenterOffset[prop.m_DetailModel].x, forward, center );
		VectorMA( center, -g_ModelCenterOffset[prop.m_DetailModel].y, right, center );
		VectorMA( center, g_ModelCenterOffset[prop.m_DetailModel].z, normal, center );
		break;

	case DETAIL_PROP_TYPE_SPRITE:
		Vector vecOffset;
		VectorMultiply( g_SpriteCenterOffset[prop.m_DetailModel], prop.m_flScale, vecOffset );
		VectorMA( center, vecOffset.x, forward, center );
		VectorMA( center, -vecOffset.y, right, center );
		VectorMA( center, vecOffset.z, normal, center );
		break;
	}
}


//-----------------------------------------------------------------------------
// Computes max direct lighting for a single detal prop
//-----------------------------------------------------------------------------
static void ComputeMaxDirectLighting( DetailObjectLump_t& prop, Vector* maxcolor, int iThread )
{
	// The max direct lighting must be along the direction to one
	// of the static lights....

	Vector origin, normal;
	ComputeWorldCenter( prop, origin, normal );

	int cluster = ClusterFromPoint(origin);

	Vector delta;
	CUtlVector< directlight_t* >	lights;
	CUtlVector< Vector >			directions;

	directlight_t* dl;
	for (dl = activelights; dl != 0; dl = dl->next)
	{
		// skyambient doesn't affect dlights..
		if (dl->light.type == emit_skyambient)
			continue;

		// is this lights cluster visible?
		if ( (dl->pvs[ (cluster)>>3] & (1<< (cluster & 7)) ) )
		{
			lights.AddToTail(dl);
			VectorSubtract( dl->light.origin, origin, delta );
			VectorNormalize( delta );
			directions.AddToTail( delta );
		}
	}

	// Find the max illumination
	int i;
	for ( i = 0; i < MAX_LIGHTSTYLES; ++i)
	{
		maxcolor[i].Init(0,0,0);
	}

	// NOTE: See version 10 for a method where we choose a normal based on whichever
	// one produces the maximum possible illumination. This appeared to work better on
	// e3_town, so I'm trying it now; hopefully it'll be good for all cases.
	int j;
	for ( j = 0; j < lights.Count(); ++j)
	{
		float falloff;
		Vector delta;

		dl = lights[j];

		float dot = GatherSampleLight( dl, -1, origin, normal,
			delta, &falloff, iThread );

		if (dot > 0)
		{
			// The first sample is for non-bumped lighting.
			// The other sample are for bumpmapping.
			VectorMA( maxcolor[dl->light.style], falloff * dot, dl->light.intensity, maxcolor[dl->light.style] );
		}
	}
}


//-----------------------------------------------------------------------------
// Computes the ambient term from a particular surface
//-----------------------------------------------------------------------------

static void ComputeAmbientFromSurface( dface_t* pFace, directlight_t* pSkylight, 
									   Vector& radcolor )
{
	texinfo_t* pTex = &texinfo[pFace->texinfo];
	if (pTex)
	{
		// If we hit the sky, use the sky ambient
		if (pTex->flags & SURF_SKY)
		{
			if (pSkylight)
			{
				// add in sky ambient
				VectorDivide( pSkylight->light.intensity, 255.0f, radcolor ); 
			}
		}
		else
		{
			VectorMultiply( radcolor, dtexdata[pTex->texdata].reflectivity, radcolor );
		}
	}
}


//-----------------------------------------------------------------------------
// Computes the lightmap color at a particular point
//-----------------------------------------------------------------------------

static void ComputeLightmapColorFromAverage( dface_t* pFace,
						directlight_t* pSkylight, Vector* pColor )
{
	texinfo_t* pTex = &texinfo[pFace->texinfo];
	if (pTex->flags & SURF_SKY)
	{
		if (pSkylight)
		{
			// add in sky ambient
			Vector amb;
			VectorDivide( pSkylight->light.intensity, 255.0f, amb ); 
			pColor[0] += amb;
		}
		return;
	}

	for (int maps = 0 ; maps < MAXLIGHTMAPS && pFace->styles[maps] != 255 ; ++maps)
	{
		int style = pFace->styles[maps];
		colorRGBExp32* pAvgColor = &pFace->m_AvgLightColor[maps];

		Vector color;
		color[0] = TexLightToLinear( pAvgColor->r, pAvgColor->exponent );
		color[1] = TexLightToLinear( pAvgColor->g, pAvgColor->exponent );
		color[2] = TexLightToLinear( pAvgColor->b, pAvgColor->exponent );

		ComputeAmbientFromSurface( pFace, pSkylight, color );

		pColor[style] += color;
	}
}


//-----------------------------------------------------------------------------
// Returns true if the surface has bumped lightmaps
//-----------------------------------------------------------------------------

static bool SurfHasBumpedLightmaps( dface_t *pSurf )
{
	bool hasBumpmap = false;
	if( ( texinfo[pSurf->texinfo].flags & SURF_BUMPLIGHT ) && 
		( !( texinfo[pSurf->texinfo].flags & SURF_NOLIGHT ) ) )
	{
		hasBumpmap = true;
	}
	return hasBumpmap;
}

//-----------------------------------------------------------------------------
// Computes the lightmap color at a particular point
//-----------------------------------------------------------------------------

static void ComputeLightmapColorDisplacement( dface_t* pFace,
				directlight_t* pSkylight, Vector2D const& luv, Vector* pColor )
{
	// luv is in the space of the accumulated lightmap page; we need to convert
	// it to be in the space of the surface
	int ds = (int)(luv.x + 0.5f);
	int dt = (int)(luv.y + 0.5f);

	int smax = ( pFace->m_LightmapTextureSizeInLuxels[0] ) + 1;
	int tmax = ( pFace->m_LightmapTextureSizeInLuxels[1] ) + 1;
	int offset = smax * tmax;
	if ( SurfHasBumpedLightmaps( pFace ) )
		offset *= ( NUM_BUMP_VECTS + 1 );

	colorRGBExp32* pLightmap = (colorRGBExp32*)&dlightdata[pFace->lightofs];
	pLightmap += dt * smax + ds;
	for (int maps = 0 ; maps < MAXLIGHTMAPS && pFace->styles[maps] != 255 ; ++maps)
	{
		int style = pFace->styles[maps];

		Vector color;
		color[0] = TexLightToLinear( pLightmap->r, pLightmap->exponent );
		color[1] = TexLightToLinear( pLightmap->g, pLightmap->exponent );
		color[2] = TexLightToLinear( pLightmap->b, pLightmap->exponent );

		ComputeAmbientFromSurface( pFace, pSkylight, color );
		pColor[style] += color;

		pLightmap += offset;
	}
}


//-----------------------------------------------------------------------------
// Tests a particular node
//-----------------------------------------------------------------------------

class CLightSurface : public IBSPNodeEnumerator
{
public:
	CLightSurface() : m_pSurface(0), m_HitFrac(1.0f) {}

	// call back with a node and a context
	bool EnumerateNode( int node, Ray_t const& ray, float f, int context )
	{
		dface_t* pSkySurface = 0;

		// Compute the actual point
		Vector pt;
		VectorMA( ray.m_Start, f, ray.m_Delta, pt );

		dnode_t* pNode = &dnodes[node];
		dface_t* pFace = &dfaces[pNode->firstface];
		for (int i=0 ; i < pNode->numfaces ; ++i, ++pFace)
		{
			// Don't take into account faces that are int a leaf
			if ( !pFace->onNode )
				continue;

			// Don't test displacement faces
			if ( pFace->dispinfo != -1 )
				continue;

			texinfo_t* pTex = &texinfo[pFace->texinfo];

			// Don't immediately return when we hit sky; 
			// we may actually hit another surface
			if (pTex->flags & SURF_SKY)
			{
				if (TestPointAgainstSkySurface( pt, pFace ))
				{
					pSkySurface = pFace;
				}

				continue;
			}

			if (TestPointAgainstSurface( pt, pFace, pTex ))
			{
				m_HitFrac = f;
				m_pSurface = pFace;
				return false;
			}
		}

		// if we hit a sky surface, return it
		m_pSurface = pSkySurface;
		return (m_pSurface == 0);
	}

	// call back with a leaf and a context
	virtual bool EnumerateLeaf( int leaf, Ray_t const& ray, float start, float end, int context )
	{
		bool hit = false;
		dleaf_t* pLeaf = &dleafs[leaf];
		dface_t* pFace = &dfaces[pLeaf->firstleafface];
		for (int i=0 ; i < pLeaf->numleaffaces ; ++i, ++pFace)
		{
			// Don't test displacement faces; we need to check another list
			if ( pFace->dispinfo != -1 )
				continue;

			// Don't take into account faces that are on a node
			if ( pFace->onNode )
				continue;

			// Find intersection point against detail brushes
			texinfo_t* pTex = &texinfo[pFace->texinfo];

			dplane_t* pPlane = &dplanes[pFace->planenum];

			// Backface cull...
			if (DotProduct( pPlane->normal, ray.m_Delta ) > 0)
				continue;

			float startDotN = DotProduct( ray.m_Start, pPlane->normal );
			float deltaDotN = DotProduct( ray.m_Delta, pPlane->normal );

			float front = startDotN + start * deltaDotN - pPlane->dist;
			float back = startDotN + end * deltaDotN - pPlane->dist;
			
			int side = front < 0;

			// Blow it off if it doesn't split the plane...
			if ( (back < 0) == side )
				continue;

			// Don't test a surface that is farther away from the closest found intersection
			float f = front / (front-back);
			float mid = start * (1.0f - f) + end * f;
			if (mid >= m_HitFrac)
				continue;

			Vector pt;
			VectorMA( ray.m_Start, mid, ray.m_Delta, pt );

			if (TestPointAgainstSurface( pt, pFace, pTex ))
			{
				m_HitFrac = mid;
				m_pSurface = pFace;
				hit = true;
			}
		}

		// Now try to clip against all displacements in the leaf
		float dist;
		Vector2D luxelCoord;
		StaticDispMgr()->ClipRayToDispInLeaf( s_DispTested, ray, leaf, dist, pFace, luxelCoord );
		if (dist < m_HitFrac)
		{
			m_HitFrac = dist;
			m_pSurface = pFace;
			Vector2DCopy( luxelCoord, m_LuxelCoord );
			hit = true;
		}
		return !hit;
	}

	bool FindIntersection( Ray_t const& ray )
	{
		StaticDispMgr()->StartRayTest( s_DispTested );
		return !EnumerateNodesAlongRay( ray, this, 0 );
	}

private:
	bool TestPointAgainstSurface( Vector const& pt, dface_t* pFace, texinfo_t* pTex )
	{
		// no lightmaps on this surface? punt...
		// FIXME: should be water surface?
		if (pTex->flags & SURF_NOLIGHT)
			return false;	
		
		// See where in lightmap space our intersection point is 
		float s, t;
		s = DotProduct (pt.Base(), pTex->lightmapVecsLuxelsPerWorldUnits[0]) + 
			pTex->lightmapVecsLuxelsPerWorldUnits[0][3];
		t = DotProduct (pt.Base(), pTex->lightmapVecsLuxelsPerWorldUnits[1]) + 
			pTex->lightmapVecsLuxelsPerWorldUnits[1][3];

		// Not in the bounds of our lightmap? punt...
		if( s < pFace->m_LightmapTextureMinsInLuxels[0] || t < pFace->m_LightmapTextureMinsInLuxels[1] )
			return false;	
		
		// assuming a square lightmap (FIXME: which ain't always the case),
		// lets see if it lies in that rectangle. If not, punt...
		float ds = s - pFace->m_LightmapTextureMinsInLuxels[0];
		float dt = t - pFace->m_LightmapTextureMinsInLuxels[1];
		if( ds > pFace->m_LightmapTextureSizeInLuxels[0] || dt > pFace->m_LightmapTextureSizeInLuxels[1] )
			return false;	

		return true;
	}

	bool TestPointAgainstSkySurface( Vector const &pt, dface_t *pFace )
	{
		// Create sky face winding.
		winding_t *pWinding = WindingFromFace( pFace, Vector( 0.0f, 0.0f, 0.0f ) );

		// Test point in winding. (Since it is at the node, it is in the plane.)
		return PointInWinding( pt, pWinding );
	}


public:
	dface_t* m_pSurface;
	float	m_HitFrac;
	Vector2D	m_LuxelCoord;
	static DispTested_t s_DispTested;
};

DispTested_t CLightSurface::s_DispTested;

//-----------------------------------------------------------------------------
// Computes lighting for a single detal prop
//-----------------------------------------------------------------------------

static void ComputeAmbientLighting( DetailObjectLump_t& prop, Vector* color )
{
	Vector origin, normal;
	ComputeWorldCenter( prop, origin, normal );

	// NOTE: I'm not dealing with shadow-casting static props here
	// This is for speed, although we can add it if it turns out to
	// be important

	// find any ambient lights
	directlight_t* pSkylight = FindAmbientLight();

	// sample world by casting N rays distributed across a sphere
	Vector radcolor[NUMVERTEXNORMALS];
	Vector upend;

	int j;
	for ( j = 0; j < MAX_LIGHTSTYLES; ++j)
	{
		color[j].Init( 0,0,0 );
	}

	for (int i = 0; i < NUMVERTEXNORMALS; i++)
	{
		VectorMA( origin, COORD_EXTENT * 1.74, s_raddir[i], upend );

		// Now that we've got a ray, see what surface we've hit
		Ray_t ray;
		ray.Init( origin, upend, vec3_origin, vec3_origin );

		CLightSurface surfEnum;
		if (!surfEnum.FindIntersection( ray ))
			continue;

		// This is the faster path; it looks slightly different though
		if (surfEnum.m_pSurface->dispinfo == -1)
		{
			ComputeLightmapColorFromAverage( surfEnum.m_pSurface, pSkylight, color );
		}
		else
		{
			ComputeLightmapColorDisplacement( surfEnum.m_pSurface, pSkylight, surfEnum.m_LuxelCoord, color );
		}

//		DumpRayToGlView( ray, surfEnum.m_HitFrac, &color[0], "test.out" );
	}

	for ( j = 0; j < MAX_LIGHTSTYLES; ++j)
	{
		VectorMultiply( color[j], 255.0f / (float)NUMVERTEXNORMALS, color[j] );
	}
}


//-----------------------------------------------------------------------------
// Computes lighting for a single detal prop
//-----------------------------------------------------------------------------

static void ComputeLighting( DetailObjectLump_t& prop, int iThread )
{
	// We're going to take the maximum of the ambient lighting and 
	// the strongest directional light. This works because we're assuming
	// the props will have built-in faked lighting.

	Vector directColor[MAX_LIGHTSTYLES];
	Vector ambColor[MAX_LIGHTSTYLES];

	// Get the max influence of all direct lights
	ComputeMaxDirectLighting( prop, directColor, iThread );

	// Get the ambient lighting + lightstyles
	ComputeAmbientLighting( prop, ambColor );

	// Base lighting
	Vector totalColor;
	VectorAdd( directColor[0], ambColor[0], totalColor );
	Vec3toColorRGBExp32( totalColor, &prop.m_Lighting );

	bool hasLightstyles = false;

	// lightstyles
	for (int i = 1; i < MAX_LIGHTSTYLES; ++i )
	{
		VectorAdd( directColor[i], ambColor[i], totalColor );
		totalColor *= 0.5f;

		if ((totalColor[0] != 0.0f) || (totalColor[1] != 0.0f) ||
			(totalColor[2] != 0.0f) )
		{
			if (!hasLightstyles)
			{
				prop.m_LightStyles = s_DetailPropLightStyleLump.Size();
				prop.m_LightStyleCount = 0;
				hasLightstyles = true;
			}

			int j = s_DetailPropLightStyleLump.AddToTail();
			Vec3toColorRGBExp32( totalColor, &s_DetailPropLightStyleLump[j].m_Lighting );
			s_DetailPropLightStyleLump[j].m_Style = i;
			++prop.m_LightStyleCount;
		}
	}
}


//-----------------------------------------------------------------------------
// Unserialization
//-----------------------------------------------------------------------------
static void UnserializeModelDict( CUtlBuffer& buf )
{
	// Get origin offset for each model...
	int count = buf.GetInt();
	while ( --count >= 0 )
	{
		DetailObjectDictLump_t lump;
		buf.Get( &lump, sizeof(DetailObjectDictLump_t) );
		
		int i = g_ModelCenterOffset.AddToTail();

		CUtlBuffer mdlbuf;
		if (LoadStudioModel( lump.m_Name, mdlbuf ))
		{
			studiohdr_t* pHdr = (studiohdr_t*)mdlbuf.Base();
			VectorAdd( pHdr->hull_min, pHdr->hull_max, g_ModelCenterOffset[i] );
			g_ModelCenterOffset[i] *= 0.5f;
		}
		else
		{
			g_ModelCenterOffset[i].Init(0,0,0);
		}
	}
}

static void UnserializeSpriteDict( CUtlBuffer& buf )
{
	// Get origin offset for each model...
	int count = buf.GetInt();
	while ( --count >= 0 )
	{
		DetailSpriteDictLump_t lump;
		buf.Get( &lump, sizeof(DetailSpriteDictLump_t) );
		
		// For these sprites, x goes out the front, y right, z up
		int i = g_SpriteCenterOffset.AddToTail();
		g_SpriteCenterOffset[i].x = 0.0f;
		g_SpriteCenterOffset[i].y = lump.m_LR.x + lump.m_UL.x;
		g_SpriteCenterOffset[i].z = lump.m_LR.y + lump.m_UL.y;
		g_SpriteCenterOffset[i] *= 0.5f;
	}
}


//-----------------------------------------------------------------------------
// Unserializes the detail props
//-----------------------------------------------------------------------------
static int UnserializeDetailProps( DetailObjectLump_t*& pProps )
{
	GameLumpHandle_t handle = GetGameLumpHandle( GAMELUMP_DETAIL_PROPS );

	if (GetGameLumpVersion(handle) != GAMELUMP_DETAIL_PROPS_VERSION)
		return 0;

	// Unserialize
	CUtlBuffer buf( GetGameLump(handle), GameLumpSize( handle ) );

	UnserializeModelDict( buf );
	UnserializeSpriteDict( buf );

	// Now we're pointing to the detail prop data
	// This actually works because the scope of the game lump data
	// is global and the buf was just pointing to it.
	int count = buf.GetInt();
	if (count)
		pProps = (DetailObjectLump_t*)buf.PeekGet();
	else
		pProps = 0;
	return count;
}


//-----------------------------------------------------------------------------
// Writes the detail lighting lump
//-----------------------------------------------------------------------------
static void WriteDetailLightingLump()
{
	GameLumpHandle_t handle = GetGameLumpHandle(GAMELUMP_DETAIL_PROP_LIGHTING);
	if (handle != InvalidGameLump())
		DestroyGameLump(handle);
	int lightsize = s_DetailPropLightStyleLump.Size() * sizeof(DetailPropLightstylesLump_t);
	int lumpsize = lightsize + sizeof(int);

	handle = CreateGameLump( GAMELUMP_DETAIL_PROP_LIGHTING, lumpsize, 0, GAMELUMP_DETAIL_PROP_LIGHTING_VERSION );

	// Serialize the data
	CUtlBuffer buf( GetGameLump(handle), lumpsize );
	buf.PutInt( s_DetailPropLightStyleLump.Size() );
	if (lightsize)
		buf.Put( s_DetailPropLightStyleLump.Base(), lightsize );
}


//-----------------------------------------------------------------------------
// Computes lighting for the detail props
//-----------------------------------------------------------------------------
void ComputeDetailPropLighting( int iThread )
{
	// illuminate them all
	DetailObjectLump_t* pProps;
	int count = UnserializeDetailProps( pProps );
	if (!count)
		return;

	// Needed for our computations
	BuildExponentTable();

	StartPacifier("Computing detail prop lighting : ");

	for (int i = 0; i < count; ++i)
	{
		UpdatePacifier( (float)i / (float)count );
		ComputeLighting( pProps[i], iThread );
	}

	// Write detail prop lightstyle lump...
	WriteDetailLightingLump();
	EndPacifier( true );
}
