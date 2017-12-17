//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "view.h"
#include "c_tracer.h"
#include "dlight.h"
#include "ClientEffectPrecacheSystem.h"
#include "FX_Sparks.h"
#include "iefx.h"
#include "c_te_effect_dispatch.h"
#include "tier0/vprof.h"

//Precahce the effects
CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectSparks )
CLIENTEFFECT_MATERIAL( "effects/spark" )
CLIENTEFFECT_MATERIAL( "effects/energysplash" )
CLIENTEFFECT_MATERIAL( "effects/energyball" )
CLIENTEFFECT_MATERIAL( "sprites/rico1" )
CLIENTEFFECT_MATERIAL( "sprites/rico1_noz" )
CLIENTEFFECT_MATERIAL( "sprites/blueflare1" )
CLIENTEFFECT_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool EffectOccluded( const Vector &pos )
{
	trace_t	tr;
	UTIL_TraceLine( pos, MainViewOrigin(), MASK_OPAQUE, NULL, COLLISION_GROUP_NONE, &tr );
	return ( tr.fraction < 1.0f ) ? true : false;
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CTrailParticles::CTrailParticles( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
	m_fFlags			= 0;
	m_flVelocityDampen	= 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Test for surrounding collision surfaces for quick collision testing for the particle system
// Input  : &origin - starting position
//			*dir - direction of movement (if NULL, will do a point emission test in four directions)
//			angularSpread - looseness of the spread
//			minSpeed - minimum speed
//			maxSpeed - maximum speed
//			gravity - particle gravity for the sytem
//			dampen - dampening amount on collisions
//			flags - extra information
//-----------------------------------------------------------------------------
void CTrailParticles::Setup( const Vector &origin, const Vector *direction, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen, int flags )
{
	//Take the flags
	SetFlag( (flags|bitsPARTICLE_TRAIL_COLLIDE) ); //Force this if they've called this function

	//See if we've specified a direction
	m_ParticleCollision.Setup( origin, direction, angularSpread, minSpeed, maxSpeed, gravity, dampen );
}


//-----------------------------------------------------------------------------
// Purpose: Simulate and render the particle in this system
// Input  : *pInParticle - particle to consider
//			*pDraw - drawing utilities
//			&sortKey - sorting key
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTrailParticles::SimulateAndRender( Particle *pInParticle, ParticleDraw *pDraw, float &sortKey )
{
	TrailParticle *pParticle = (TrailParticle *) pInParticle;

	const float	timeDelta = pDraw->GetTimeDelta();

	//Get our remaining time
	float lifePerc = 1.0f - ( pParticle->m_flLifetime / pParticle->m_flDieTime  );
	float scale = (pParticle->m_flLength*lifePerc);

	if ( scale < 0.01f )
		scale = 0.01f;

	Vector	start, delta;

	//NOTE: We need to do everything in screen space
	TransformParticle( g_ParticleMgr.GetModelView(), pParticle->m_Pos, start );
	sortKey = start.z;

	Vector3DMultiply( CurrentWorldToViewMatrix(), pParticle->m_vecVelocity, delta );
	
	float	color[4];
	float	ramp;
	
	ramp = ( m_fFlags & bitsPARTICLE_TRAIL_FADE ) ? ( 1.0f - ( pParticle->m_flLifetime / pParticle->m_flDieTime  ) ) : 1.0f;

	color[0] = pParticle->m_flColor[0]*ramp;
	color[1] = pParticle->m_flColor[1]*ramp;
	color[2] = pParticle->m_flColor[2]*ramp;
	color[3] = pParticle->m_flColor[3]*ramp;

	float	flLength = (pParticle->m_vecVelocity * scale).Length();//( delta - pos ).Length();
	float	flWidth	 = ( flLength < pParticle->m_flWidth ) ? flLength : pParticle->m_flWidth;

	//See if we should fade
	Tracer_Draw( pDraw, start, (delta*scale), flWidth, color );

	//Turn off collision if we're not told to do it
	if (( m_fFlags & bitsPARTICLE_TRAIL_COLLIDE )==false)
	{
		m_ParticleCollision.ClearActivePlanes();
	}

	//Simulate the movement with collision
	trace_t trace;
	m_ParticleCollision.MoveParticle( pParticle->m_Pos, pParticle->m_vecVelocity, NULL, timeDelta, &trace );

	//Laterally dampen if asked to do so
	if ( m_fFlags & bitsPARTICLE_TRAIL_VELOCITY_DAMPEN )
	{
		float attenuation = 1.0f - (timeDelta * m_flVelocityDampen);

		if ( attenuation < 0.0f )
			attenuation = 0.0f;

		//Laterally dampen
		pParticle->m_vecVelocity[0] *= attenuation;
		pParticle->m_vecVelocity[1] *= attenuation;
		pParticle->m_vecVelocity[2] *= attenuation;
	}

	//Should this particle die?
	pParticle->m_flLifetime += timeDelta;

	if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Electric spark
// Input  : &pos - origin point of effect
//-----------------------------------------------------------------------------
#define	SPARK_ELECTRIC_SPREAD	0.0f
#define	SPARK_ELECTRIC_MINSPEED	64.0f
#define	SPARK_ELECTRIC_MAXSPEED	300.0f
#define	SPARK_ELECTRIC_GRAVITY	800.0f
#define	SPARK_ELECTRIC_DAMPEN	0.3f

void FX_ElectricSpark( const Vector &pos, int nMagnitude, int nTrailLength, const Vector *vecDir )
{
	VPROF_BUDGET( "FX_ElectricSpark", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	CSmartPtr<CTrailParticles> pSparkEmitter	= CTrailParticles::Create( "FX_ElectricSpark 1" );

	if ( !pSparkEmitter )
	{
		Assert(0);
		return;
	}

	PMaterialHandle hMaterial	= pSparkEmitter->GetPMaterial( "effects/spark" );

	//Setup our collision information
	pSparkEmitter->Setup( (Vector &) pos, 
							NULL, 
							SPARK_ELECTRIC_SPREAD, 
							SPARK_ELECTRIC_MINSPEED, 
							SPARK_ELECTRIC_MAXSPEED, 
							SPARK_ELECTRIC_GRAVITY, 
							SPARK_ELECTRIC_DAMPEN, 
							bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	pSparkEmitter->SetSortOrigin( pos );

	//
	// Big sparks.
	//
	Vector	dir;
	int		numSparks = nMagnitude * nMagnitude * random->RandomFloat( 2, 4 );

	int i;
	TrailParticle	*pParticle;
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, pos );

		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= nMagnitude * random->RandomFloat( 1.0f, 2.0f );

		dir.Random( -1.0f, 1.0f );
		dir[2] = random->RandomFloat( 0.5f, 1.0f );
	
		if ( vecDir )
		{
			dir += 2 * (*vecDir);
			VectorNormalize( dir );
		}
			
		pParticle->m_flWidth		= random->RandomFloat( 2.0f, 5.0f );
		pParticle->m_flLength		= nTrailLength * random->RandomFloat( 0.02, 0.05f );
		
		pParticle->m_vecVelocity	= dir * random->RandomFloat( SPARK_ELECTRIC_MINSPEED, SPARK_ELECTRIC_MAXSPEED );

		pParticle->m_flColor[0]		= 1.0f;
		pParticle->m_flColor[1]		= 1.0f;
		pParticle->m_flColor[2]		= 1.0f;
		pParticle->m_flColor[3]		= 1.0f;
	}

	//
	// Little sparks
	//
	CSmartPtr<CTrailParticles> pSparkEmitter2	= CTrailParticles::Create( "FX_ElectricSpark 2" );

	if ( !pSparkEmitter2 )
	{
		Assert(0);
		return;
	}

	hMaterial	= pSparkEmitter2->GetPMaterial( "effects/spark" );

	pSparkEmitter2->SetSortOrigin( pos );
	
	pSparkEmitter2->m_ParticleCollision.SetGravity( 400.0f );
	pSparkEmitter2->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	numSparks = nMagnitude * random->RandomInt( 16, 32 );

	// Dump out sparks
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter2->AddParticle( sizeof(TrailParticle), hMaterial, pos );

		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;

		dir.Random( -1.0f, 1.0f );
		if ( vecDir )
		{
			dir += *vecDir;
			VectorNormalize( dir );
		}
		
		pParticle->m_flWidth		= random->RandomFloat( 2.0f, 4.0f );
		pParticle->m_flLength		= nTrailLength * random->RandomFloat( 0.02f, 0.03f );
		pParticle->m_flDieTime		= nMagnitude * random->RandomFloat( 0.1f, 0.2f );
		
		pParticle->m_vecVelocity	= dir * random->RandomFloat( 128, 256 );
		
		pParticle->m_flColor[0]		= 1.0f;
		pParticle->m_flColor[1]		= 1.0f;
		pParticle->m_flColor[2]		= 1.0f;
		pParticle->m_flColor[3]		= 1.0f;
	}


	//
	// Caps
	//
	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_ElectricSpark 3" );
	pSimple->SetSortOrigin( pos );
	
	//
	// Inner glow
	//
	SimpleParticle *sParticle;

	// Only render if not occluded
	if ( EffectOccluded( pos ) == false )
	{
		sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "sprites/rico1_noz" ), pos );
			
		if ( sParticle == NULL )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 0.1f;
		
		sParticle->m_vecVelocity.Init();

		sParticle->m_uchColor[0]	= 255;
		sParticle->m_uchColor[1]	= 255;
		sParticle->m_uchColor[2]	= 255;
		sParticle->m_uchStartAlpha	= 255;
		sParticle->m_uchEndAlpha	= 255;
		sParticle->m_uchStartSize	= nMagnitude * random->RandomInt( 4, 8 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( 1.0f, 4.0f );
		
		sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/yellowflare" ), pos );
			
		if ( sParticle == NULL )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 0.1f;
		
		sParticle->m_vecVelocity.Init();

		float	fColor = random->RandomInt( 8, 16 );
		sParticle->m_uchColor[0]	= fColor;
		sParticle->m_uchColor[1]	= fColor;
		sParticle->m_uchColor[2]	= fColor;
		sParticle->m_uchStartAlpha	= fColor;
		sParticle->m_uchEndAlpha	= fColor;
		sParticle->m_uchStartSize	= nMagnitude * random->RandomInt( 32, 64 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
	}


	//
	// Smoke
	//
	Vector	sOffs;

	sOffs[0] = pos[0] + random->RandomFloat( -4.0f, 4.0f );
	sOffs[1] = pos[1] + random->RandomFloat( -4.0f, 4.0f );
	sOffs[2] = pos[2];

	sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "particle/particle_noisesphere" ), sOffs );
		
	if ( sParticle == NULL )
		return;

	sParticle->m_flLifetime		= 0.0f;
	sParticle->m_flDieTime		= 1.0f;
	
	sParticle->m_vecVelocity.Init();
	
	sParticle->m_vecVelocity[2] = 16.0f;
	
	sParticle->m_vecVelocity[0] = random->RandomFloat( -16.0f, 16.0f );
	sParticle->m_vecVelocity[1] = random->RandomFloat( -16.0f, 16.0f );

	sParticle->m_uchColor[0]	= 255;
	sParticle->m_uchColor[1]	= 255;
	sParticle->m_uchColor[2]	= 200;
	sParticle->m_uchStartAlpha	= random->RandomInt( 16, 32 );
	sParticle->m_uchEndAlpha	= 0;
	sParticle->m_uchStartSize	= random->RandomInt( 4, 8 );
	sParticle->m_uchEndSize		= sParticle->m_uchStartSize*4.0f;
	sParticle->m_flRoll			= random->RandomInt( 0, 360 );
	sParticle->m_flRollDelta	= random->RandomFloat( -2.0f, 2.0f );

	//
	// Dlight
	//

	dlight_t *dl= effects->CL_AllocDlight ( 0 );

	dl->origin	= pos;
	dl->color.r = dl->color.g = dl->color.b = 250;
	dl->radius	= random->RandomFloat(16,32);
	dl->die		= gpGlobals->curtime + 0.001;
}

