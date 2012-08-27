//=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Defines interface to TrackerSRV dll
//=============================================================================

#ifndef TRACKERSRV_INTERFACE_H
#define TRACKERSRV_INTERFACE_H
#pragma once

#include "interface.h"

//-----------------------------------------------------------------------------
// Purpose: Tracker server interface
//-----------------------------------------------------------------------------
class ITrackerSRV : public IBaseInterface
{
public:
	virtual void RunTrackerServer(const char *lpCmdLine) = 0;
};

#define TRACKERSERVER_INTERFACE_VERSION "TrackerServer002"

#endif // TRACKERSRV_INTERFACE_H
