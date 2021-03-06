//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef GRAPHPANEL_H
#define GRAPHPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/ListPanel.h>
#include <KeyValues.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Image.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/point.h>

class KeyValues;

namespace vgui
{
class Button;
class ToggleButton;
class RadioButton;
class Label;
class TextEntry;
class ListPanel;
class MessageBox;
class ComboBox;
class Panel;
class PropertySheet;
class CheckButton;
class Point;
};

#include "point.h"

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CGraphPanel:public vgui::PropertyPage
{
public:
	CGraphPanel(vgui::Panel *parent, const char *name);
	~CGraphPanel();

	// property page handlers
	virtual void OnPageShow();
	virtual void OnPageHide();

	virtual void PerformLayout();
	void AddPoint(Points_t p) { m_pGraphs->AddPoint(p); }
	void PingOnly(bool state);

private:

	typedef enum { SECONDS, MINUTES, HOURS } intervals;

	class CGraphsImage: public vgui::Image
	{
	public:
		CGraphsImage();
		using Image::DrawLine;
		using Image::SetPos;
		bool AddPoint(Points_t p);
		void RemovePoints() { points.RemoveAll(); }
		void SaveSize(int x1,int y1) { x=x1;y=y1; SetSize(x1,y1);}

		void SetDraw(bool cpu_in,bool fps_in,bool net_in,bool ping_in);
		void SetScale(intervals time);
		void PingOnly(bool state) { ping_only=state; }

		virtual void Paint();	

	private:

		void AvgPoint(Points_t p);
		void CheckBounds(Points_t p);
		CUtlVector<Points_t> points;
		
		int x,y; // the size
		float maxIn,minIn,maxOut,minOut; // max and min bandwidths
		float maxFPS,minFPS;
		float minPing,maxPing;

		bool cpu,fps,net,ping;
		bool ping_only; // only draw pings
		intervals timeBetween;
		Points_t avgPoint;
		int numAvgs;

	};
	friend CGraphsImage; // so it can use the intervals enum


	// msg handlers 
	void OnSendChat();
	void OnCheckButton();
	void OnButtonToggled();
	void OnClearButton();

	// GUI elements
	CGraphsImage *m_pGraphs;
	vgui::ImagePanel *m_pGraphsPanel;

	vgui::Button *m_pClearButton;

	vgui::CheckButton *m_pBandwidthButton;
	vgui::CheckButton *m_pFPSButton;
	vgui::CheckButton *m_pCPUButton;
	vgui::CheckButton *m_pPINGButton;
		
	vgui::RadioButton *m_pSecButton;
	vgui::RadioButton *m_pMinButton;
	vgui::RadioButton *m_pHourButton;

	vgui::Label *m_pTimeLabel;
	vgui::Label *m_pGraphsLabel;

	bool m_bPingOnly;

	DECLARE_PANELMAP();
	typedef vgui::PropertyPage BaseClass;
};

#endif // GRAPHPANEL_H