//========= Copyright � 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef IVGUI_H
#define IVGUI_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include <vgui/VGUI.h>

#include "appframework/IAppSystem.h"

class KeyValues;

namespace vgui
{

// safe handle to a panel - can be converted to and from a VPANEL
typedef unsigned long HPanel;
typedef int HContext;

#define DEFAULT_VGUI_CONTEXT ((vgui::HContext)~0)

// safe handle to a panel - can be converted to and from a VPANEL
typedef unsigned long HPanel;

//-----------------------------------------------------------------------------
// Purpose: Interface to core vgui components
//-----------------------------------------------------------------------------
class IVGui : public IBaseInterface, public IAppSystem
{
public:
	// must be called first - provides interfaces for vgui to access
	virtual bool Init( CreateInterfaceFn *factoryList, int numFactories ) = 0;

	// activates vgui message pump
	virtual void Start() = 0;

	// signals vgui to Stop running
	virtual void Stop() = 0;

	// returns true if vgui is current active
	virtual bool IsRunning() = 0;

	// runs a single frame of vgui
	virtual void RunFrame() = 0;

	// broadcasts "ShutdownRequest" "id" message to all top-level panels in the app
	virtual void ShutdownMessage(unsigned int shutdownID) = 0;

	// panel allocation
	virtual VPANEL AllocPanel() = 0;
	virtual void FreePanel(VPANEL panel) = 0;
	
	// debugging prints
	virtual void DPrintf(const char *format, ...) = 0;
	virtual void DPrintf2(const char *format, ...) = 0;
	
	// safe-pointer handle methods
	virtual HPanel PanelToHandle(VPANEL panel) = 0;
	virtual VPANEL HandleToPanel(HPanel index) = 0;
	virtual void MarkPanelForDeletion(VPANEL panel) = 0;

	// makes panel receive a 'Tick' message every frame (~50ms, depending on sleep times/framerate)
	// panel is automatically removed from tick signal list when it's deleted
	virtual void AddTickSignal(VPANEL panel, int intervalMilliseconds = 0 ) = 0;
	virtual void RemoveTickSignal(VPANEL panel) = 0;

	// message sending
	virtual void PostMessage(VPANEL target, KeyValues *params, VPANEL from, float delaySeconds = 0.0f) = 0;

	// Creates/ destroys vgui contexts, which contains information
	// about which controls have mouse + key focus, for example.
	virtual HContext CreateContext() = 0;
	virtual void DestroyContext( HContext context ) = 0; 

	// Associates a particular panel with a vgui context
	// Associating NULL is valid; it disconnects the panel from the context
	virtual void AssociatePanelWithContext( HContext context, VPANEL pRoot ) = 0;

	// Activates a particular context, use DEFAULT_VGUI_CONTEXT
	// to get the one normally used by VGUI
	virtual void ActivateContext( HContext context ) = 0;

	virtual void SetSleep( bool state) = 0; // whether to sleep each frame or not, true = sleep
};

#define VGUI_IVGUI_INTERFACE_VERSION "VGUI_ivgui005"

};


#endif // IVGUI_H
