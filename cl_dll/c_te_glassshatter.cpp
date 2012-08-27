//======== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "c_basetempentity.h"
#include "particle_simple3D.h"

#define PI 3.14159265359
#define GLASS_SHARD_MIN_LIFE 1
#define GLASS_SHARD_MAX_LIFE 10
#define GLASS_SHARD_NOISE	 0.3
#define GLASS_SHARD_GRAVITY  500
#define GLASS_SHARD_DAMPING	 0.3

#include "ClientEffectPrecacheSystem.h"

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectGlassShatter )
CLIENTEFFECT_MATERIAL( "effects/fleck_glass1" )
CLIENTEFFECT_MATERIAL( "effects/fleck_glass2" )
CLIENTEFFECT_MATERIAL( "effects/fleck_tile1" )
CLIENTEFFECT_MATERIAL( "effects/fleck_tile2" )
CLIENTEFFECT_REGISTER_END()

//###################################################
// > C_TEShatterSurface
//###################################################
class C_TEShatterSurface : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEShatterSurface, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	C_TEShatterSurface( void );
	~C_TEShatterSurface( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	Vector					m_vecOrigin;
	QAngle					m_vecAngles;
	Vector					m_vecForce;
	Vector					m_vecForcePos;
	float					m_flWidth;
	float					m_flHeight;
	float					m_flShardSize;
	PMaterialHandle			m_pMaterialHandle;
	int						m_nSurfaceType;
	byte					m_uchFrontColor[3];
	byte					m_uchBackColor[3];
};

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
C_TEShatterSurface::C_TEShatterSurface( void )
{
	m_vecOrigin.Init();
	m_vecAngles.Init();
	m_vecForce.Init();
	m_vecForcePos.Init();
	m_flWidth			= 16.0;
	m_flHeight			= 16.0;
	m_flShardSize		= 3;
	m_nSurfaceType		= SHATTERSURFACE_GLASS;
	m_uchFrontColor[0]	= 255;
	m_uchFrontColor[1]	= 255;
	m_uchFrontColor[2]	= 255;
	m_uchBackColor[0]	= 255;
	m_uchBackColor[1]	= 255;
	m_uchBackColor[2]	= 255;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
C_TEShatterSurface::~C_TEShatterSurface()
{
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEShatterSurface::PostDataUpdate( DataUpdateType_t updateType )
{
	CSmartPtr<CSimple3DEmitter> pGlassEmitter = CSimple3DEmitter::Create( "C_TEShatterSurface 1" );
	pGlassEmitter->SetSortOrigin( m_vecOrigin );

	Vector vecColor;
	engine->ComputeLighting( m_vecOrigin, NULL, false, vecColor );

	// HACK: Blend a little toward white to match the materials...
	VectorLerp( vecColor, Vector( 1, 1, 1 ), 0.3, vecColor );

	PMaterialHandle hMaterial1;
	PMaterialHandle hMaterial2;
	if (m_nSurfaceType == SHATTERSURFACE_GLASS)
	{
		hMaterial1 = pGlassEmitter->GetPMaterial( "effects/fleck_glass1" );
		hMaterial2 = pGlassEmitter->GetPMaterial( "effects/fleck_glass2" );
	}
	else
	{
		hMaterial1 = pGlassEmitter->GetPMaterial( "effects/fleck_tile1" );
		hMaterial2 = pGlassEmitter->GetPMaterial( "effects/fleck_tile2" );
	}

	// ---------------------------------------------------
	// Figure out number of particles required to fill space
	// ---------------------------------------------------
	int nNumWide = m_flWidth  / m_flShardSize;
	int nNumHigh = m_flHeight / m_flShardSize;

	Vector vWidthStep,vHeightStep;
	AngleVectors(m_vecAngles,NULL,&vWidthStep,&vHeightStep);
	vWidthStep	*= m_flShardSize;
	vHeightStep *= m_flShardSize;

	// ---------------------
	// Create glass shards
	// ----------------------
	Vector vCurPos = m_vecOrigin;
	vCurPos.x += 0.5*m_flShardSize;
	vCurPos.z += 0.5*m_flShardSize;

	float flMinSpeed = 9999999999;
	float flMaxSpeed = 0;

	for (int width=0;width<nNumWide;width++)
	{
		for (int height=0;height<nNumHigh;height++)
		{
			Particle3D *pParticle;
			if (random->RandomInt(0,1))
			{
				pParticle = (Particle3D *) pGlassEmitter->AddParticle( sizeof(Particle3D), hMaterial1, vCurPos );
			}
			else
			{
				pParticle = (Particle3D *) pGlassEmitter->AddParticle( sizeof(Particle3D), hMaterial2, vCurPos );
			}

			Vector vForceVel = Vector(0,0,0);
			if (random->RandomInt(0, 3) != 0)
			{
				float flForceDistSqr = (vCurPos - m_vecForcePos).LengthSqr();
				vForceVel = m_vecForce;
				if (flForceDistSqr > 0 )
				{
					vForceVel *= ( 40.0f / flForceDistSqr );
				}
			}

			if (pParticle)
			{
				pParticle->m_flLifetime			= 0.0f;
				pParticle->m_flDieTime			= random->RandomFloat(GLASS_SHARD_MIN_LIFE,GLASS_SHARD_MAX_LIFE);
				pParticle->m_vecVelocity		= vForceVel;
				pParticle->m_vecVelocity	   += RandomVector(-25,25);
				pParticle->m_uchSize			= m_flShardSize + random->RandomFloat(-0.5*m_flShardSize,0.5*m_flShardSize);
				pParticle->m_vAngles			= m_vecAngles;
				pParticle->m_flAngSpeed			= random->RandomFloat(-400,400);

				pParticle->m_uchFrontColor[0]	= (int)(m_uchFrontColor[0] * vecColor.x + 0.5f);
				pParticle->m_uchFrontColor[1]	= (int)(m_uchFrontColor[1] * vecColor.y + 0.5f);
				pParticle->m_uchFrontColor[2]	= (int)(m_uchFrontColor[2] * vecColor.z + 0.5f);
				pParticle->m_uchBackColor[0]	= (int)(m_uchBackColor[0] * vecColor.x + 0.5f);
				pParticle->m_uchBackColor[1]	= (int)(m_uchBackColor[1] * vecColor.y + 0.5f);
				pParticle->m_uchBackColor[2]	= (int)(m_uchBackColor[2] * vecColor.z + 0.5f);
			}

			// Keep track of min and max speed for collision detection
			float  flForceSpeed = vForceVel.Length();
			if (flForceSpeed > flMaxSpeed)
			{
				flMaxSpeed = flForceSpeed;
			}
			if (flForceSpeed < flMinSpeed)
			{
				flMinSpeed = flForceSpeed;
			}

			vCurPos += vHeightStep;
		}
		vCurPos	 -= nNumHigh*vHeightStep;
		vCurPos	 += vWidthStep;
	}

	// --------------------------------------------------
	// Set collision parameters
	// --------------------------------------------------
	Vector vMoveDir = m_vecForce;
	VectorNormalize(vMoveDir);

	pGlassEmitter->m_ParticleCollision.Setup( m_vecOrigin, &vMoveDir, GLASS_SHARD_NOISE, 
												flMinSpeed, flMaxSpeed, GLASS_SHARD_GRAVITY, GLASS_SHARD_DAMPING );
}


IMPLEMENT_CLIENTCLASS_EVENT_DT(C_TEShatterSurface, DT_TEShatterSurface, CTEShatterSurface)
	RecvPropVector( RECVINFO(m_vecOrigin)),
	RecvPropVector( RECVINFO(m_vecAngles)),
	RecvPropVector( RECVINFO(m_vecForce)),
	RecvPropVector( RECVINFO(m_vecForcePos)),
	RecvPropFloat( RECVINFO(m_flWidth)),
	RecvPropFloat( RECVINFO(m_flHeight)),
	RecvPropFloat( RECVINFO(m_flShardSize)),
	RecvPropInt( RECVINFO(m_nSurfaceType)),	
	RecvPropInt( RECVINFO(m_uchFrontColor[0])),
	RecvPropInt( RECVINFO(m_uchFrontColor[1])),
	RecvPropInt( RECVINFO(m_uchFrontColor[2])),
	RecvPropInt( RECVINFO(m_uchBackColor[0])),
	RecvPropInt( RECVINFO(m_uchBackColor[1])),
	RecvPropInt( RECVINFO(m_uchBackColor[2])),
END_RECV_TABLE()

void TE_ShatterSurface( IRecipientFilter& filter, float delay,
	const Vector* pos, const QAngle* angle, const Vector* vForce, const Vector* vForcePos, 
	float width, float height, float shardsize, ShatterSurface_t surfacetype,
	int front_r, int front_g, int front_b, int back_r, int back_g, int back_b)
{
	// Major hack to simulate receiving network message
	__g_C_TEShatterSurface.m_vecOrigin = *pos;
	__g_C_TEShatterSurface.m_vecAngles = *angle;
	__g_C_TEShatterSurface.m_vecForce = *vForce;
	__g_C_TEShatterSurface.m_vecForcePos = *vForcePos;
	__g_C_TEShatterSurface.m_flWidth = width;
	__g_C_TEShatterSurface.m_flHeight = height;
	__g_C_TEShatterSurface.m_flShardSize = shardsize;
	__g_C_TEShatterSurface.m_nSurfaceType = surfacetype;
	__g_C_TEShatterSurface.m_uchFrontColor[0] = front_r;
	__g_C_TEShatterSurface.m_uchFrontColor[1] = front_g;
	__g_C_TEShatterSurface.m_uchFrontColor[2] = front_b;
	__g_C_TEShatterSurface.m_uchBackColor[0] = back_r;
	__g_C_TEShatterSurface.m_uchBackColor[1] = back_g;
	__g_C_TEShatterSurface.m_uchBackColor[2] = back_b;

	__g_C_TEShatterSurface.PostDataUpdate( DATA_UPDATE_CREATED );
}