//-----------------------------------------------------------------------------
// Purpose: Sparks created by scraping metal
// Input  : &position - start
//			&normal - direction of spark travel
//-----------------------------------------------------------------------------

#define	METAL_SCRAPE_MINSPEED	128.0f
#define METAL_SCRAPE_MAXSPEED	512.0f
#define METAL_SCRAPE_SPREAD		0.3f
#define METAL_SCRAPE_GRAVITY	800.0f
#define METAL_SCRAPE_DAMPEN		0.4f

void FX_MetalScrape( Vector &position, Vector &normal )
{
	VPROF_BUDGET( "FX_MetalScrape", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	Vector	offset = position + ( normal * 1.0f );

	CSmartPtr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "FX_MetalScrape 1" );

	if ( !sparkEmitter )
		return;

	sparkEmitter->SetSortOrigin( offset );

	//Setup our collision information
	sparkEmitter->Setup( offset, 
						&normal, 
						METAL_SCRAPE_SPREAD, 
						METAL_SCRAPE_MINSPEED, 
						METAL_SCRAPE_MAXSPEED, 
						METAL_SCRAPE_GRAVITY, 
						METAL_SCRAPE_DAMPEN, 
						bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	int	numSparks = random->RandomInt( 4, 8 );
	
	TrailParticle	*pParticle;
	PMaterialHandle	hMaterial = sparkEmitter->GetPMaterial( "effects/spark" );
	Vector			dir;

	float	length	= 0.06f;

	//Dump out sparks
	for ( int i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;

		float	spreadOfs = random->RandomFloat( 0.0f, 2.0f );

		dir[0] = normal[0] + random->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
		dir[1] = normal[1] + random->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
		dir[2] = normal[2] + random->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
	
		pParticle->m_flWidth		= random->RandomFloat( 2.0f, 5.0f );
		pParticle->m_flLength		= random->RandomFloat( length*0.25f, length );
		pParticle->m_flDieTime		= random->RandomFloat( 2.0f, 2.0f );
		
		pParticle->m_vecVelocity	= dir * random->RandomFloat( (METAL_SCRAPE_MINSPEED*(2.0f-spreadOfs)), (METAL_SCRAPE_MAXSPEED*(2.0f-spreadOfs)) );
		
		pParticle->m_flColor[0]		= 1.0f;
		pParticle->m_flColor[1]		= 1.0f;
		pParticle->m_flColor[2]		= 1.0f;
		pParticle->m_flColor[3]		= 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ricochet spark on metal
// Input  : &position - origin of effect
//			&normal - normal of the surface struck
//-----------------------------------------------------------------------------
#define	METAL_SPARK_SPREAD		0.5f
#define	METAL_SPARK_MINSPEED	128.0f
#define	METAL_SPARK_MAXSPEED	512.0f
#define	METAL_SPARK_GRAVITY		400.0f
#define	METAL_SPARK_DAMPEN		0.25f

void FX_MetalSpark( const Vector &position, const Vector &normal, int iScale )
{
	VPROF_BUDGET( "FX_MetalSpark", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	//
	// Emitted particles
	//

	Vector	offset = position + ( normal * 1.0f );

	CSmartPtr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "FX_MetalSpark 1" );

	if ( !sparkEmitter )
		return;

	//Setup our information
	sparkEmitter->SetSortOrigin( offset );
	sparkEmitter->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );
	sparkEmitter->SetVelocityDampen( 8.0f );
	sparkEmitter->SetGravity( METAL_SPARK_GRAVITY );
	sparkEmitter->SetCollisionDamped( METAL_SPARK_DAMPEN );

	int	numSparks = random->RandomInt( 4, 8 ) * ( iScale * 2 );
	
	TrailParticle	*pParticle;
	PMaterialHandle	hMaterial = sparkEmitter->GetPMaterial( "effects/spark" );
	Vector			dir;

	float	length	= 0.1f;

	//Dump out sparks
	for ( int i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;
		
		if( iScale > 1 && i%3 == 0 )
		{
			// Every third spark goes flying far if we're having a big batch of sparks.
			pParticle->m_flDieTime	= random->RandomFloat( 0.1f, 0.2f ) * iScale * 2;
		}
		else
		{
			pParticle->m_flDieTime	= random->RandomFloat( 0.1f, 0.2f ) * iScale;
		}

		float	spreadOfs = random->RandomFloat( 0.0f, 2.0f );

		dir[0] = normal[0] + random->RandomFloat( -(METAL_SPARK_SPREAD*spreadOfs), (METAL_SPARK_SPREAD*spreadOfs) );
		dir[1] = normal[1] + random->RandomFloat( -(METAL_SPARK_SPREAD*spreadOfs), (METAL_SPARK_SPREAD*spreadOfs) );
		dir[2] = normal[2] + random->RandomFloat( -(METAL_SPARK_SPREAD*spreadOfs), (METAL_SPARK_SPREAD*spreadOfs) );
	
		VectorNormalize( dir );

		pParticle->m_flWidth		= random->RandomFloat( 1.0f, 4.0f );
		pParticle->m_flLength		= random->RandomFloat( length*0.25f, length );
		
		pParticle->m_vecVelocity	= dir * random->RandomFloat( (METAL_SPARK_MINSPEED*(2.0f-spreadOfs)), (METAL_SPARK_MAXSPEED*(2.0f-spreadOfs)) );
		
		pParticle->m_flColor[0]		= 1.0f;
		pParticle->m_flColor[1]		= 1.0f;
		pParticle->m_flColor[2]		= 1.0f;
		pParticle->m_flColor[3]		= 1.0f;
	}

	//
	// Filler
	//

	offset = position + ( normal * 2.0f );

	if ( EffectOccluded( offset ) == false )
	{
		CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_MetalSpark" );
		pSimple->SetSortOrigin( offset );
		
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "sprites/rico1_noz" ), offset );
			
		if ( sParticle == NULL )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 0.1f;
		
		sParticle->m_vecVelocity.Init();

		sParticle->m_uchColor[0]	= 255;
		sParticle->m_uchColor[1]	= 255;
		sParticle->m_uchColor[2]	= 255;
		sParticle->m_uchStartAlpha	= 255;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 2, 4 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize*2;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -2.0f, 2.0f );

		//Glow
		sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/yellowflare" ), offset );
			
		if ( sParticle == NULL )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 0.1f;
		
		sParticle->m_vecVelocity.Init();

		float	fColor = random->RandomInt( 8, 16 );
		sParticle->m_uchColor[0]	= fColor;
		sParticle->m_uchColor[1]	= fColor;
		sParticle->m_uchColor[2]	= fColor;
		sParticle->m_uchStartAlpha	= fColor;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 4, 8 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize*4;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -8.0f, 8.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Spark effect. Nothing but sparks.
// Input  : &pos - origin point of effect
//-----------------------------------------------------------------------------
#define	SPARK_SPREAD	3.0f
#define	SPARK_GRAVITY	800.0f
#define	SPARK_DAMPEN	0.3f

void FX_Sparks( const Vector &pos, int nMagnitude, int nTrailLength, const Vector &vecDir, float flWidth, float flMinSpeed, float flMaxSpeed, char *pSparkMaterial )
{
	VPROF_BUDGET( "FX_Sparks", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	CSmartPtr<CTrailParticles> pSparkEmitter	= CTrailParticles::Create( "FX_Sparks 1" );

	if ( !pSparkEmitter )
	{
		Assert(0);
		return;
	}

	PMaterialHandle hMaterial;
	if ( pSparkMaterial )
	{
		hMaterial = pSparkEmitter->GetPMaterial( pSparkMaterial );
	}
	else
	{
		hMaterial = pSparkEmitter->GetPMaterial( "effects/spark" );
	}

	//Setup our collision information
	pSparkEmitter->Setup( (Vector &) pos, 
							NULL, 
							SPARK_SPREAD, 
							flMinSpeed, 
							flMaxSpeed, 
							SPARK_GRAVITY, 
							SPARK_DAMPEN, 
							bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	pSparkEmitter->SetSortOrigin( pos );

	//
	// Big sparks.
	//
	Vector	dir;
	int		numSparks = nMagnitude * nMagnitude * random->RandomFloat( 2, 4 );

	int i;
	TrailParticle	*pParticle;
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, pos );

		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= nMagnitude * random->RandomFloat( 1.0f, 2.0f );

		float	spreadOfs = random->RandomFloat( 0.0f, 2.0f );
		dir[0] = vecDir[0] + random->RandomFloat( -(SPARK_SPREAD*spreadOfs), (SPARK_SPREAD*spreadOfs) );
		dir[1] = vecDir[1] + random->RandomFloat( -(SPARK_SPREAD*spreadOfs), (SPARK_SPREAD*spreadOfs) );
		dir[2] = vecDir[2] + random->RandomFloat( -(SPARK_SPREAD*spreadOfs), (SPARK_SPREAD*spreadOfs) );
		pParticle->m_vecVelocity	= dir * random->RandomFloat( (flMinSpeed*(2.0f-spreadOfs)), (flMaxSpeed*(2.0f-spreadOfs)) );
			
		pParticle->m_flWidth		= flWidth + random->RandomFloat( 0.0f, 0.5f );
		pParticle->m_flLength		= nTrailLength * random->RandomFloat( 0.02, 0.05f );
		pParticle->m_flColor[0]		= 1.0f;
		pParticle->m_flColor[1]		= 1.0f;
		pParticle->m_flColor[2]		= 1.0f;
		pParticle->m_flColor[3]		= 1.0f;
	}

	//
	// Little sparks
	//
	CSmartPtr<CTrailParticles> pSparkEmitter2	= CTrailParticles::Create( "FX_ElectricSpark 2" );

	if ( !pSparkEmitter2 )
	{
		Assert(0);
		return;
	}

	if ( pSparkMaterial )
	{
		hMaterial = pSparkEmitter->GetPMaterial( pSparkMaterial );
	}
	else
	{
		hMaterial = pSparkEmitter->GetPMaterial( "effects/spark" );
	}

	pSparkEmitter2->SetSortOrigin( pos );
	
	pSparkEmitter2->m_ParticleCollision.SetGravity( 400.0f );
	pSparkEmitter2->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	numSparks = nMagnitude * random->RandomInt( 4, 8 );

	// Dump out sparks
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter2->AddParticle( sizeof(TrailParticle), hMaterial, pos );

		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;

		dir.Random( -1.0f, 1.0f );
		dir += vecDir;
		VectorNormalize( dir );
		
		pParticle->m_flWidth		= (flWidth * 0.25) + random->RandomFloat( 0.0f, 0.5f );
		pParticle->m_flLength		= nTrailLength * random->RandomFloat( 0.02f, 0.03f );
		pParticle->m_flDieTime		= nMagnitude * random->RandomFloat( 0.3f, 0.5f );
		
		pParticle->m_vecVelocity	= dir * random->RandomFloat( flMinSpeed, flMaxSpeed );
		
		pParticle->m_flColor[0]		= 1.0f;
		pParticle->m_flColor[1]		= 1.0f;
		pParticle->m_flColor[2]		= 1.0f;
		pParticle->m_flColor[3]		= 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Energy splash for plasma/beam weapon impacts
// Input  : &pos - origin point of effect
//-----------------------------------------------------------------------------
#define	ENERGY_SPLASH_SPREAD	0.7f
#define	ENERGY_SPLASH_MINSPEED	128.0f
#define	ENERGY_SPLASH_MAXSPEED	160.0f
#define	ENERGY_SPLASH_GRAVITY	800.0f
#define	ENERGY_SPLASH_DAMPEN	0.3f

void FX_EnergySplash( const Vector &pos, const Vector &normal, bool bExplosive )
{
	VPROF_BUDGET( "FX_EnergySplash", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	Vector	offset = pos + ( normal * 2.0f );

	CSmartPtr<CTrailParticles> pSparkEmitter	= CTrailParticles::Create( "FX_EnergySplash 1" );
	CSmartPtr<CFleckParticles> pFleckEmitter	= CFleckParticles::Create( "FX_EnergySplash 2" );

	if ( !pSparkEmitter || !pFleckEmitter )
	{
		Assert(0);
		return;
	}

	PMaterialHandle hMaterial	= pSparkEmitter->GetPMaterial( "effects/energysplash" );

	//Setup our collision information
	pSparkEmitter->Setup(	offset, 
							&normal, 
							ENERGY_SPLASH_SPREAD, 
							ENERGY_SPLASH_MINSPEED, 
							ENERGY_SPLASH_MAXSPEED, 
							ENERGY_SPLASH_GRAVITY, 
							ENERGY_SPLASH_DAMPEN, 
							bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	pSparkEmitter->SetSortOrigin( offset );

	Vector	dir;
	int		numSparks = random->RandomInt( 2, 4 );

	TrailParticle	*pParticle;

	// Dump out sparks
	int i;
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( pParticle == NULL )
			break;

		pParticle->m_flLifetime	= 0.0f;

		pParticle->m_flWidth		= random->RandomFloat( 5.0f, 10.0f );
		pParticle->m_flLength		= random->RandomFloat( 0.05, 0.1f );
		pParticle->m_flDieTime		= random->RandomFloat( 1.0f, 2.0f );
		
		dir[0] = normal[0] + random->RandomFloat( -ENERGY_SPLASH_SPREAD, ENERGY_SPLASH_SPREAD );
		dir[1] = normal[1] + random->RandomFloat( -ENERGY_SPLASH_SPREAD, ENERGY_SPLASH_SPREAD );
		dir[2] = normal[2] + random->RandomFloat( -ENERGY_SPLASH_SPREAD, ENERGY_SPLASH_SPREAD );
		pParticle->m_vecVelocity	= dir * random->RandomFloat( ENERGY_SPLASH_MINSPEED, ENERGY_SPLASH_MAXSPEED );

		pParticle->m_flColor[0]		= 1.0f;
		pParticle->m_flColor[1]		= 1.0f;
		pParticle->m_flColor[2]		= 1.0f;
		pParticle->m_flColor[3]		= 1.0f;
	}


	//Dump out some energy bizalls
	hMaterial = pFleckEmitter->GetPMaterial( "effects/energyball" );
	numSparks = random->RandomInt( 2, 4 );
	FleckParticle *pBallParticle;

	pFleckEmitter->Setup(	offset, 
							&normal, 
							ENERGY_SPLASH_SPREAD, 
							ENERGY_SPLASH_MINSPEED, 
							ENERGY_SPLASH_MAXSPEED, 
							ENERGY_SPLASH_GRAVITY, 
							ENERGY_SPLASH_DAMPEN ); 

	pFleckEmitter->SetSortOrigin( offset );

	for ( i = 0; i < numSparks; i++ )
	{
		pBallParticle = (FleckParticle *) pFleckEmitter->AddParticle( sizeof(FleckParticle), hMaterial, offset );

		if ( pBallParticle == NULL )
			break;

		pBallParticle->m_flLifetime	= 0.0f;
		pBallParticle->m_flDieTime = random->RandomFloat( 1.0f, 2.0f );
		
		dir[0] = normal[0] + random->RandomFloat( -ENERGY_SPLASH_SPREAD, ENERGY_SPLASH_SPREAD );
		dir[1] = normal[1] + random->RandomFloat( -ENERGY_SPLASH_SPREAD, ENERGY_SPLASH_SPREAD );
		dir[2] = normal[2] + random->RandomFloat( -ENERGY_SPLASH_SPREAD, ENERGY_SPLASH_SPREAD );
		pBallParticle->m_vecVelocity	= dir * random->RandomFloat( ENERGY_SPLASH_MINSPEED, ENERGY_SPLASH_MAXSPEED );

		pBallParticle->m_uchColor[0]	= 255;
		pBallParticle->m_uchColor[1]	= 255;
		pBallParticle->m_uchColor[2]	= 255;

		pBallParticle->m_uchSize = random->RandomFloat( 1,3 );
	}

	//
	// Little sparks
	//

	CSmartPtr<CTrailParticles> pSparkEmitter2	= CTrailParticles::Create( "FX_EnergySplash 3" );

	if ( !pSparkEmitter2 )
	{
		Assert(0);
		return;
	}

	hMaterial	= pSparkEmitter2->GetPMaterial( "effects/energysplash" );

	pSparkEmitter2->SetSortOrigin( offset );
	pSparkEmitter2->m_ParticleCollision.SetGravity( 400.0f );
	pSparkEmitter2->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	numSparks = random->RandomInt( 8, 16 );

	//Dump out sparks
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter2->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( pParticle == NULL )
			break;

		pParticle->m_flLifetime	= 0.0f;

		pParticle->m_flWidth		= random->RandomFloat( 2.0f, 4.0f );
		pParticle->m_flLength		= random->RandomFloat( 0.02f, 0.03f );
		pParticle->m_flDieTime		= random->RandomFloat( 0.1f, 0.2f );
		
		dir[0] = normal[0] + random->RandomFloat( -ENERGY_SPLASH_SPREAD, ENERGY_SPLASH_SPREAD );
		dir[1] = normal[1] + random->RandomFloat( -ENERGY_SPLASH_SPREAD, ENERGY_SPLASH_SPREAD );
		dir[2] = normal[2] + random->RandomFloat( -ENERGY_SPLASH_SPREAD, ENERGY_SPLASH_SPREAD );
		pParticle->m_vecVelocity	= dir * random->RandomFloat( ENERGY_SPLASH_MINSPEED, ENERGY_SPLASH_MAXSPEED );
		
		pParticle->m_flColor[0]		= 1.0f;
		pParticle->m_flColor[1]		= 1.0f;
		pParticle->m_flColor[2]		= 1.0f;
		pParticle->m_flColor[3]		= 1.0f;
	}

	//
	// Caps
	//

	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_EnergySplash 4" );
	pSimple->SetSortOrigin( offset );

	//
	// Inner glow
	//
	SimpleParticle *sParticle;
	if ( !EffectOccluded( offset ) )
	{
		sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "sprites/rico1_noz" ), offset );		
		if ( sParticle )
		{
			sParticle->m_flLifetime		= 0.0f;
			sParticle->m_flDieTime		= 0.2f;
			
			sParticle->m_vecVelocity.Init();
			
			sParticle->m_uchColor[0]	= 128;
			sParticle->m_uchColor[1]	= 128;
			sParticle->m_uchColor[2]	= 255;
			sParticle->m_uchStartAlpha	= random->RandomInt( 128, 255 );
			sParticle->m_uchEndAlpha	= 0;
			sParticle->m_uchStartSize	= random->RandomInt( 32, 64 );
			sParticle->m_uchEndSize		= 0;
			sParticle->m_flRoll			= random->RandomInt( 0, 360 );
			sParticle->m_flRollDelta	= random->RandomFloat( 1.0f, 4.0f );
		}
	}

	//
	// Smoke
	//

	Vector	sOffs;

	sOffs[0] = offset[0] + random->RandomFloat( -4.0f, 4.0f );
	sOffs[1] = offset[1] + random->RandomFloat( -4.0f, 4.0f );
	sOffs[2] = offset[2];

	sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "particle/particle_noisesphere" ), sOffs );		
	if ( sParticle )
	{
		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 1.0f;
		
		sParticle->m_vecVelocity.Init();
		
		sParticle->m_vecVelocity[2] = 16.0f;
		
		sParticle->m_vecVelocity[0] = random->RandomFloat( -16.0f, 16.0f );
		sParticle->m_vecVelocity[1] = random->RandomFloat( -16.0f, 16.0f );
		
		sParticle->m_uchColor[0]	= 140;
		sParticle->m_uchColor[1]	= 170;
		sParticle->m_uchColor[2]	= 220;
		sParticle->m_uchStartAlpha	= random->RandomInt( 64, 96 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 4, 8 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize*4.0f;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -2.0f, 2.0f );
	}

	//
	// Dlight
	//

	dlight_t *dl= effects->CL_AllocDlight ( 0 );

	dl->origin	= offset;
	dl->color.r = dl->color.g = dl->color.b = 250;
	dl->radius	= random->RandomFloat(16,32);
	dl->die		= gpGlobals->curtime + 0.001;


	// If it's an explosive splash, make some big hacky effects
	if ( bExplosive )
	{
		Vector dir;
		Vector normal( 0,0,1 );

		CSmartPtr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "FX_EnergySplash 4" );
		pSmokeEmitter->SetSortOrigin( offset );
		PMaterialHandle	hSphereMaterial = pSmokeEmitter->GetPMaterial( "particle/particle_noisesphere" );;
		SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, offset );
		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= 0.25;
		pParticle->m_uchStartSize	= 32;
		pParticle->m_uchEndSize		= 128;
		dir[0] = normal[0] + random->RandomFloat( -0.4f, 0.4f );
		dir[1] = normal[1] + random->RandomFloat( -0.4f, 0.4f );
		dir[2] = normal[2] + random->RandomFloat( 0, 0.6f );
		pParticle->m_vecVelocity = dir * random->RandomFloat( 2.0f, 24.0f );
		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_flRoll			= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -1, 1 );
		pParticle->m_uchColor[0] = 128;
		pParticle->m_uchColor[1] = 255;
		pParticle->m_uchColor[2] = 128;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Micro-Explosion effect
