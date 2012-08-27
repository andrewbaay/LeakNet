//========= Copyright � 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A blood spray effect, like a big exit wound, used when people are
//			violently impaled, skewered, eviscerated, etc.
//
//=============================================================================

#include "cbase.h"
#include "ClientEffectPrecacheSystem.h"
#include "FX_Sparks.h"
#include "iefx.h"
#include "c_te_effect_dispatch.h"
#include "particles_ez.h"
#include "decals.h"
#include "engine/IEngineSound.h"
#include "fx_quad.h"
#include "engine/IVDebugOverlay.h"
#include "shareddefs.h"
#include "fx_blood.h"

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectBloodSpray )
CLIENTEFFECT_MATERIAL( "effects/blood_gore" )
CLIENTEFFECT_MATERIAL( "effects/blood_drop" )
CLIENTEFFECT_MATERIAL( "effects/blood_puff" )
CLIENTEFFECT_REGISTER_END()


ConVar	cl_show_bloodspray( "cl_show_bloodspray", "1" );


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			normal - 
//			scale - 
//-----------------------------------------------------------------------------
void FX_BloodSpray( const Vector &origin, const Vector &normal, float scale, unsigned char r, unsigned char g, unsigned char b, int flags )
{
	if ( cl_show_bloodspray.GetBool() == false )
		return;

	//debugoverlay->AddLineOverlay( origin, origin + normal * 72, 255, 255, 255, true, 10 ); 

	Vector offset;
	float spread	= 0.2f;
	
	//Find area ambient light color and use it to tint smoke
	Vector worldLight = WorldGetLightForPoint( origin, true );
	Vector color = Vector( (float)(worldLight[0] * r) / 255.0f, (float)(worldLight[1] * g) / 255.0f, (float)(worldLight[2] * b) / 255.0f );
	float colorRamp;

	int i;

	Vector	offDir;

	Vector right;
	Vector up;

	if (normal != Vector(0, 0, 1) )
	{
		right = normal.Cross( Vector(0, 0, 1) );
		up = right.Cross( normal );
	}
	else
	{
		right = Vector(0, 0, 1);
		up = right.Cross( normal );
	}

	//
	// Dump out drops
	//
	if (flags & FX_BLOODSPRAY_DROPS)
	{
		TrailParticle *tParticle;

		CSmartPtr<CTrailParticles> pTrailEmitter = CTrailParticles::Create( "blooddrops" );
		if ( !pTrailEmitter )
			return;

		pTrailEmitter->SetSortOrigin( origin );

		// Partial gravity on blood drops.
		pTrailEmitter->SetGravity( 600.0 ); 
		
		// Enable simple collisions with nearby surfaces.
		pTrailEmitter->Setup(origin, &normal, 1, 10, 100, 600, 0.2, 0 );

		PMaterialHandle	hMaterial = g_ParticleMgr.GetPMaterial( "effects/blood_drop" );

		//
		// Long stringy drops of blood.
		//
		for ( i = 0; i < 14; i++ )
		{
			// Originate from within a circle 'scale' inches in diameter.
			offset = origin;
			offset += right * random->RandomFloat( -0.5f, 0.5f ) * scale;
			offset += up * random->RandomFloat( -0.5f, 0.5f ) * scale;

			tParticle = (TrailParticle *) pTrailEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

			if ( tParticle == NULL )
				break;

			tParticle->m_flLifetime	= 0.0f;

			offDir = normal + RandomVector( -0.3f, 0.3f );

			tParticle->m_vecVelocity = offDir * random->RandomFloat( 4.0f * scale, 40.0f * scale );
			tParticle->m_vecVelocity[2] += random->RandomFloat( 4.0f, 16.0f ) * scale;

			tParticle->m_flWidth		= random->RandomFloat( 0.125f, 0.275f ) * scale;
			tParticle->m_flLength		= random->RandomFloat( 0.02f, 0.03f ) * scale;
			tParticle->m_flDieTime		= random->RandomFloat( 0.5f, 1.0f );

			tParticle->m_flColor[0]		= color[0];
			tParticle->m_flColor[1]		= color[1];
			tParticle->m_flColor[2]		= color[2];
			tParticle->m_flColor[3]		= 1.0f;
		}

		//
		// Shorter droplets.
		//
		for ( i = 0; i < 24; i++ )
		{
			// Originate from within a circle 'scale' inches in diameter.
			offset = origin;
			offset += right * random->RandomFloat( -0.5f, 0.5f ) * scale;
			offset += up * random->RandomFloat( -0.5f, 0.5f ) * scale;

			tParticle = (TrailParticle *) pTrailEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

			if ( tParticle == NULL )
				break;

			tParticle->m_flLifetime	= 0.0f;

			offDir = normal + RandomVector( -1.0f, 1.0f );
			offDir[2] += random->RandomFloat(0, 1.0f);

			tParticle->m_vecVelocity = offDir * random->RandomFloat( 2.0f * scale, 25.0f * scale );
			tParticle->m_vecVelocity[2] += random->RandomFloat( 4.0f, 16.0f ) * scale;

			tParticle->m_flWidth		= random->RandomFloat( 0.25f, 0.375f ) * scale;
			tParticle->m_flLength		= random->RandomFloat( 0.0025f, 0.005f ) * scale;
			tParticle->m_flDieTime		= random->RandomFloat( 0.5f, 1.0f );

			tParticle->m_flColor[0]		= color[0];
			tParticle->m_flColor[1]		= color[1];
			tParticle->m_flColor[2]		= color[2];
			tParticle->m_flColor[3]		= 1.0f;
		}
	}

	if ((flags & FX_BLOODSPRAY_GORE) || (flags & FX_BLOODSPRAY_CLOUD))
	{
		CSmartPtr<CBloodSprayEmitter> pSimple = CBloodSprayEmitter::Create( "bloodgore" );
		if ( !pSimple )
			return;

		pSimple->SetSortOrigin( origin );
		pSimple->SetGravity( 0 );

		PMaterialHandle	hMaterial;

		//
		// Tight blossom of blood at the center.
		//
		if (flags & FX_BLOODSPRAY_GORE)
		{
			hMaterial = g_ParticleMgr.GetPMaterial( "effects/blood_gore" );

			SimpleParticle *pParticle;

			for ( i = 0; i < 6; i++ )
			{
				// Originate from within a circle 'scale' inches in diameter.
				offset = origin + ( 0.5 * scale * normal );
				offset += right * random->RandomFloat( -0.5f, 0.5f ) * scale;
				offset += up * random->RandomFloat( -0.5f, 0.5f ) * scale;

				pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, offset );

				if ( pParticle != NULL )
				{
					pParticle->m_flLifetime = 0.0f;
					pParticle->m_flDieTime	= 0.3f;

					spread = 0.2f;
					pParticle->m_vecVelocity.Random( -spread, spread );
					pParticle->m_vecVelocity += normal * random->RandomInt( 10, 100 );
					//VectorNormalize( pParticle->m_vecVelocity );

					colorRamp = random->RandomFloat( 0.75f, 1.25f );

					pParticle->m_uchColor[0]	= min( 1.0f, color[0] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[1]	= min( 1.0f, color[1] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[2]	= min( 1.0f, color[2] * colorRamp ) * 255.0f;
					
					pParticle->m_uchStartSize	= random->RandomFloat( scale * 0.25, scale );
					pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 2;
					
					pParticle->m_uchStartAlpha	= random->RandomInt( 200, 255 );
					pParticle->m_uchEndAlpha	= 0;
					
					pParticle->m_flRoll			= random->RandomInt( 0, 360 );
					pParticle->m_flRollDelta	= random->RandomFloat( -4.0f, 4.0f );
				}
			}
		}

		//
		// Diffuse cloud just in front of the exit wound.
		//
		if (flags & FX_BLOODSPRAY_CLOUD)
		{
			hMaterial = g_ParticleMgr.GetPMaterial( "effects/blood_puff" );

			SimpleParticle *pParticle;

			for ( i = 0; i < 6; i++ )
			{
				// Originate from within a circle '2 * scale' inches in diameter.
				offset = origin + ( scale * normal );
				offset += right * random->RandomFloat( -1, 1 ) * scale;
				offset += up * random->RandomFloat( -1, 1 ) * scale;

				pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, offset );

				if ( pParticle != NULL )
				{
					pParticle->m_flLifetime = 0.0f;
					pParticle->m_flDieTime	= random->RandomFloat( 0.5f, 0.8f);

					spread = 0.5f;
					pParticle->m_vecVelocity.Random( -spread, spread );
					pParticle->m_vecVelocity += normal * random->RandomInt( 100, 200 );

					colorRamp = random->RandomFloat( 0.75f, 1.25f );

					pParticle->m_uchColor[0]	= min( 1.0f, color[0] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[1]	= min( 1.0f, color[1] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[2]	= min( 1.0f, color[2] * colorRamp ) * 255.0f;
					
					pParticle->m_uchStartSize	= random->RandomFloat( scale * 1.5f, scale * 2.0f );
					pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 4;
					
					pParticle->m_uchStartAlpha	= random->RandomInt( 80, 128 );
					pParticle->m_uchEndAlpha	= 0;
					
					pParticle->m_flRoll			= random->RandomInt( 0, 360 );
					pParticle->m_flRollDelta	= random->RandomFloat( -4.0f, 4.0f );
				}
			}
		}
	}

	// TODO: Play a sound?
	//CLocalPlayerFilter filter;
	//C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, CHAN_VOICE, "Physics.WaterSplash", 1.0, ATTN_NORM, 0, 100, &origin );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bloodtype - 
//			r - 
//			g - 
//			b - 
//-----------------------------------------------------------------------------
void GetBloodColor( int bloodtype, unsigned char &r, unsigned char &g, unsigned char &b )
{
	if (bloodtype == BLOOD_COLOR_RED)
	{
		r = 64;
		g = 0;
		b = 0;
	}
	else if ((bloodtype == BLOOD_COLOR_YELLOW) || (bloodtype == BLOOD_COLOR_GREEN))
	{
		r = 195;
		g = 195;
		b = 0;
	}
	else if (bloodtype == BLOOD_COLOR_MECH)
	{
		r = 20;
		g = 20;
		b = 20;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Intercepts the blood spray message.
//-----------------------------------------------------------------------------
void BloodSprayCallback( const CEffectData &data )
{
	unsigned char r = 0;
	unsigned char g = 0;
	unsigned char b = 0;
	GetBloodColor( data.m_nColor, r, g, b );
	FX_BloodSpray( data.m_vOrigin, data.m_vNormal, data.m_flScale, r, g, b, data.m_fFlags );
}

DECLARE_CLIENT_EFFECT( "bloodspray", BloodSprayCallback );
