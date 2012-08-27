//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VCOLLIDE_H
#define VCOLLIDE_H
#ifdef _WIN32
#pragma once
#endif

class CPhysCollide;

struct vcollide_t
{
	int		solidCount;
	// VPhysicsSolids
	CPhysCollide	**solids;
	char			*pKeyValues;
};

#endif // VCOLLIDE_H