// Input  : &position - origin of effect
//			&normal - normal of the surface struck
//-----------------------------------------------------------------------------

#define	MICRO_EXPLOSION_MINSPEED	100.0f
#define MICRO_EXPLOSION_MAXSPEED	150.0f
#define MICRO_EXPLOSION_SPREAD		1.0f
#define MICRO_EXPLOSION_GRAVITY		0.0f
#define MICRO_EXPLOSION_DAMPEN		0.4f

void FX_MicroExplosion( Vector &position, Vector &normal )
{
	VPROF_BUDGET( "FX_MicroExplosion", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	Vector	offset = position + ( normal * 2.0f );

	CSmartPtr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "FX_MicroExplosion 1" );

	if ( !sparkEmitter )
		return;

	sparkEmitter->SetSortOrigin( offset );

	//Setup our collision information
	sparkEmitter->Setup( offset, 
						&normal, 
						MICRO_EXPLOSION_SPREAD, 
						MICRO_EXPLOSION_MINSPEED, 
						MICRO_EXPLOSION_MAXSPEED, 
						MICRO_EXPLOSION_GRAVITY, 
						MICRO_EXPLOSION_DAMPEN, 
						bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	int	numSparks = random->RandomInt( 8, 16 );
	
	TrailParticle	*pParticle;
	PMaterialHandle	hMaterial = sparkEmitter->GetPMaterial( "effects/spark" );
	Vector			dir, vOfs;

	float	length = 0.2f;

	//Fast lines
	for ( int i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( pParticle )
		{
			pParticle->m_flLifetime	= 0.0f;

			float	ramp = ( (float) i / (float)numSparks );

			dir[0] = normal[0] + random->RandomFloat( -MICRO_EXPLOSION_SPREAD*ramp, MICRO_EXPLOSION_SPREAD*ramp );
			dir[1] = normal[1] + random->RandomFloat( -MICRO_EXPLOSION_SPREAD*ramp, MICRO_EXPLOSION_SPREAD*ramp );
			dir[2] = normal[2] + random->RandomFloat( -MICRO_EXPLOSION_SPREAD*ramp, MICRO_EXPLOSION_SPREAD*ramp );
		
			pParticle->m_flWidth		= random->RandomFloat( 5.0f, 10.0f );
			pParticle->m_flLength		= (length*((1.0f-ramp)*(1.0f-ramp)*0.5f));
			pParticle->m_flDieTime		= 0.2f;
			pParticle->m_vecVelocity	= dir * random->RandomFloat( MICRO_EXPLOSION_MINSPEED*(1.5f-ramp), MICRO_EXPLOSION_MAXSPEED*(1.5f-ramp) );

			pParticle->m_flColor[0]		= 1.0f;
			pParticle->m_flColor[1]		= 1.0f;
			pParticle->m_flColor[2]		= 1.0f;
			pParticle->m_flColor[3]		= 1.0f;
		}
	}

	//
	// Filler
	//

	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_MicroExplosion 2" );
	pSimple->SetSortOrigin( offset );
	
	SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "sprites/rico1" ), offset );
		
	if ( sParticle )
	{
		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 0.3f;
		
		sParticle->m_vecVelocity.Init();

		sParticle->m_uchColor[0]	= 255;
		sParticle->m_uchColor[1]	= 255;
		sParticle->m_uchColor[2]	= 255;
		sParticle->m_uchStartAlpha	= random->RandomInt( 128, 255 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 12, 16 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ugly prototype explosion effect
//-----------------------------------------------------------------------------
#define	EXPLOSION_MINSPEED		300.0f
#define EXPLOSION_MAXSPEED		300.0f
#define EXPLOSION_SPREAD		0.8f
#define EXPLOSION_GRAVITY		800.0f
#define EXPLOSION_DAMPEN		0.4f

#define	EXPLOSION_FLECK_MIN_SPEED		150.0f
#define	EXPLOSION_FLECK_MAX_SPEED		350.0f
#define	EXPLOSION_FLECK_GRAVITY			800.0f
#define	EXPLOSION_FLECK_DAMPEN			0.3f
#define	EXPLOSION_FLECK_ANGULAR_SPRAY	0.8f

void FX_Explosion( Vector& origin, Vector& normal, char materialType )
{
	VPROF_BUDGET( "FX_Explosion", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	Vector	offset = origin + ( normal * 2.0f );

	CSmartPtr<CTrailParticles> pSparkEmitter = CTrailParticles::Create( "FX_Explosion 1" );
	if ( !pSparkEmitter )
		return;

	// Get color data from our hit point
	IMaterial	*pTraceMaterial;
	Vector		diffuseColor, baseColor;
	pTraceMaterial = engine->TraceLineMaterialAndLighting( origin, normal * -16.0f, diffuseColor, baseColor );
	// Get final light value
	float r = pow( diffuseColor[0], 1.0f/2.2f ) * baseColor[0];
	float g = pow( diffuseColor[1], 1.0f/2.2f ) * baseColor[1];
	float b = pow( diffuseColor[2], 1.0f/2.2f ) * baseColor[2];

	PMaterialHandle hMaterial	= pSparkEmitter->GetPMaterial( "effects/spark" );

	// Setup our collision information
	pSparkEmitter->Setup(	offset, 
							&normal, 
							EXPLOSION_SPREAD, 
							EXPLOSION_MINSPEED, 
							EXPLOSION_MAXSPEED, 
							EXPLOSION_GRAVITY, 
							EXPLOSION_DAMPEN, 
							bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	pSparkEmitter->SetSortOrigin( offset );

	Vector	dir;
	int		numSparks = random->RandomInt( 8,16 );

	// Dump out sparks
	int i;
	for ( i = 0; i < numSparks; i++ )
	{
		TrailParticle *pParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( pParticle == NULL )
			break;

		pParticle->m_flLifetime	= 0.0f;

		pParticle->m_flWidth		= random->RandomFloat( 5.0f, 10.0f );
		pParticle->m_flLength		= random->RandomFloat( 0.05, 0.1f );
		pParticle->m_flDieTime		= random->RandomFloat( 1.0f, 2.0f );
		
		dir[0] = normal[0] + random->RandomFloat( -EXPLOSION_SPREAD, EXPLOSION_SPREAD );
		dir[1] = normal[1] + random->RandomFloat( -EXPLOSION_SPREAD, EXPLOSION_SPREAD );
		dir[2] = normal[2] + random->RandomFloat( -EXPLOSION_SPREAD, EXPLOSION_SPREAD );
		pParticle->m_vecVelocity	= dir * random->RandomFloat( EXPLOSION_MINSPEED, EXPLOSION_MAXSPEED );

		pParticle->m_flColor[0]		= 1.0f;
		pParticle->m_flColor[1]		= 1.0f;
		pParticle->m_flColor[2]		= 1.0f;
		pParticle->m_flColor[3]		= 1.0f;
	}


	// Chunks o'dirt
	// Only create dirt chunks on concrete/world
	if ( materialType == 'C' || materialType == 'W' )
	{
		CSmartPtr<CFleckParticles> fleckEmitter = CFleckParticles::Create( "FX_Explosion 10" );
		if ( !fleckEmitter )
			return;

		fleckEmitter->SetSortOrigin( offset );

		// Setup our collision information
		fleckEmitter->m_ParticleCollision.Setup( offset, &normal, EXPLOSION_FLECK_ANGULAR_SPRAY, EXPLOSION_FLECK_MIN_SPEED, EXPLOSION_FLECK_MAX_SPEED, EXPLOSION_FLECK_GRAVITY, EXPLOSION_FLECK_DAMPEN );

		PMaterialHandle	hMaterialArray[2];
		
		switch ( materialType )
		{
		case 'C':
		case 'c':
		default:
			hMaterialArray[0] = fleckEmitter->GetPMaterial( "effects/fleck_cement1" );
			hMaterialArray[1] = fleckEmitter->GetPMaterial( "effects/fleck_cement2" );
			break;
		}

		int	numFlecks = random->RandomInt( 48, 64 );
		// Dump out flecks
		for ( i = 0; i < numFlecks; i++ )
		{
			FleckParticle *pParticle = (FleckParticle *) fleckEmitter->AddParticle( sizeof(FleckParticle), hMaterialArray[random->RandomInt(0,1)], offset );

			if ( pParticle == NULL )
				break;

			pParticle->m_flLifetime	= 0.0f;
			pParticle->m_flDieTime		= 3.0f;
			dir[0] = normal[0] + random->RandomFloat( -EXPLOSION_FLECK_ANGULAR_SPRAY, EXPLOSION_FLECK_ANGULAR_SPRAY );
			dir[1] = normal[1] + random->RandomFloat( -EXPLOSION_FLECK_ANGULAR_SPRAY, EXPLOSION_FLECK_ANGULAR_SPRAY );
			dir[2] = normal[2] + random->RandomFloat( -EXPLOSION_FLECK_ANGULAR_SPRAY, EXPLOSION_FLECK_ANGULAR_SPRAY );
			pParticle->m_uchSize		= random->RandomInt( 2, 6 );
			pParticle->m_vecVelocity	= dir * ( random->RandomFloat( EXPLOSION_FLECK_MIN_SPEED, EXPLOSION_FLECK_MAX_SPEED ) * ( 7 - pParticle->m_uchSize ) );
			pParticle->m_flRoll		= random->RandomFloat( 0, 360 );
			pParticle->m_flRollDelta	= random->RandomFloat( 0, 360 );

			float colorRamp = random->RandomFloat( 0.75f, 1.5f );
			pParticle->m_uchColor[0] = min( 1.0f, r*colorRamp )*255.0f;
			pParticle->m_uchColor[1] = min( 1.0f, g*colorRamp )*255.0f;
			pParticle->m_uchColor[2] = min( 1.0f, b*colorRamp )*255.0f;
		}
	}

	// Large sphere bursts
	CSmartPtr<CSimpleEmitter> pSimpleEmitter = CSimpleEmitter::Create( "FX_Explosion 1" );
	PMaterialHandle	hSphereMaterial = pSimpleEmitter->GetPMaterial( "particle/particle_noisesphere" );;
	Vector vecBurstOrigin = offset + normal * 8.0;
	pSimpleEmitter->SetSortOrigin( vecBurstOrigin );
	SimpleParticle *pSphereParticle = (SimpleParticle *) pSimpleEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, vecBurstOrigin );
	if ( pSphereParticle )
	{
		pSphereParticle->m_flLifetime		= 0.0f;
		pSphereParticle->m_flDieTime		= 0.3f;
		pSphereParticle->m_uchStartAlpha	= 150.0;
		pSphereParticle->m_uchEndAlpha		= 64.0;
		pSphereParticle->m_uchStartSize		= 0.0;
		pSphereParticle->m_uchEndSize		= 255.0;
		pSphereParticle->m_vecVelocity		= Vector(0,0,0);

		float colorRamp = random->RandomFloat( 0.75f, 1.5f );
		pSphereParticle->m_uchColor[0] = min( 1.0f, r*colorRamp )*255.0f;
		pSphereParticle->m_uchColor[1] = min( 1.0f, g*colorRamp )*255.0f;
		pSphereParticle->m_uchColor[2] = min( 1.0f, b*colorRamp )*255.0f;
	}

	// Throw some smoke balls out around the normal
	int numBalls = 12;
	Vector vecRight, vecForward, vecUp;
	QAngle vecAngles;
	VectorAngles( normal, vecAngles );
	AngleVectors( vecAngles, NULL, &vecRight, &vecUp );
	for ( i = 0; i < numBalls; i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pSimpleEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, vecBurstOrigin );
		if ( pParticle )
		{
			pParticle->m_flLifetime		= 0.0f;
			pParticle->m_flDieTime		= 0.25f;
			pParticle->m_uchStartAlpha	= 128.0;
			pParticle->m_uchEndAlpha	= 64.0;
			pParticle->m_uchStartSize	= 16.0;
			pParticle->m_uchEndSize		= 64.0;

			float flAngle = ((float)i * M_PI * 2) / numBalls;
			float x = cos( flAngle );
			float y = sin( flAngle );
			pParticle->m_vecVelocity = (vecRight*x + vecUp*y) * 1024.0;

			float colorRamp = random->RandomFloat( 0.75f, 1.5f );
			pParticle->m_uchColor[0] = min( 1.0f, r*colorRamp )*255.0f;
			pParticle->m_uchColor[1] = min( 1.0f, g*colorRamp )*255.0f;
			pParticle->m_uchColor[2] = min( 1.0f, b*colorRamp )*255.0f;
		}
	}

	// Create a couple of big, floating smoke clouds
	CSmartPtr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "FX_Explosion 2" );
	pSmokeEmitter->SetSortOrigin( offset );
	hSphereMaterial = pSmokeEmitter->GetPMaterial( "particle/particle_noisesphere" );
	for ( i = 0; i < 2; i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, offset );
		if ( pParticle == NULL )
			break;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random->RandomFloat( 2.0f, 3.0f );
		pParticle->m_uchStartSize	= 64;
		pParticle->m_uchEndSize		= 255;
		dir[0] = normal[0] + random->RandomFloat( -0.8f, 0.8f );
		dir[1] = normal[1] + random->RandomFloat( -0.8f, 0.8f );
		dir[2] = normal[2] + random->RandomFloat( -0.8f, 0.8f );
		pParticle->m_vecVelocity = dir * random->RandomFloat( 2.0f, 24.0f )*(i+1);
		pParticle->m_uchStartAlpha	= 160;
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_flRoll			= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -1, 1 );

		float colorRamp = random->RandomFloat( 0.5f, 1.25f );
		pParticle->m_uchColor[0] = min( 1.0f, r*colorRamp )*255.0f;
		pParticle->m_uchColor[1] = min( 1.0f, g*colorRamp )*255.0f;
		pParticle->m_uchColor[2] = min( 1.0f, b*colorRamp )*255.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			normal - 
//-----------------------------------------------------------------------------
void FX_ConcussiveExplosion( Vector &origin, Vector &normal )
{
	VPROF_BUDGET( "FX_ConcussiveExplosion", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	Vector	offset = origin + ( normal * 2.0f );
	Vector	dir;
	int		i;

	// 
	// Smoke
	//

	CSmartPtr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "FX_ConcussiveExplosion 1" );
	pSmokeEmitter->SetSortOrigin( offset );
	PMaterialHandle	hSphereMaterial = pSmokeEmitter->GetPMaterial( "particle/particle_noisesphere" );

	//Quick moving sprites
	 for ( i = 0; i < 16; i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, offset );

		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= random->RandomFloat( 0.2f, 0.4f );
		pParticle->m_uchStartSize	= random->RandomInt( 4, 8 );
		pParticle->m_uchEndSize		= random->RandomInt( 32, 64 );
		
		dir[0] = random->RandomFloat( -1.0f, 1.0f );
		dir[1] = random->RandomFloat( -1.0f, 1.0f );
		dir[2] = random->RandomFloat( -1.0f, 1.0f );
		
		pParticle->m_vecVelocity	= dir * random->RandomFloat( 64.0f, 128.0f );
		pParticle->m_uchStartAlpha	= random->RandomInt( 64, 128 );
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_flRoll			= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -4, 4 );

		int colorRamp = random->RandomFloat( 235, 255 );
		pParticle->m_uchColor[0] = colorRamp;
		pParticle->m_uchColor[1] = colorRamp;
		pParticle->m_uchColor[2] = colorRamp;
	}

	//Slow lingering sprites
	for ( i = 0; i < 2; i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, offset );
		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random->RandomFloat( 1.0f, 2.0f );
		pParticle->m_uchStartSize	= random->RandomInt( 32, 64 );
		pParticle->m_uchEndSize		= random->RandomInt( 100, 128 );

		dir[0] = normal[0] + random->RandomFloat( -0.8f, 0.8f );
		dir[1] = normal[1] + random->RandomFloat( -0.8f, 0.8f );
		dir[2] = normal[2] + random->RandomFloat( -0.8f, 0.8f );

		pParticle->m_vecVelocity = dir * random->RandomFloat( 16.0f, 32.0f );

		pParticle->m_uchStartAlpha	= random->RandomInt( 32, 64 );
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_flRoll			= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -1, 1 );

		int colorRamp = random->RandomFloat( 235, 255 );
		pParticle->m_uchColor[0] = colorRamp;
		pParticle->m_uchColor[1] = colorRamp;
		pParticle->m_uchColor[2] = colorRamp;
	}
	

	//	
	// Quick sphere
	//

	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_ConcussiveExplosion 2" );

	pSimple->SetSortOrigin( offset );
	
	SimpleParticle *pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof(SimpleParticle), pSimple->GetPMaterial( "effects/blueflare1" ), offset );
	
	if ( pParticle )
	{
		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= 0.1f;
		pParticle->m_vecVelocity.Init();
		pParticle->m_flRoll			= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -1, 1 );

		pParticle->m_uchColor[0] = pParticle->m_uchColor[1] = pParticle->m_uchColor[2] = 128;
		
		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 0;

		pParticle->m_uchStartSize	= 16;
		pParticle->m_uchEndSize		= 64;
	}
	
	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof(SimpleParticle), pSimple->GetPMaterial( "effects/blueflare1" ), offset );
	
	if ( pParticle )
	{
		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= 0.2f;
		pParticle->m_vecVelocity.Init();
		pParticle->m_flRoll		= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -1, 1 );
		pParticle->m_uchColor[0] = pParticle->m_uchColor[1] = pParticle->m_uchColor[2] = 32;
		
		pParticle->m_uchStartAlpha	= 64;
		pParticle->m_uchEndAlpha	= 0;

		pParticle->m_uchStartSize	= 64;
		pParticle->m_uchEndSize		= 128;
	}


	//
	// Dlight
	//

	dlight_t *dl= effects->CL_AllocDlight ( 0 );

	dl->origin	= offset;
	dl->color.r = dl->color.g = dl->color.b = 64;
	dl->radius	= random->RandomFloat(128,256);
	dl->die		= gpGlobals->curtime + 0.1;


	//
	// Moving lines
	//

	TrailParticle	*pTrailParticle;
	CSmartPtr<CTrailParticles> pSparkEmitter	= CTrailParticles::Create( "FX_ConcussiveExplosion 3" );
	PMaterialHandle hMaterial;
	int				numSparks;

	if ( pSparkEmitter.IsValid() )
	{
		hMaterial= pSparkEmitter->GetPMaterial( "effects/blueflare1" );

		pSparkEmitter->SetSortOrigin( offset );
		pSparkEmitter->m_ParticleCollision.SetGravity( 0.0f );

		numSparks = random->RandomInt( 16, 32 );

		//Dump out sparks
		for ( i = 0; i < numSparks; i++ )
		{
			pTrailParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

			if ( pTrailParticle == NULL )
				return;

			pTrailParticle->m_flLifetime	= 0.0f;

			dir.Random( -1.0f, 1.0f );

			pTrailParticle->m_flWidth		= random->RandomFloat( 1.0f, 2.0f );
			pTrailParticle->m_flLength		= random->RandomFloat( 0.01f, 0.1f );
			pTrailParticle->m_flDieTime		= random->RandomFloat( 0.1f, 0.2f );
			
			pTrailParticle->m_vecVelocity	= dir * random->RandomFloat( 800, 1000 );

			float colorRamp = random->RandomFloat( 0.75f, 1.0f );
			pTrailParticle->m_flColor[0]	= colorRamp;
			pTrailParticle->m_flColor[1]	= colorRamp;
			pTrailParticle->m_flColor[2]	= 1.0f;
			pTrailParticle->m_flColor[3]	= 1.0f;
		}
	}

	//Moving particles
	CSmartPtr<CTrailParticles> pCollisionEmitter	= CTrailParticles::Create( "FX_ConcussiveExplosion 4" );

	if ( pCollisionEmitter.IsValid() )
	{
		//Setup our collision information
		pCollisionEmitter->Setup( (Vector &) offset,
								NULL, 
								SPARK_ELECTRIC_SPREAD, 
								SPARK_ELECTRIC_MINSPEED*6, 
								SPARK_ELECTRIC_MAXSPEED*6, 
								-400, 
								SPARK_ELECTRIC_DAMPEN, 
								bitsPARTICLE_TRAIL_FADE );

		pCollisionEmitter->SetSortOrigin( offset );

		numSparks = random->RandomInt( 8, 16 );
		hMaterial = pCollisionEmitter->GetPMaterial( "effects/blueflare1" );

		//Dump out sparks
		for ( i = 0; i < numSparks; i++ )
		{
			pTrailParticle = (TrailParticle *) pCollisionEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

			if ( pTrailParticle == NULL )
				return;

			pTrailParticle->m_flLifetime	= 0.0f;

			dir.Random( -1.0f, 1.0f );
			dir[2] = random->RandomFloat( 0.0f, 0.75f );

			pTrailParticle->m_flWidth		= random->RandomFloat( 1.0f, 2.0f );
			pTrailParticle->m_flLength		= random->RandomFloat( 0.01f, 0.1f );
			pTrailParticle->m_flDieTime		= random->RandomFloat( 0.2f, 1.0f );
			
			pTrailParticle->m_vecVelocity	= dir * random->RandomFloat( 128, 512 );

			float colorRamp = random->RandomFloat( 0.75f, 1.0f );
			pTrailParticle->m_flColor[0]	= colorRamp;
			pTrailParticle->m_flColor[1]	= colorRamp;
			pTrailParticle->m_flColor[2]	= 1.0f;
			pTrailParticle->m_flColor[3]	= 1.0f;
		}
	}
}


void FX_SparkFan( Vector &position, Vector &normal )
{
	Vector	offset = position + ( normal * 1.0f );

	CSmartPtr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "FX_MetalScrape 1" );

	if ( !sparkEmitter )
		return;

	sparkEmitter->SetSortOrigin( offset );

	//Setup our collision information
	sparkEmitter->Setup( offset, 
						&normal, 
						METAL_SCRAPE_SPREAD, 
						METAL_SCRAPE_MINSPEED, 
						METAL_SCRAPE_MAXSPEED, 
						METAL_SCRAPE_GRAVITY, 
						METAL_SCRAPE_DAMPEN, 
						bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	TrailParticle	*pParticle;
	PMaterialHandle	hMaterial = sparkEmitter->GetPMaterial( "effects/spark" );
	Vector			dir;

	float	length	= 0.06f;

	//Dump out sparks
	for ( int i = 0; i < 35; i++ )
	{
		pParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;

		float	spreadOfs = random->RandomFloat( 0.0f, 2.0f );

		dir[0] = normal[0] + random->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
		dir[1] = normal[1] + random->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
		dir[2] = normal[2] + random->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
	
		pParticle->m_flWidth		= random->RandomFloat( 2.0f, 5.0f );
		pParticle->m_flLength		= random->RandomFloat( length*0.25f, length );
		pParticle->m_flDieTime		= random->RandomFloat( 2.0f, 2.0f );
		
		pParticle->m_vecVelocity	= dir * random->RandomFloat( (METAL_SCRAPE_MINSPEED*(2.0f-spreadOfs)), (METAL_SCRAPE_MAXSPEED*(2.0f-spreadOfs)) );
		
		pParticle->m_flColor[0]		= 1.0f;
		pParticle->m_flColor[1]		= 1.0f;
		pParticle->m_flColor[2]		= 1.0f;
		pParticle->m_flColor[3]		= 1.0f;
	}
}


void ManhackSparkCallback( const CEffectData & data )
{
	Vector vecNormal;
	Vector vecPosition;
	QAngle angles;

	vecPosition = data.m_vOrigin;
	vecNormal = data.m_vNormal;
	angles = data.m_vAngles;

	FX_SparkFan( vecPosition, vecNormal );
}

DECLARE_CLIENT_EFFECT( "ManhackSparks", ManhackSparkCallback );
