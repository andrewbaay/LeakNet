//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ControlPanel.cpp
// last modified:  May 29 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.2
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#include "ControlPanel.h"
#include "ViewerSettings.h"
#include "StudioModel.h"
#include "MatSysWin.h"
#include "vphysics/constraints.h"
#include "physmesh.h"
#include "sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mx/mx.h>
#include <mx/mxBmp.h>
#include "vphysics_interface.h"
#include "UtlVector.h"
#include "UtlSymbol.h"
#include "UtlBuffer.h"
#include "attachments_window.h"


extern char g_appTitle[];
extern IPhysicsSurfaceProps *physprop;
extern bool LoadPhysicsProperties( void );

//-----------------------------------------------------------------------------
// Reads all of the physics materials from surface property file
//-----------------------------------------------------------------------------

static void ReadPhysicsMaterials( mxChoice *plist )
{
	LoadPhysicsProperties();
	plist->removeAll();

	if ( !physprop || !physprop->SurfacePropCount() )
	{
		plist->add("default");
		plist->select(0);
		return;
	}

	for ( int i = 0; i < physprop->SurfacePropCount(); i++ )
	{
		plist->add(physprop->GetPropName( i ) );
	}
	plist->select(0);
}


//-----------------------------------------------------------------------------
// Populates a control with all the physics bones
//-----------------------------------------------------------------------------

static bool PopulatePhysicsBoneList( mxChoice* pChoice )
{
	pChoice->removeAll();

	if ( g_pStudioModel->Physics_GetBoneCount() )
	{
		for ( int i = 0; i < g_pStudioModel->Physics_GetBoneCount(); i++ )
		{
			pChoice->add (g_pStudioModel->Physics_GetBoneName( i ) );
		}
	}
	else
	{
		pChoice->add( "None" );
		pChoice->select (0);
		return false;
	}

	pChoice->select (0);
	return true;
}

//-----------------------------------------------------------------------------
// Populates a control with all the physics bones
//-----------------------------------------------------------------------------

bool PopulateBoneList( mxChoice* pChoice )
{
	pChoice->removeAll();

	if ( g_pStudioModel )
	{
		studiohdr_t* pHdr = g_pStudioModel->getStudioHeader();
		if (pHdr && pHdr->numbones > 0)
		{
			for ( int i = 0; i < pHdr->numbones; i++ )
			{
				pChoice->add ( pHdr->pBone(i)->pszName() );
			}

			pChoice->select (0);
			return true;
		}
	}

	pChoice->add( "None" );
	pChoice->select (0);
	return false;
}

//-----------------------------------------------------------------------------
// Finds the index of a surface prop
//-----------------------------------------------------------------------------

static int FindSurfaceProp( char const* pSurfaceProp )
{
	for (int i = 0; i < physprop->SurfacePropCount(); ++i)
	{
		if (!stricmp( physprop->GetPropName( i ), pSurfaceProp ))
			return i;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// The tab associated with bone control
//-----------------------------------------------------------------------------

class CBoneControlWindow : public mxWindow
{
public:
	CBoneControlWindow( ControlPanel* pParent );

	void Init( );

	// This gets called when the model is loaded and unloaded
	void OnLoadModel();
	void OnUnloadModel();

	// Handles various events
	int handleEvent (mxEvent *event);

	// Called when we're selected
	void OnTabSelected();

	int		GetHitboxSet( void );
private:
	// Called when the bone is selected
	void OnBoneSelected( int boneIndex );
	void OnBoneHighlighted( bool isChecked );
	void OnHitboxHighlighted( bool isChecked );
	void OnShowDefaultPose( bool isChecked );
	void OnHitboxSelected( int hitbox );
	void OnHitboxGroupChanged( );
	void OnHitboxChanged( );
	void OnHitboxSetChanged( void );
	void OnAddHitbox( );
	void OnDeleteHitbox( );
	void OnGenerateQC( );
	void OnAutogenerateHitboxes( bool isChecked );

	// Writes out qc-style text to a utlbuffer
	bool SerializeQC( CUtlBuffer& buf );

	// Sets the surface property
	void OnSurfaceProp( int propIndex );

	// Duplictes the surface property to all children
	void OnSurfacePropApplyToChildren( );

	void RefreshHitbox( );
	void ComputeHitboxList( );

	void ComputeHitboxSetList( void );
	void OnHitboxAddSet( void );
	void OnHitboxDeleteSet( void );
	void OnHitboxSetChangeName( void );

	// Generates a list of all hitboxes per bone
	void PopulateHitboxLists();

	// Applies a surface property to all children
	void OnSurfacePropApplyToChildren_R( int bone, CUtlSymbol prop );

	// Selects the bone
	mxChoice* m_cBone;

	mxChoice*	m_cHitboxSet;

	mxLineEdit	*m_eHitboxSetName;
	mxButton	*m_bHitboxSetUpdateName;
	mxButton	*m_bAddHitboxSet;
	mxButton	*m_bDeleteHitboxSet;

	// Selects a hitbox associated with a bone
	mxChoice* m_cHitbox;

	// The materials to assign to the bone
	mxChoice* m_cSurfaceProp;

	// Are we highlighting bones?
	mxCheckBox* m_cBoneHighlight;

	// Are we highlighting hitboxes?
	mxCheckBox* m_cHitboxHighlight;

	// Should we show the default pose?
	mxCheckBox* m_cShowDefaultPose;

	// Should hitboxes be autogenerated?
	mxCheckBox* m_cAutoHitbox;

	// The control panel to which we're attached
	ControlPanel* m_pControlPanel;

	// Hitbox group
	mxLineEdit*	m_eHitboxGroup;

	// Hitbox name
	mxLineEdit* m_eHitboxName;


	// Hitbox origin
	mxLineEdit*	m_eOriginX;
	mxLineEdit*	m_eOriginY;
	mxLineEdit*	m_eOriginZ;

	// Hitbox size
	mxLineEdit*	m_eSizeX;
	mxLineEdit*	m_eSizeY;
	mxLineEdit*	m_eSizeZ;

	// Hitbox buttons
	mxButton*	m_bUpdateHitbox;
	mxButton*	m_bAddHitbox;
	mxButton*	m_bDeleteHitbox;

	// The list of hitboxes per bone...
	typedef CUtlVector< int	>	HitboxList_t;
	typedef CUtlVector<	HitboxList_t > BoneHitboxes_t;
	
	CUtlVector< BoneHitboxes_t >	m_SetBoneHitBoxes;

	// Currently selected hitbox + bone
	int	m_Bone;
	int	m_Hitbox;
	int m_nHitboxSet;
};


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------

CBoneControlWindow::CBoneControlWindow( ControlPanel* pParent ) : mxWindow( pParent, 0, 0, 0, 0 )
{
	m_pControlPanel = pParent;

	m_nHitboxSet = 0;
}

//-----------------------------------------------------------------------------
// Sets up all the controls in the window
//-----------------------------------------------------------------------------

void CBoneControlWindow::Init( )
{
	int left, top;
	left = 5;
	top = 0;

	// Bone selection list
	new mxLabel (this, left + 3, top + 4, 30, 18, "Bone");
	m_cBone = new mxChoice (this, left, top + 20, 260, 22, IDC_BONE_BONELIST);
	m_cBone->add ("None");
	m_cBone->select (0);
	mxToolTip::add (m_cBone, "Select a bone to modify");

	// Show bone checkbox
	m_cBoneHighlight = new mxCheckBox (this, left, top + 45, 140, 20, "Highlight Bone", IDC_BONE_HIGHLIGHT_BONE);
	mxToolTip::add (m_cBoneHighlight, "Toggle display of the bone being modified");

	// Bone surface property selection
	new mxLabel (this, left + 3, top + 68, 100, 18, "Bone Surface Prop");
	m_cSurfaceProp = new mxChoice( this, left, top + 85, 140, 22, IDC_BONE_SURFACEPROP );
	mxToolTip::add (m_cSurfaceProp, "Select a surface property to apply to the bone");

	// FIXME: We can't read the surface props yet because the vphysics path isn't known!!!
	// This will only add 'default' to the list
	ReadPhysicsMaterials( m_cSurfaceProp );

	// This button will apply the surface prop to all children
	mxButton *btnApplyChild = new mxButton( this, left, top + 115, 140, 20, "Apply to Children", IDC_BONE_APPLY_TO_CHILDREN );
	mxToolTip::add (btnApplyChild, "Apply the surface property to all child bones");

	// Use autogenerated hitboxes
	m_cAutoHitbox = new mxCheckBox (this, left, top + 140, 140, 20, "Autogenerate Hitboxes", IDC_BONE_USE_AUTOGENERATED_HITBOXES);
	mxToolTip::add (m_cAutoHitbox, "When this is checked, studiomdl will automatically generate hitboxes");

	m_cShowDefaultPose = new mxCheckBox (this, left, top + 160, 140, 20, "Show Default Pose", IDC_BONE_SHOW_DEFAULT_POSE);
	mxToolTip::add (m_cShowDefaultPose, "Toggles display of the default physics pose");

	left = 160;

	new mxLabel( this, left + 3, top + 50, 80, 18, "Hitbox Set" );
	m_cHitboxSet = new mxChoice (this, left, top + 66, 100, 22, IDC_BONE_HITBOXSET);
	m_cHitboxSet->add ("unnamed");
	m_cHitboxSet->select (0);
	mxToolTip::add (m_cHitboxSet, "Change hitbox set");

	m_eHitboxSetName = new mxLineEdit(this, left, top + 96, 100, 18, "", IDC_BONE_HITBOXSETNAME_EDIT);
	mxToolTip::add (m_eHitboxSetName, "Type in a name and hit the set button");

	m_bHitboxSetUpdateName = new mxButton( this, left, top + 116, 100, 16, "Set Name", IDC_BONE_HITBOXSETNAME );
	mxToolTip::add (m_bHitboxSetUpdateName, "Press to set hitbox set name from above text field");

	m_bAddHitboxSet = new mxButton( this, left, top + 140, 100, 20, "Add set", IDC_BONE_HITBOXADDSET );
	mxToolTip::add (m_bAddHitboxSet, "Add a hitbox set");
	m_bDeleteHitboxSet= new mxButton( this, left, top + 162, 100, 20, "Delete set", IDC_BONE_HITBOXDELETESET );
	mxToolTip::add (m_bDeleteHitboxSet, "Remove a hitbox set");

	left += 120;

	// Hitbox selection list
	new mxLabel (this, left + 3, top + 4, 30, 18, "Hitbox");
	m_cHitbox = new mxChoice (this, left, top + 20, 100, 22, IDC_BONE_HITBOXLIST);
	m_cHitbox->add ("None");
	m_cHitbox->select (0);
	mxToolTip::add (m_cHitbox, "Select a hitbox to modify");

	// Show hitbox checkbox
	m_cHitboxHighlight = new mxCheckBox (this, left + 3, top + 45, 100, 20, "Highlight Hitbox", IDC_BONE_HIGHLIGHT_HITBOX);
	mxToolTip::add (m_cHitboxHighlight, "Toggle display of the hitbox being modified");

	// Hitbox group
	new mxLabel (this, left + 3, top + 80, 80, 18, "Hitbox Group");
	m_eHitboxGroup = new mxLineEdit(this, left + 80, top + 75, 50, 22, "", IDC_BONE_HITBOX_GROUP);
	mxToolTip::add (m_eHitboxGroup, "The group of the current hitbox");

	// Hitbox name
	new mxLabel (this, left + 133, top + 80, 80, 18, "Hitbox Name");
	m_eHitboxName = new mxLineEdit(this, left + 133+80, top + 75, 50, 22, "", IDC_BONE_HITBOX_NAME);
	m_eHitboxName->setEnabled(false);
	mxToolTip::add (m_eHitboxName, "The name of the current hitbox");

	// Hitbox origin
	new mxLabel (this, left + 3,   top + 110, 80, 18, "Hitbox Origin");
	new mxLabel (this, left + 133, top + 110, 10, 18, "X");
	new mxLabel (this, left + 198, top + 110, 10, 18, "Y");
	new mxLabel (this, left + 263, top + 110, 10, 18, "Z");
	m_eOriginX = new mxLineEdit(this, left + 80, top + 105, 50, 22, "", IDC_BONE_HITBOX_ORIGINX);
	m_eOriginY = new mxLineEdit(this, left + 145, top + 105, 50, 22, "", IDC_BONE_HITBOX_ORIGINY);
	m_eOriginZ = new mxLineEdit(this, left + 210, top + 105, 50, 22, "", IDC_BONE_HITBOX_ORIGINZ);

	// Hitbox size
	new mxLabel (this, left + 3,   top + 140, 80, 18, "Hitbox Size");
	new mxLabel (this, left + 133, top + 140, 10, 18, "X");
	new mxLabel (this, left + 198, top + 140, 10, 18, "Y");
	new mxLabel (this, left + 263, top + 140, 10, 18, "Z");
	m_eSizeX = new mxLineEdit(this, left + 80, top + 135, 50, 22, "", IDC_BONE_HITBOX_SIZEX);
	m_eSizeY = new mxLineEdit(this, left + 145, top + 135, 50, 22, "", IDC_BONE_HITBOX_SIZEY);
	m_eSizeZ = new mxLineEdit(this, left + 210, top + 135, 50, 22, "", IDC_BONE_HITBOX_SIZEZ);

	// Update hitboxes here
	m_bUpdateHitbox = new mxButton( this, left, top + 163, 100, 20, "Update Hitbox", IDC_BONE_UPDATE_HITBOX );
	mxToolTip::add (m_bUpdateHitbox, "Apply hitbox group, origin, and size to the hitbox");

	left += 160;

	// Generate a QC file
	mxButton* btnGenerateQC = new mxButton (this, left, top + 10, 100, 20, "Generate QC", IDC_BONE_GENERATEQC);
	mxToolTip::add (btnGenerateQC, "Copy a .qc file snippet to the clipboard");

	// Add, remove hitboxes here
	m_bAddHitbox = new mxButton( this, left, top + 30, 100, 20, "Add Hitbox", IDC_BONE_ADD_HITBOX );
	m_bDeleteHitbox = new mxButton( this, left, top + 50, 100, 20, "Delete Hitbox", IDC_BONE_DELETE_HITBOX );
	mxToolTip::add (m_bAddHitbox, "Create a new hitbox attached to the current bone");
	mxToolTip::add (m_bDeleteHitbox, "Delete the currently selected hitbox");

	OnAutogenerateHitboxes( false );
}

//-----------------------------------------------------------------------------
// Generates a list of all hitboxes per bone
//-----------------------------------------------------------------------------

void CBoneControlWindow::PopulateHitboxLists()
{
	// Find the surface prop associated with this bone
	if (!g_pStudioModel)
		return;

	m_SetBoneHitBoxes.RemoveAll();
	for (int set = 0; set < g_pStudioModel->m_HitboxSets.Size(); set++ )
	{
		m_SetBoneHitBoxes.AddToTail();

		for (unsigned short i = g_pStudioModel->m_HitboxSets[ set ].Head(); 
			i != g_pStudioModel->m_HitboxSets[ set ].InvalidIndex(); 
			i = g_pStudioModel->m_HitboxSets[ set ].Next(i) )
		{
			mstudiobbox_t* pHitbox = &g_pStudioModel->m_HitboxSets[ set ][i];
			m_SetBoneHitBoxes[ set ].EnsureCount( pHitbox->bone + 1 );
			m_SetBoneHitBoxes[ set ][ pHitbox->bone ].AddToTail( i );
		}
	}
}

//-----------------------------------------------------------------------------
// This gets called when the model is loaded and unloaded
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnLoadModel()
{
	// Now that the vphysics path is known, we can load the surface props
	ReadPhysicsMaterials( m_cSurfaceProp );

	studiohdr_t* pHdr = g_pStudioModel->getStudioHeader();
	m_cAutoHitbox->setChecked( (pHdr->flags & STUDIOHDR_FLAGS_AUTOGENERATED_HITBOX) != 0 );
	OnAutogenerateHitboxes( m_cAutoHitbox->isChecked() );

	// Determine all bones for this model
	PopulateBoneList( m_cBone );

	ComputeHitboxSetList();
	PopulateHitboxLists();

	OnBoneSelected( 0 );
}

void CBoneControlWindow::OnUnloadModel()
{
	m_cBone->removeAll();
	m_cBone->add ("None");
	m_cBone->select (0);

	m_cAutoHitbox->setChecked( false );
}

//-----------------------------------------------------------------------------
// Called when we're selected
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnTabSelected()
{
	// Make the selected bone and highlight state match
	OnBoneSelected( m_cBone->getSelectedIndex() );
	OnBoneHighlighted( m_cBoneHighlight->isChecked() ); 
	OnHitboxHighlighted( m_cHitboxHighlight->isChecked() );
	OnShowDefaultPose( m_cShowDefaultPose->isChecked() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBoneControlWindow::GetHitboxSet( void )
{
	return m_nHitboxSet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneControlWindow::ComputeHitboxSetList( void )
{
	m_cHitboxSet->removeAll();

	for ( int i = 0 ; i < g_pStudioModel->m_HitboxSets.Size(); i++ )
	{
		char const *name = g_pStudioModel->m_HitboxSetNames[ i ].name;
		m_cHitboxSet->add( name );
	}
	m_cHitboxSet->select( 0 );
}

//-----------------------------------------------------------------------------
// Recomputes the hitbox list
//-----------------------------------------------------------------------------

void CBoneControlWindow::ComputeHitboxList( )
{
	// Reset the hitbox list
	m_cHitbox->removeAll();
	int count = 0;
	if ( m_SetBoneHitBoxes.Size() > 0 )
	{
		if (m_SetBoneHitBoxes[ m_nHitboxSet ].Count() > m_Bone)
			count = m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Count();
		if (count > 0)
		{
			for (int i = 0; i < count; ++i )
			{
				char buf[32];
				sprintf( buf, "%d", m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone][i] );
				m_cHitbox->add( buf );
			}
		}
		else
		{
			m_cHitbox->add ("None");
		}
	}
	else
	{
		m_cHitbox->add ("None");
	}

	OnHitboxSelected(0);
}

//-----------------------------------------------------------------------------
// Here's what we do when a new bone is selected
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnBoneSelected( int boneIndex )
{
	// Reset the surface prop
	if (!g_pStudioModel)
		return;

	if ( physprop && g_pStudioModel->m_SurfaceProps.Count() )
	{
		// Find the surface prop associated with this bone
		char const* pSurfaceProp = g_pStudioModel->m_SurfaceProps[boneIndex].String();
		int idx = FindSurfaceProp( pSurfaceProp );

		// Can't find it? Then apply the default one
		if (idx < 0)
			idx = FindSurfaceProp( "default" );
		if (idx < 0)
			idx = 0;
		m_cSurfaceProp->select(idx);
	}
	else
	{
		m_cSurfaceProp->select(0);
	}

	// Store off the bone
	m_Bone = boneIndex;
	if (m_cBoneHighlight->isChecked())
		g_viewerSettings.highlightBone = m_Bone;

	ComputeHitboxList();

	// select the render/highlight bone
	m_pControlPanel->redraw();
}


//-----------------------------------------------------------------------------
// When the bone is highlighted or not
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnShowDefaultPose( bool isChecked )
{
	g_viewerSettings.showPhysicsPreview = isChecked;
}

//-----------------------------------------------------------------------------
// When the bone is highlighted or not
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnBoneHighlighted( bool isChecked )
{
	g_viewerSettings.highlightBone = isChecked ? m_Bone : -1;
}

//-----------------------------------------------------------------------------
// When the hitbox is highlighted or not
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnHitboxHighlighted( bool isChecked )
{
	g_viewerSettings.highlightHitbox = isChecked ? m_Hitbox : -1;
}


//-----------------------------------------------------------------------------
// Refreshes the hitbox size
//-----------------------------------------------------------------------------

void CBoneControlWindow::RefreshHitbox( )
{
	if ( m_nHitboxSet < 0 || m_nHitboxSet >= g_pStudioModel->m_HitboxSets.Size() )
	{
		m_nHitboxSet = 0;
	}

	if (m_Hitbox >= 0 && g_pStudioModel->m_HitboxSets.Size() > 0 )
	{
		// Set the hitbox size + origin + group
		mstudiobbox_t* pHitbox = &g_pStudioModel->m_HitboxSets[ m_nHitboxSet ][ m_Hitbox ];

		Vector origin, size;
		VectorSubtract( pHitbox->bbmax, pHitbox->bbmin, size );
		VectorAdd( pHitbox->bbmax, pHitbox->bbmin, origin );
		origin *= 0.5f;

		m_eHitboxGroup->setLabel( "%i", pHitbox->group );
		char const *hitboxname = "";
		if ( pHitbox->szhitboxnameindex != 0 )
		{
			hitboxname = (char*)(g_pStudioModel->getStudioHeader() ) + pHitbox->szhitboxnameindex;
		}

		m_eHitboxName->setLabel( hitboxname );

		m_eOriginX->setLabel("%.3f", origin.x );
		m_eOriginY->setLabel("%.3f", origin.y );
		m_eOriginZ->setLabel("%.3f", origin.z );
		m_eSizeX->setLabel("%.3f", size.x );
		m_eSizeY->setLabel("%.3f", size.y );
		m_eSizeZ->setLabel("%.3f", size.z );
	}
	else
	{
		m_eHitboxGroup->setLabel( "" );
		m_eHitboxName->setLabel( "" );
		m_eOriginX->setLabel("");
		m_eOriginY->setLabel("");
		m_eOriginZ->setLabel("");
		m_eSizeX->setLabel("");
		m_eSizeY->setLabel("");
		m_eSizeZ->setLabel("");
	}
}

//-----------------------------------------------------------------------------
// When a hitbox is selected
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnHitboxSelected( int hitbox )
{
	if ( m_nHitboxSet < 0 || m_nHitboxSet >= g_pStudioModel->m_HitboxSets.Size() )
	{
		m_nHitboxSet = 0;
	}

	m_cHitbox->select(hitbox);

	if ( m_SetBoneHitBoxes.Size() == 0 ||
		(m_SetBoneHitBoxes[ m_nHitboxSet ].Count() <= m_Bone) || 
		(m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Count() <= hitbox))
	{
		m_Hitbox = -1;
		studiohdr_t* pHdr = g_pStudioModel->getStudioHeader();

		// This'll cause no boxes to be drawn
		if (m_cHitboxHighlight->isChecked())
			g_viewerSettings.highlightHitbox = pHdr ? pHdr->numbones + 1 : 1;
	}
	else
	{
		m_Hitbox = m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone][hitbox];
		if (m_cHitboxHighlight->isChecked())
			g_viewerSettings.highlightHitbox = m_Hitbox;
	}

	RefreshHitbox();
}


//-----------------------------------------------------------------------------
// Hitbox size/origin changed
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnAutogenerateHitboxes( bool isChecked )
{
	m_eHitboxGroup->setEnabled( !isChecked );
	m_eHitboxName->setEnabled( !isChecked );
	m_eOriginX->setEnabled( !isChecked );
	m_eOriginY->setEnabled( !isChecked );
	m_eOriginZ->setEnabled( !isChecked );
	m_eSizeX->setEnabled( !isChecked );
	m_eSizeY->setEnabled( !isChecked );
	m_eSizeZ->setEnabled( !isChecked );
	m_bUpdateHitbox->setEnabled( !isChecked );
	m_bAddHitbox->setEnabled( !isChecked );
	m_bDeleteHitbox->setEnabled( !isChecked );

	m_cHitboxSet->setEnabled( !isChecked );
	m_eHitboxSetName->setEnabled( !isChecked );
	m_bHitboxSetUpdateName->setEnabled( !isChecked );
	m_bAddHitboxSet->setEnabled( !isChecked );
	m_bDeleteHitboxSet->setEnabled( !isChecked );
}

//-----------------------------------------------------------------------------
// Hitbox size/origin changed
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnHitboxChanged( )
{
	if ( m_nHitboxSet < 0 || m_nHitboxSet >= g_pStudioModel->m_HitboxSets.Size() )
	{
		m_nHitboxSet = 0;
	}

	if (m_Hitbox < 0 || g_pStudioModel->m_HitboxSets.Size() <= 0 )
	{
		// Blat out the hitbox size + origin + group
		RefreshHitbox();
		return;
	}

	Vector size, origin;
	mstudiobbox_t* pHitbox;
	const char *pGroup;

	// Gotta do it this way since getLabel whacks the previous return result to getLable
	char const* pLabel = m_eOriginX->getLabel();
	if (!pLabel)
		goto errOut;
	origin.x = atof( pLabel );

	pLabel = m_eOriginY->getLabel();
	if (!pLabel)
		goto errOut;
	origin.y = atof( pLabel );

	pLabel = m_eOriginZ->getLabel();
	if (!pLabel)
		goto errOut;
	origin.z = atof( pLabel );

	pLabel = m_eSizeX->getLabel();
	if (!pLabel)
		goto errOut;
	size.x = atof( pLabel );

	pLabel = m_eSizeY->getLabel();
	if (!pLabel)
		goto errOut;
	size.y = atof( pLabel );

	pLabel = m_eSizeZ->getLabel();
 	if (!pLabel)
		goto errOut;
	size.z = atof( pLabel );

	pHitbox = &g_pStudioModel->m_HitboxSets[ m_nHitboxSet ][m_Hitbox];

	// Recompute the hitbox from the new data
	VectorMA( origin, -0.5f, size, pHitbox->bbmin );
	VectorMA( origin,  0.5f, size, pHitbox->bbmax );

	pGroup = m_eHitboxGroup->getLabel();
	if (pGroup)
	{
		pHitbox->group = atol( pGroup );
	}

errOut:
	RefreshHitbox();
}

//-----------------------------------------------------------------------------
// Purpose: Hitbox set changed
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnHitboxSetChanged( void )
{
	m_Hitbox = 0;
	m_nHitboxSet = m_cHitboxSet->getSelectedIndex();

	PopulateHitboxLists();

	// Repopulate other controls
	OnHitboxGroupChanged( );
	ComputeHitboxList();
	RefreshHitbox();
}

//-----------------------------------------------------------------------------
// Hitbox size/origin changed
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnHitboxGroupChanged( )
{
	if (m_Hitbox >= 0 && g_pStudioModel->m_HitboxSets.Size() > 0 )
	{
		const char *pGroup = m_eHitboxGroup->getLabel();
		if (pGroup && m_Hitbox < g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].Count() )
		{
			mstudiobbox_t* pHitbox = &g_pStudioModel->m_HitboxSets[ m_nHitboxSet ][m_Hitbox];
			pHitbox->group = atol( pGroup );
		}
	}
}


//-----------------------------------------------------------------------------
// Add, remove hitboxes
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnAddHitbox( )
{
	if (!g_pStudioModel)
		return;

	// Remove the 'none' entry
	if (m_SetBoneHitBoxes[ m_nHitboxSet ].Size() > 0 &&
		m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Count() == 0)
	{
		m_cHitbox->removeAll();
	}

	int i = g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].AddToTail();
	g_pStudioModel->m_HitboxSets[ m_nHitboxSet ][i].bone = m_Bone;
	g_pStudioModel->m_HitboxSets[ m_nHitboxSet ][i].group = 0;
	g_pStudioModel->m_HitboxSets[ m_nHitboxSet ][i].bbmin.Init( -8, -8, -8 );
	g_pStudioModel->m_HitboxSets[ m_nHitboxSet ][i].bbmax.Init( 8, 8, 8 );
	g_pStudioModel->m_HitboxSets[ m_nHitboxSet ][i].szhitboxnameindex = 0;


	m_SetBoneHitBoxes[ m_nHitboxSet ].EnsureCount( m_Bone + 1 );
	m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].AddToTail(i);

	char buf[32];
	sprintf(buf, "%d", i );
	m_cHitbox->add ( buf );
	OnHitboxSelected(m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Count() - 1);
}

void CBoneControlWindow::OnDeleteHitbox( )
{
	if (!g_pStudioModel || (m_Hitbox < 0))
		return;

	g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].Remove(m_Hitbox);
	for (int i = m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Count(); --i >= 0; )
	{
		if (m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone][i] == m_Hitbox)
		{
			m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Remove(i);
			break;
		}
	}

	// Recompute the list of hitboxes
	ComputeHitboxList();
}


//-----------------------------------------------------------------------------
// Sets the surface property
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnSurfaceProp( int propIndex )
{
	if (g_pStudioModel->IsModelLoaded())
	{
		// Store off the new surface prop symbol
		CUtlSymbol prop( physprop->GetPropName( propIndex ) );
		g_pStudioModel->m_SurfaceProps[m_Bone] = prop;
	}
}

//-----------------------------------------------------------------------------
// Duplictes the surface property to all children
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnSurfacePropApplyToChildren_R( int bone, CUtlSymbol prop )
{
	g_pStudioModel->m_SurfaceProps[bone] = prop;

	studiohdr_t* pHdr = g_pStudioModel->getStudioHeader();
	for ( int i = 0; i < pHdr->numbones; i++ )
	{
		mstudiobone_t* pBone = pHdr->pBone(i);
		if (pBone->parent == bone)
		{
			OnSurfacePropApplyToChildren_R( i, prop );
		}
	}
}

void CBoneControlWindow::OnSurfacePropApplyToChildren( )
{
	if (!g_pStudioModel)
		return;

	CUtlSymbol prop = g_pStudioModel->m_SurfaceProps[m_Bone];
	OnSurfacePropApplyToChildren_R( m_Bone, prop );
}

//-----------------------------------------------------------------------------
// Writes out qc-style text to a utlbuffer
//-----------------------------------------------------------------------------

bool CBoneControlWindow::SerializeQC( CUtlBuffer& buf )
{
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
	if (!hdr)
		return false;

	buf.Printf("// .qc block generated by HLMV begins.\n\n");

	// Print out the surface props
	buf.Printf( "$surfaceprop \"%s\"\n", g_pStudioModel->m_SurfaceProps[0].String() );
	Assert( g_pStudioModel->m_SurfaceProps.Count() == hdr->numbones );

	int i;
	for ( i = 1; i < g_pStudioModel->m_SurfaceProps.Count(); ++i)
	{
		mstudiobone_t* pBone = hdr->pBone(i);

		// Don't bother printing out the name if it's got the same
		// surface prop as the parent does
		if (pBone->parent >= 0)
		{
			if (!stricmp( g_pStudioModel->m_SurfaceProps[i].String(), 
							g_pStudioModel->m_SurfaceProps[pBone->parent].String() ))
				continue;
		}

		buf.Printf( "$jointsurfaceprop \"%s\"\t \"%s\"\n", pBone->pszName(), 
			g_pStudioModel->m_SurfaceProps[i].String() );
	}

	if (!m_cAutoHitbox->isChecked())
	{
		buf.Printf("\n");

		for ( i = 0 ; i < g_pStudioModel->m_HitboxSets.Size(); i++ )
		{
			buf.Printf( "\n$hboxset \"%s\"\n\n", g_pStudioModel->m_HitboxSetNames[ i ].name );

			for (unsigned short j = g_pStudioModel->m_HitboxSets[ i ].Head(); 
				j != g_pStudioModel->m_HitboxSets[ i ].InvalidIndex(); 
				j = g_pStudioModel->m_HitboxSets[ i ].Next(j) )
			{
				mstudiobone_t* pBone = hdr->pBone(g_pStudioModel->m_HitboxSets[ i ][j].bone);
				buf.Printf( "$hbox %d \"%s\"\t  %7.2f %7.2f %7.2f  %7.2f %7.2f %7.2f\n", 
					g_pStudioModel->m_HitboxSets[ i ][j].group, pBone->pszName(), 
					g_pStudioModel->m_HitboxSets[ i ][j].bbmin.x, g_pStudioModel->m_HitboxSets[ i ][j].bbmin.y, g_pStudioModel->m_HitboxSets[ i ][j].bbmin.z,
					g_pStudioModel->m_HitboxSets[ i ][j].bbmax.x, g_pStudioModel->m_HitboxSets[ i ][j].bbmax.y, g_pStudioModel->m_HitboxSets[ i ][j].bbmax.z );
			}
		}
	}

	buf.Printf("\n// .qc block generated by HLMV ends.\n\n");

	return true;
}

//-----------------------------------------------------------------------------
// Generates the QC file and copies it to the clipboard
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnGenerateQC( )
{
	CUtlBuffer outbuf( 0, 0, true );
	SerializeQC( outbuf );
	if ( outbuf.TellPut() )
	{
		// Null-terminate the string so CopyString works...
		outbuf.PutChar('\0');
		Sys_CopyStringToClipboard( (char const*)outbuf.Base() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnHitboxAddSet( void )
{
	char sz[ 32 ];
	sprintf( sz, "set%02i", g_pStudioModel->m_HitboxSets.Size() + 1 );

	int newsetnumber = g_pStudioModel->m_HitboxSets.AddToTail();
	StudioModel::hbsetname_s *setname = &g_pStudioModel->m_HitboxSetNames[ g_pStudioModel->m_HitboxSetNames.AddToTail() ];
	strcpy( setname->name, sz );
	
	ComputeHitboxSetList();

	m_cHitboxSet->select( newsetnumber );

	OnHitboxSetChanged();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnHitboxDeleteSet( void )
{
	// Can't remove last element
	if ( m_nHitboxSet == 0 )
	{
		return;
	}

	g_pStudioModel->m_HitboxSets.Remove( m_nHitboxSet );
	g_pStudioModel->m_HitboxSetNames.Remove( m_nHitboxSet );

	ComputeHitboxSetList();

	m_cHitboxSet->select( 0 );

	OnHitboxSetChanged();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnHitboxSetChangeName( void )
{
	if ( g_pStudioModel->m_HitboxSetNames.Size() <= 0 )
		return;

	char newname[ 512 ];

	strcpy( newname, m_eHitboxSetName->getLabel() );
	if ( !newname[ 0 ] )
		return;

	strcpy( g_pStudioModel->m_HitboxSetNames[ m_nHitboxSet ].name, newname );

	int oldsel = m_nHitboxSet;

	ComputeHitboxSetList();

	m_cHitboxSet->select( oldsel );

	OnHitboxSetChanged();
}

//-----------------------------------------------------------------------------
// Responds to events on controls in the window
//-----------------------------------------------------------------------------

int CBoneControlWindow::handleEvent (mxEvent *event)
{
	if ( event->event == mxEvent::KeyDown )
	{
 		OnHitboxChanged( );
		return 1;
	}

	switch( event->action )
	{
	case IDC_BONE_BONELIST:
		if (g_pStudioModel->IsModelLoaded())
		{
 			OnHitboxGroupChanged( );
			OnBoneSelected( m_cBone->getSelectedIndex() );
		}
		break;

	case IDC_BONE_HIGHLIGHT_BONE:
		OnBoneHighlighted( ((mxCheckBox *) event->widget)->isChecked () );
		break;

	case IDC_BONE_HIGHLIGHT_HITBOX:
 		OnHitboxHighlighted( ((mxCheckBox *) event->widget)->isChecked () );
		break;

	case IDC_BONE_SHOW_DEFAULT_POSE:
 		OnShowDefaultPose( ((mxCheckBox *) event->widget)->isChecked () );
		break;

	case IDC_BONE_HITBOXLIST:
 		OnHitboxGroupChanged( );
 		OnHitboxSelected( m_cHitbox->getSelectedIndex() );
		break;

	case IDC_BONE_HITBOXSET:
		OnHitboxSetChanged();
 		break;

	case IDC_BONE_HITBOXADDSET:
		OnHitboxAddSet();
		break;
	case IDC_BONE_HITBOXDELETESET:
		OnHitboxDeleteSet();
		break;
	case IDC_BONE_HITBOXSETNAME:
		OnHitboxSetChangeName();
		break;

	case IDC_BONE_UPDATE_HITBOX:
 		OnHitboxChanged( );
		break;

	case IDC_BONE_ADD_HITBOX:
 		OnHitboxGroupChanged( );
 		OnAddHitbox( );
		break;

	case IDC_BONE_DELETE_HITBOX:
 		OnDeleteHitbox( );
		break;

	case IDC_BONE_USE_AUTOGENERATED_HITBOXES:
		if (g_pStudioModel->IsModelLoaded())
 			OnAutogenerateHitboxes( m_cAutoHitbox->isChecked() );
		break;

	case IDC_BONE_SURFACEPROP:
 		OnSurfaceProp( m_cSurfaceProp->getSelectedIndex() );
		break;

	case IDC_BONE_APPLY_TO_CHILDREN:
		if (g_pStudioModel->IsModelLoaded())
	 		OnSurfacePropApplyToChildren( );
		break;

	case IDC_BONE_GENERATEQC:
		if (g_pStudioModel->IsModelLoaded())
		{
 			OnHitboxGroupChanged( );
	 		OnGenerateQC( );
		}
		break;

	default:
		return 0;
	}

	return 1;
}



//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------

ControlPanel *g_ControlPanel = 0;



ControlPanel::ControlPanel (mxWindow *parent)
: mxWindow (parent, 0, 0, 0, 0, "Control Panel", mxWindow::Normal)
{
	int i;

	InitViewerSettings ( "hlmv" );

	// create tabcontrol with subdialog windows
	tab = new mxTab (this, 0, 0, 0, 0, IDC_TAB);
#ifdef WIN32
	SetWindowLong ((HWND) tab->getHandle (), GWL_EXSTYLE, WS_EX_CLIENTEDGE);
#endif

	// Render Window
	wRender = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wRender, "Render");
	cRenderMode = new mxChoice (wRender, 5, 5, 100, 22, IDC_RENDERMODE);
	cRenderMode->add ("Wireframe");
	cRenderMode->add ("Flatshaded");
	cRenderMode->add ("Smoothshaded");
	cRenderMode->add ("Textured");
	cRenderMode->select (3);
	mxToolTip::add (cRenderMode, "Select Render Mode");
	cbGround = new mxCheckBox (wRender, 110, 5, 150, 20, "Ground", IDC_GROUND);
	cbGround->setEnabled( true );
	cbMovement = new mxCheckBox (wRender, 110, 25, 150, 20, "Movement", IDC_MOVEMENT);
	cbMovement->setEnabled( true );
	cbBackground = new mxCheckBox (wRender, 110, 45, 150, 20, "Background", IDC_BACKGROUND);
	cbBackground->setEnabled( true );
	cbHitBoxes = new mxCheckBox (wRender, 110, 65, 150, 20, "Hit Boxes", IDC_HITBOXES);
	cbSequenceBoxes = new mxCheckBox (wRender, 110, 85, 150, 20, "Seq. Boxes", IDC_SEQUENCEBOXES);
	cbSoftwareSkin = new mxCheckBox (wRender, 110, 125, 150, 20, "Software Skin", IDC_SOFTWARESKIN);
	cbSoftwareSkin->setEnabled( true );
	cbOverbright2 = new mxCheckBox (wRender, 110, 145, 150, 20, "Enable Overbrightening", IDC_OVERBRIGHT2);
	cbOverbright2->setEnabled( true );
	cbOverbright2->setChecked( true );
	setOverbright( true );

	cbAttachments = new mxCheckBox (wRender, 5, 45, 100, 20, "Attachments", IDC_ATTACHMENTS);
	cbAttachments->setEnabled( true );
	cbBones = new mxCheckBox (wRender, 5, 65, 100, 20, "Bones", IDC_BONES);
	cbNormals = new mxCheckBox (wRender, 5, 85, 100, 20, "Normals", IDC_NORMALS);

	cbRunIK = new mxCheckBox (wRender, 260, 65, 150, 20, "Enable IK", IDC_RUNIK);
	cbEnableHead = new mxCheckBox (wRender, 260, 85, 150, 20, "Head Turn", IDC_HEADTURN);

	mxCheckBox *cbPhysicsModel = new mxCheckBox (wRender, 260, 5, 150, 20, "Physics Model", IDC_PHYSICSMODEL);
	cHighlightBone = new mxChoice (wRender, 260, 25, 150, 22, IDC_PHYSICSHIGHLIGHT);
	cHighlightBone->add ("None");
	cHighlightBone->select (0);
	mxToolTip::add (cHighlightBone, "Select Physics Bone to highlight");

	new mxLabel (wRender, 5, 110, 30, 18, "FOV:");
	leFOV = new mxLineEdit(wRender, 35, 105, 30, 22, "65", IDC_RENDER_FOV);

	// Sequence Window
	mxWindow *wSequence = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wSequence, "Sequence");

	for (i = 0; i < MAX_SEQUENCES; i++)
	{
		cSequence[i] = new mxChoice (wSequence, 5, 5 + i * 22, 200, 22, IDC_SEQUENCE0+i);	
		mxToolTip::add (cSequence[i], "Select Sequence");
		slSequence[i] = new mxSlider (wSequence, 208, 5 + i * 22, 80, 18, IDC_SEQUENCESCALE0+i);
		slSequence[i]->setRange (0, 1.0, 100);
		slSequence[i]->setValue (0.0);

		rbFrameSelection[i] = new mxRadioButton (wSequence, 300, 5 + i * 22, 35, 22, "", IDC_FRAMESELECTION0+i, i == 0);
	}
	slSequence[0]->setVisible( false );

	laGroundSpeed = new mxLabel( wSequence, 208, 5, 80, 18, "" );

	for (i = 0; i < NUM_POSEPARAMETERS; i++)
	{
		int x, y;
		x = 334;
		y = 2 + (i % 8) * 17;

		cPoseParameter[i] = new mxChoice (wSequence, 520, y, 96, 22, IDC_POSEPARAMETER+i);	
		cPoseParameter[i]->setVisible( false );

		slPoseParameter[i] = new mxSlider (wSequence, x, y, 140, 16, IDC_POSEPARAMETER_SCALE+i);
		slPoseParameter[i]->setRange (0.0, 1.0);
		mxToolTip::add (slPoseParameter[i], "Parameter");
		slPoseParameter[i]->setVisible( false );

		laPoseParameter[i] = new mxLabel( wSequence, x + 146, y, 40, 16, "X" );
		laPoseParameter[i]->setVisible( false );
	}

	slSpeedScale = new mxSlider (wSequence, 5, 115, 200, 18, IDC_SPEEDSCALE);
	slSpeedScale->setRange (0, 1.0, 100);
	slSpeedScale->setValue (1.0);
	mxToolTip::add (slSpeedScale, "Speed Scale");
	laFPS = new mxLabel (wSequence, 208, 115, 128, 22, "" );

	new mxLabel( wSequence, 5, 137, 50, 18, "Frame" );
	slForceFrame = new mxSlider (wSequence, 55, 137, 450, 18, IDC_FORCEFRAME);
	slForceFrame->setRange (0, 1.0);
	slForceFrame->setValue (0.0);
	slForceFrame->setSteps(1,1);
	mxToolTip::add (slForceFrame, "Force To Frame");
	lForcedFrame = new mxLabel( wSequence, 505, 137, 30, 18, "0" );

	mxCheckBox *cbBlendSequences = new mxCheckBox (wSequence, 5, 162, 150, 20, "Blend Sequence Changes", IDC_BLENDSEQUENCECHANGES);
	mxButton *btnBlendNow = new mxButton( wSequence, 155, 162, 80, 20, "Blend Now", IDC_BLENDNOW );
	laBlendAmount = new mxLabel( wSequence, 240, 162, 60, 20, "" );

	slBlendTime = new mxSlider( wSequence, 308, 162, 200, 18, IDC_BLENDTIME );
	slBlendTime->setRange( 0, 1.0, 100 );
	slBlendTime->setValue( DEFAULT_BLEND_TIME );
	laBlendTime = new mxLabel( wSequence, 540, 162, 80, 22, "" );

	// Body Window
	SetupBodyWindow( tab );

	// Flex Window
	SetupFlexWindow( tab );

 	// Physics Window
	SetupPhysicsWindow( tab );

	// Bone control window
	SetupBoneControlWindow( tab );

	SetupAttachmentsWindow( tab );

	g_ControlPanel = this;
}

//-----------------------------------------------------------------------------
// Sets up the window dealing with body control
//-----------------------------------------------------------------------------

void ControlPanel::SetupBodyWindow( mxTab* pTab )
{
	mxWindow *wBody = new mxWindow (this, 0, 0, 0, 0);
	pTab->add (wBody, "Model");
	cBodypart = new mxChoice (wBody, 5, 5, 100, 22, IDC_BODYPART);
	mxToolTip::add (cBodypart, "Choose a bodypart");
	cSubmodel = new mxChoice (wBody, 110, 5, 100, 22, IDC_SUBMODEL);
	mxToolTip::add (cSubmodel, "Choose a submodel of current bodypart");
	cController = new mxChoice (wBody, 5, 30, 100, 22, IDC_CONTROLLER);	
	mxToolTip::add (cController, "Choose a bone controller");
	slController = new mxSlider (wBody, 105, 32, 100, 18, IDC_CONTROLLERVALUE);
	slController->setRange (0, 255);
	mxToolTip::add (slController, "Change current bone controller value");
	lModelInfo1 = new mxLabel (wBody, 220, 5, 120, 100, "No Model.");
	lModelInfo2 = new mxLabel (wBody, 340, 5, 120, 100, "");
	cSkin = new mxChoice (wBody, 5, 55, 100, 22, IDC_SKINS);
	mxToolTip::add (cSkin, "Choose a skin family");

	lModelInfo3 = new mxLabel (wBody, 220, 80, 120, 100, "");
	
	cbAutoLOD = new mxCheckBox (wBody, 5, 80, 100, 20, "Auto LOD", IDC_AUTOLOD);
	cbAutoLOD->setEnabled( true );
	cLODChoice = new mxChoice( wBody, 5, 101, 100, 22, IDC_LODCHOICE);
	mxToolTip::add (cLODChoice, "Select model LOD to render");
	new mxLabel (wBody, 5, 126, 60, 18, "LOD Switch:");
	leLODSwitch = new mxLineEdit(wBody, 70, 126, 35, 22, "", IDC_LODSWITCH);
	new mxLabel (wBody, 5, 151, 60, 18, "LOD Metric:" ); 
	lLODMetric = new mxLabel( wBody, 70, 151, 35, 22, "" );


}

//-----------------------------------------------------------------------------
// Sets up the window dealing with flexes
//-----------------------------------------------------------------------------

void ControlPanel::SetupFlexWindow( mxTab* pTab )
{
	mxWindow *wFlex = new mxWindow (this, 0, 0, 0, 0);
	pTab->add (wFlex, "Flex");
	for (int i = 0; i < NUM_FLEX_SLIDERS; i++)
	{
		int w = (i / 8) * 156 + 5;
		int h = (i % 8) * 20 + 5;

		cFlex[i] = new mxChoice (wFlex, w, h, 76, 22, IDC_FLEX + i);	
		mxToolTip::add (cFlex[i], "Select Flex");
		slFlexScale[i] = new mxSlider (wFlex, w + 76, h, 76, 18, IDC_FLEXSCALE + i);
		slFlexScale[i]->setRange (0, 1.0);
		slFlexScale[i]->setValue (0);
		mxToolTip::add (slFlexScale[i], "Flex Scale");
	}
}

//-----------------------------------------------------------------------------
// Sets up the window dealing with physics
//-----------------------------------------------------------------------------

void ControlPanel::SetupPhysicsWindow( mxTab* pTab )
{
	// Physics Window
	mxWindow *wPhysics = new mxWindow (this, 0, 0, 0, 0);
	pTab->add (wPhysics, "Physics");

	new mxLabel (wPhysics, 5, 8, 30, 18, "Mass");
	leMass = new mxLineEdit(wPhysics, 35, 5, 30, 22, "", IDC_PHYS_MASS);

	cPhysicsBone = new mxChoice (wPhysics, 5, 30, 100, 22, IDC_PHYS_BONE);
	cPhysicsBone->add ("None");
	cPhysicsBone->select (0);

	mxCheckBox *cbHighlight = new mxCheckBox (wPhysics, 5, 55, 100, 20, "Highlight", IDC_PHYSICSMODEL);
	int x = 5;
	int y = 80;

	rbConstraintAxis[0] = new mxRadioButton (wPhysics, x, y, 35, 22, "X", IDC_PHYS_CON_AXIS_X, true);
	rbConstraintAxis[1] = new mxRadioButton (wPhysics, x+35, y, 35, 22, "Y", IDC_PHYS_CON_AXIS_Y);
	rbConstraintAxis[2] = new mxRadioButton (wPhysics, x+70, y, 35, 22, "Z", IDC_PHYS_CON_AXIS_Z);
	setPhysicsAxis( 0 );
	y += 25;

	new mxLabel (wPhysics, x, y, 45, 18, "Friction");
	slPhysicsFriction = new mxSlider (wPhysics, x+50, y, 180, 18, IDC_PHYS_CON_FRICTION);
	slPhysicsFriction->setRange (1, 1000.0, 1000);
	slPhysicsFriction->setValue (1);
	slPhysicsFriction->setSteps( 1, 1 );
	lPhysicsFriction = new mxLabel (wPhysics, x+230, y, 30, 18, "1");

	new mxLabel (wPhysics, 115, 5, 30, 18, "Min");
	new mxLabel (wPhysics, 115, 30, 30, 18, "Max");
	new mxLabel (wPhysics, 115, 55, 30, 18, "Test");

	x = 135;
	y = 5;

	slPhysicsConMin = new mxSlider (wPhysics, x, y, 180, 18, IDC_PHYS_CON_MIN);
	slPhysicsConMin->setRange (-180, 180.0, 360);
	slPhysicsConMin->setValue (-90);
	slPhysicsConMin->setSteps( 1, 1 );
	lPhysicsConMin = new mxLabel (wPhysics, x+180, y, 30, 18, "-90");
	y += 25;

	slPhysicsConMax = new mxSlider (wPhysics, x, y, 180, 18, IDC_PHYS_CON_MAX);
	slPhysicsConMax->setRange (-180.0, 180.0, 360);
	slPhysicsConMax->setValue (90.0);
	slPhysicsConMax->setSteps( 1, 1 );
	lPhysicsConMax = new mxLabel (wPhysics, x+180, y, 30, 18, "90");
	y += 25;

	slPhysicsConTest = new mxSlider (wPhysics, x, y, 55, 18, IDC_PHYS_CON_TEST);
	slPhysicsConTest->setRange (0, 1.0, 100);
	slPhysicsConTest->setValue (0);

	cbLinked = new mxCheckBox (wPhysics, 200, 55, 50, 20, "Link", IDC_PHYS_CON_LINK_LIMITS);
	mxToolTip::add (cbLinked, "Link mins/maxs to be symmetric");
	new mxButton (wPhysics, 250, 55, 80, 22, "Generate QC", IDC_PHYS_QCFILE);

	x += 220;
	y = 5;
	new mxLabel (wPhysics, x, y, 55, 18, "Mass Bias");
	new mxLabel (wPhysics, x, y+25, 55, 18, "Inertia");
	new mxLabel (wPhysics, x, y+50, 55, 18, "Damping");
	new mxLabel (wPhysics, x, y+75, 55, 18, "Rot Damp");
	new mxLabel (wPhysics, x, y+100+3, 55, 18, "Material");
	x += 55;

	slPhysicsParamMassBias = new mxSlider (wPhysics, x, y, 76, 18, IDC_PHYS_P_MASSBIAS );
	slPhysicsParamMassBias->setRange (0, 10.0, 100);
	slPhysicsParamMassBias->setValue (1.0);
	slPhysicsParamMassBias->setSteps( 1, 1 );
	lPhysicsParamMassBias = new mxLabel (wPhysics, x+80, y, 30, 18, "1.0");
	y += 25;

	slPhysicsParamInertia = new mxSlider (wPhysics, x, y, 76, 18, IDC_PHYS_P_INERTIA );
	slPhysicsParamInertia->setRange (0, 10.0, 100);
	slPhysicsParamInertia->setValue (1.0);
	slPhysicsParamInertia->setSteps( 1, 1 );
	lPhysicsParamInertia = new mxLabel (wPhysics, x+80, y, 30, 18, "1.0");
	y += 25;

	slPhysicsParamDamping = new mxSlider (wPhysics, x, y, 76, 18, IDC_PHYS_P_DAMPING );
	slPhysicsParamDamping->setRange (0, 1.0, 100);
	slPhysicsParamDamping->setValue (0.01);
	slPhysicsParamDamping->setSteps( 1, 1 );
	lPhysicsParamDamping = new mxLabel (wPhysics, x+80, y, 30, 18, "0.5");
	y += 25;

	slPhysicsParamRotDamping = new mxSlider (wPhysics, x, y, 76, 18, IDC_PHYS_P_ROT_DAMPING );
	slPhysicsParamRotDamping->setRange (0, 10.0, 200);
	slPhysicsParamRotDamping->setValue (0.2);
	slPhysicsParamRotDamping->setSteps( 1, 1 );
	lPhysicsParamRotDamping = new mxLabel (wPhysics, x+80, y, 30, 18, "0.2");
	y += 25;

	lPhysicsMaterial = new mxLabel( wPhysics, x, y+3, 110, 18, "default" );
}

//-----------------------------------------------------------------------------
// Sets up the window dealing with bone control
//-----------------------------------------------------------------------------

void ControlPanel::SetupBoneControlWindow( mxTab* pTab )
{
	m_pBoneWindow = new CBoneControlWindow(this);
	pTab->add (m_pBoneWindow, "Bones");
	m_pBoneWindow->Init();
}


void ControlPanel::SetupAttachmentsWindow( mxTab *pTab )
{
	m_pAttachmentsWindow = new CAttachmentsWindow(this);
	pTab->add( m_pAttachmentsWindow, "Attachments" );
	m_pAttachmentsWindow->Init();
}


int ControlPanel::GetCurrentHitboxSet( void )
{
	return m_pBoneWindow ? m_pBoneWindow->GetHitboxSet() : 0;
}

ControlPanel::~ControlPanel ()
{
}



int
ControlPanel::handleEvent (mxEvent *event)
{
	if ( !g_ControlPanel )
		return 1;

	if (event->event == mxEvent::Size)
	{
		tab->setBounds (0, 0, event->width, event->height);
		return 1;
	}
	
	if ( event->event == mxEvent::KeyDown )
	{
		if ( tab->getSelectedIndex() == 4 )
		{
			handlePhysicsKey( event );
		}
		return 1;
	}

	switch (event->action)
	{
		case IDC_TAB:
		{
			int tabIndex = tab->getSelectedIndex();
			
			g_viewerSettings.highlightBone = -1;
			g_viewerSettings.highlightHitbox = -1;
			g_viewerSettings.showTexture = (tabIndex == 3) ? true : false;
			g_viewerSettings.showPhysicsPreview = (tabIndex == 4) ? true : false;
			setHighlightBone(cHighlightBone->getSelectedIndex ());

			if (tabIndex == 4)
			{
				setupPhysicsBone(cPhysicsBone->getSelectedIndex());
			}

			if (tabIndex == 5)
			{
				m_pBoneWindow->OnTabSelected();
			}

			if ( tabIndex == 6 )
			{
				m_pAttachmentsWindow->OnTabSelected();
			}
			else
			{
				m_pAttachmentsWindow->OnTabUnselected();
			}
		}
		break;

		case IDC_RENDERMODE:
		{
			int index = cRenderMode->getSelectedIndex ();
			if (index >= 0)
			{
				setRenderMode (index);
			}
		}
		break;

		case IDC_PHYSICSHIGHLIGHT:
		{
			int index = cHighlightBone->getSelectedIndex ();
			setHighlightBone(index);
		}
		break;

		case IDC_RENDER_FOV:
		{
			mxLineEdit *pLineEdit = ((mxLineEdit *) event->widget);
			const char *pText = pLineEdit->getLabel();
			float val = atof( pText );
			g_viewerSettings.fov = val;
			break;
		}

		case IDC_LODCHOICE:
		{
			int index = cLODChoice->getSelectedIndex();
			if( index >= 0 )
			{
				setLOD( index, false, false );
			}
			break;
		}

		case IDC_LODSWITCH:
		{
			mxLineEdit *pLineEdit = ((mxLineEdit *) event->widget);
			const char *pText = pLineEdit->getLabel();
			float val = atof( pText );
			g_pStudioModel->SetLODSwitchValue( g_viewerSettings.lod, val );
			break;
		}
		
		case IDC_AUTOLOD:
			setAutoLOD (((mxCheckBox *) event->widget)->isChecked ());
			break;
		
		case IDC_SOFTWARESKIN:
			setSoftwareSkin(((mxCheckBox *) event->widget)->isChecked ());
			break;
		
		case IDC_OVERBRIGHT2:
			setOverbright(((mxCheckBox *) event->widget)->isChecked ());
			break;
		case IDC_GROUND:
			setShowGround (((mxCheckBox *) event->widget)->isChecked ());
			break;

		case IDC_MOVEMENT:
			setShowMovement (((mxCheckBox *) event->widget)->isChecked ());
			break;

		case IDC_BACKGROUND:
			setShowBackground (((mxCheckBox *) event->widget)->isChecked ());
			break;

		case IDC_HITBOXES:
			g_viewerSettings.showHitBoxes = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_SEQUENCEBOXES:
			g_viewerSettings.showSequenceBoxes = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_RUNIK:
			g_viewerSettings.enableIK = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_HEADTURN:
			g_viewerSettings.solveHeadTurn = ((mxCheckBox *) event->widget)->isChecked () ? 1 : 0;
			break;

		case IDC_PHYSICSMODEL:
			g_viewerSettings.showPhysicsModel = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_BONES:
			g_viewerSettings.showBones = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_NORMALS:
			g_viewerSettings.showNormals = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_ATTACHMENTS:
			g_viewerSettings.showAttachments = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_SEQUENCE0:
		case IDC_SEQUENCE1:
		case IDC_SEQUENCE2:
		case IDC_SEQUENCE3:
		case IDC_SEQUENCE4:
		{
			int i = event->action - IDC_SEQUENCE0;
			int index = ((mxChoice *) event->widget)->getSelectedIndex ();
			if (index >= 0)
			{
				if (i == 0)
				{
					setSequence (index);
				}
				else
				{
					setOverlaySequence( i, index, slSequence[i]->getValue() );
				}
			}
		}
		break;

		case IDC_SEQUENCESCALE0:
		case IDC_SEQUENCESCALE1:
		case IDC_SEQUENCESCALE2:
		case IDC_SEQUENCESCALE3:
		case IDC_SEQUENCESCALE4:
		{
			int i = event->action - IDC_SEQUENCESCALE0;
			int index = cSequence[i]->getSelectedIndex ();
			if (index >= 0)
			{
				if (i == 0)
				{
					setSequence (index);
				}
				else
				{
					setOverlaySequence( i, index, (float)((mxSlider *) event->widget)->getValue () );
				}
			}
		}
		break;

		case IDC_FRAMESELECTION0:
		case IDC_FRAMESELECTION1:
		case IDC_FRAMESELECTION2:
		case IDC_FRAMESELECTION3:
		case IDC_FRAMESELECTION4:
		{
			updateFrameSelection();
		}
		break;

		case IDC_BLENDTIME:
			{
				char sz[ 256 ];
				sprintf( sz, "%.2f s", (float)((mxSlider *) event->widget)->getValue () );

				g_pStudioModel->SetBlendTime( ((mxSlider *) event->widget)->getValue () );
				laBlendTime->setLabel( sz );
			}
			break;
		case IDC_BLENDSEQUENCECHANGES:
			g_viewerSettings.blendSequenceChanges = ((mxCheckBox *) event->widget)->isChecked ();
			break;
		case IDC_BLENDNOW:
			startBlending();
			break;

		case IDC_SPEEDSCALE:
		{
			setSpeedScale( ((mxSlider *) event->widget)->getValue () );
		}
		break;

		case IDC_FORCEFRAME:
		{
			setFrame( ((mxSlider *) event->widget)->getValue () );

			// stop the animation
			setSpeedScale( 0 );
		}
		break;

		case IDC_BODYPART:
		{
			int index = cBodypart->getSelectedIndex ();
			if (index >= 0)
			{
				g_viewerSettings.submodels[cBodypart->getSelectedIndex ()] = index;
				g_pStudioModel->SetBodygroup (cBodypart->getSelectedIndex (), index);
				setBodypart (index);

			}
		}
		break;

		case IDC_SUBMODEL:
		{
			int index = cSubmodel->getSelectedIndex ();
			if (index >= 0)
			{
				setSubmodel (index);

			}
		}
		break;

		case IDC_CONTROLLER:
		{
			int index = cController->getSelectedIndex ();
			if (index >= 0)
				setBoneController (index);
		}
		break;

		case IDC_CONTROLLERVALUE:
		{
			int index = cController->getSelectedIndex ();
			if (index >= 0)
			{
				float flValue = slController->getValue();
				studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
				mstudiobonecontroller_t *pbonecontroller = hdr->pBonecontroller(index);
				if (pbonecontroller->end < pbonecontroller->start)
				{
					flValue *= -1.0f;
				}
				setBoneControllerValue(index, flValue);
			}
		}
		break;

		case IDC_SKINS:
		{
			int index = cSkin->getSelectedIndex ();
			if (index >= 0)
			{
				g_pStudioModel->SetSkin (index);
				g_viewerSettings.skin = index;
				d_MatSysWindow->redraw ();
			}
		}
		break;
		default:
		{
			if (event->action >= IDC_FLEX && event->action < IDC_FLEX + NUM_FLEX_SLIDERS)
			{
				int flex = (event->action - IDC_FLEX);
				int index = cFlex[flex]->getSelectedIndex ();

				if (index >= 0)
				{
					slFlexScale[flex]->setValue(g_pStudioModel->GetFlexController(index) * 100);
				}
			}
			else if (event->action >= IDC_FLEXSCALE && event->action < IDC_FLEXSCALE + NUM_FLEX_SLIDERS)
			{
				int flex = (event->action - IDC_FLEXSCALE);
				g_pStudioModel->SetFlexController( cFlex[flex]->getSelectedIndex(), ((mxSlider *) event->widget)->getValue () );
			}
			else if ( event->action >= IDC_PHYS_FIRST && event->action <= IDC_PHYS_LAST )
			{
				return handlePhysicsEvent( event );
			}
			else if (event->action >= IDC_POSEPARAMETER && event->action < IDC_POSEPARAMETER + NUM_POSEPARAMETERS)
			{
				int index = event->action - IDC_POSEPARAMETER;
				int poseparam = cPoseParameter[index]->getSelectedIndex();

				float flMin, flMax;
				if (g_pStudioModel->GetPoseParameterRange( poseparam, &flMin, &flMax ))
				{
					slPoseParameter[index]->setRange( flMin, flMax );
					slPoseParameter[index]->setValue( g_pStudioModel->GetPoseParameter( poseparam ) );
					laPoseParameter[index]->setLabel( "%.1f", g_pStudioModel->GetPoseParameter( poseparam ) );
				}
			}
			else if (event->action >= IDC_POSEPARAMETER_SCALE && event->action < IDC_POSEPARAMETER_SCALE + NUM_POSEPARAMETERS)
			{
				int index = event->action - IDC_POSEPARAMETER_SCALE;
				int poseparam = cPoseParameter[index]->getSelectedIndex();

				setBlend( poseparam, ((mxSlider *) event->widget)->getValue () );
				laPoseParameter[index]->setLabel( "%.1f", ((mxSlider *) event->widget)->getValue () );
			}
		}
		break;
	}

	return 1;
}



void
ControlPanel::dumpModelInfo ()
{
//#if 0 // VXP
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
	if (hdr)
	{
		DeleteFile ("midump.txt");
		FILE *file = fopen ("midump.txt", "wt");
		if (file)
		{
			byte *phdr = (byte *) hdr;
			int i;

			fprintf (file, "id: %c%c%c%c\n", phdr[0], phdr[1], phdr[2], phdr[3]);

		//	fprintf (file, "version: %d\n", hdr->version); // VXP: This is wrong since our convertation code
			bool done = false;
			studiohdr_t *temp_phdr;

			char const *filename = g_pStudioModel->GetFileName();
			FILE *fp = fopen( filename, "rb" );
			if( fp != NULL ) // VXP: Cannot open file for some reason - let's just use new version value then
			{
			/*
				fseek( fp, 0, SEEK_END );
				long size = ftell( fp );
				fseek( fp, 0, SEEK_SET );

				void *buffer = malloc( size );
				if (!buffer)
				{
					fclose (fp);
					done = false;
				}

				fread( buffer, size, 1, fp );
				fclose( fp );

				byte *temp_pin;

				temp_pin = (byte *)buffer;
				temp_phdr = (studiohdr_t *)temp_pin;

				free (buffer);

				if ( temp_phdr != NULL )
					done = true;
			*/
				fseek( fp, 0, SEEK_END );
				int len = ftell( fp );
				rewind( fp );
				temp_phdr = ( studiohdr_t * )malloc( len );
				fread( temp_phdr, 1, len, fp );
				if ( temp_phdr != NULL && (int)temp_phdr->version != hdr->version )
					done = true;
			}
			if ( done )
			{
				fprintf (file, "version: %d (converted from %d)\n", ( int )temp_phdr->version, hdr->version); // VXP: Getting the right version
			}
			else
			{
				fprintf (file, "version: %d\n", hdr->version);
			}

			fprintf (file, "name: \"%s\"\n", hdr->name);
			fprintf (file, "length: %d bytes\n\n", hdr->length);

			fprintf (file, "eyeposition: %f %f %f\n", hdr->eyeposition[0], hdr->eyeposition[1], hdr->eyeposition[2]);
		//	fprintf (file, "min: %f %f %f\n", hdr->min[0], hdr->min[1], hdr->min[2]);
		//	fprintf (file, "max: %f %f %f\n", hdr->max[0], hdr->max[1], hdr->max[2]);
			fprintf (file, "hull_min: %f %f %f\n", hdr->hull_min[0], hdr->hull_min[1], hdr->hull_min[2]);
			fprintf (file, "hull_max: %f %f %f\n", hdr->hull_max[0], hdr->hull_max[1], hdr->hull_max[2]);
		//	fprintf (file, "bbmin: %f %f %f\n", hdr->bbmin[0], hdr->bbmin[1], hdr->bbmin[2]);
		//	fprintf (file, "bbmax: %f %f %f\n", hdr->bbmax[0], hdr->bbmax[1], hdr->bbmax[2]);
			fprintf (file, "view_bbmin: %f %f %f\n", hdr->view_bbmin[0], hdr->view_bbmin[1], hdr->view_bbmin[2]);
			fprintf (file, "view_bbmax: %f %f %f\n", hdr->view_bbmax[0], hdr->view_bbmax[1], hdr->view_bbmax[2]);
			
			fprintf (file, "\nflags: %d\n", hdr->flags);
			if ( hdr->flags & STUDIOHDR_FLAGS_AUTOGENERATED_HITBOX )
			{
				fprintf (file, "\tSTUDIOHDR_FLAGS_AUTOGENERATED_HITBOX\n");
			}
			if ( hdr->flags & STUDIOHDR_FLAGS_USES_ENV_CUBEMAP )
			{
				fprintf (file, "\tSTUDIOHDR_FLAGS_USES_ENV_CUBEMAP\n");
			}
			if ( hdr->flags & STUDIOHDR_FLAGS_FORCE_OPAQUE )
			{
				fprintf (file, "\tSTUDIOHDR_FLAGS_FORCE_OPAQUE\n");
			}
			if ( hdr->flags & STUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS )
			{
				fprintf (file, "\tSTUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS\n");
			}
			if ( hdr->flags & STUDIOHDR_FLAGS_STATIC_PROP )
			{
				fprintf (file, "\tSTUDIOHDR_FLAGS_STATIC_PROP\n");
			}
			if ( hdr->flags & STUDIOHDR_FLAGS_USES_FB_TEXTURE )
			{
				fprintf (file, "\tSTUDIOHDR_FLAGS_USES_FB_TEXTURE\n");
			}
			if ( hdr->flags & STUDIOHDR_FLAGS_HASSHADOWLOD )
			{
				fprintf (file, "\tSTUDIOHDR_FLAGS_HASSHADOWLOD\n");
			}
			if ( hdr->flags & STUDIOHDR_FLAGS_USES_BUMPMAPPING )
			{
				fprintf (file, "\tSTUDIOHDR_FLAGS_USES_BUMPMAPPING\n");
			}
			if ( hdr->flags & STUDIOHDR_FLAGS_USE_SHADOWLOD_MATERIALS )
			{
				fprintf (file, "\tSTUDIOHDR_FLAGS_USE_SHADOWLOD_MATERIALS\n");
			}

			fprintf (file, "\nnumbones: %d\n", hdr->numbones);
			for (i = 0; i < hdr->numbones; i++)
			{
				mstudiobone_t *pbones = (mstudiobone_t *) (phdr + hdr->boneindex);
			//	fprintf (file, "\nbone %d.name: \"%s\"\n", i + 1, pbones[i].name);
				fprintf (file, "\n\tbone %d.name: \"%s\"\n", i + 1, pbones[i].pszName());
				fprintf (file, "\tbone %d.parent: %d\n", i + 1, pbones[i].parent);
				fprintf (file, "\tbone %d.flags: %d\n", i + 1, pbones[i].flags);
				fprintf (file, "\tbone %d.bonecontroller: %d %d %d %d %d %d\n", i + 1, pbones[i].bonecontroller[0], pbones[i].bonecontroller[1], pbones[i].bonecontroller[2], pbones[i].bonecontroller[3], pbones[i].bonecontroller[4], pbones[i].bonecontroller[5]);
				fprintf (file, "\tbone %d.value: %f %f %f %f %f %f\n", i + 1, pbones[i].value[0], pbones[i].value[1], pbones[i].value[2], pbones[i].value[3], pbones[i].value[4], pbones[i].value[5]);
				fprintf (file, "\tbone %d.scale: %f %f %f %f %f %f\n", i + 1, pbones[i].scale[0], pbones[i].scale[1], pbones[i].scale[2], pbones[i].scale[3], pbones[i].scale[4], pbones[i].scale[5]);
			}

			fprintf (file, "\nnumbonecontrollers: %d\n", hdr->numbonecontrollers);
			for (i = 0; i < hdr->numbonecontrollers; i++)
			{
				mstudiobonecontroller_t *pbonecontrollers = (mstudiobonecontroller_t *) (phdr + hdr->bonecontrollerindex);
			//	fprintf (file, "\n\tbonecontroller %d.bone: %d\n", i + 1, pbonecontrollers[i].bone);
				mstudiobone_t *pBone = hdr->pBone(pbonecontrollers[i].bone);
				fprintf (file, "\n\tbonecontroller %d.bone: %s\n", i + 1, pBone->pszName());
				fprintf (file, "\tbonecontroller %d.type: %d\n", i + 1, pbonecontrollers[i].type);
				fprintf (file, "\tbonecontroller %d.start: %f\n", i + 1, pbonecontrollers[i].start);
				fprintf (file, "\tbonecontroller %d.end: %f\n", i + 1, pbonecontrollers[i].end);
				fprintf (file, "\tbonecontroller %d.rest: %d\n", i + 1, pbonecontrollers[i].rest);
			//	fprintf (file, "bonecontroller %d.index: %d\n", i + 1, pbonecontrollers[i].index);
				fprintf (file, "\tbonecontroller %d.inputfield: %d\n", i + 1, pbonecontrollers[i].inputfield);
			}

		/*
			fprintf (file, "\nnumhitboxes: %d\n", hdr->numhitboxes);
			for (i = 0; i < hdr->numhitboxes; i++)
			{
				mstudiobbox_t *pbboxes = (mstudiobbox_t *) (phdr + hdr->hitboxindex);
				fprintf (file, "\nhitbox %d.bone: %d\n", i + 1, pbboxes[i].bone);
				fprintf (file, "hitbox %d.group: %d\n", i + 1, pbboxes[i].group);
				fprintf (file, "hitbox %d.bbmin: %f %f %f\n", i + 1, pbboxes[i].bbmin[0], pbboxes[i].bbmin[1], pbboxes[i].bbmin[2]);
				fprintf (file, "hitbox %d.bbmax: %f %f %f\n", i + 1, pbboxes[i].bbmax[0], pbboxes[i].bbmax[1], pbboxes[i].bbmax[2]);
			}
		*/
			fprintf (file, "\nhitbox sets: %d\n", hdr->numhitboxsets);
			for (i = 0; i < hdr->numhitboxsets; i++)
			{
				fprintf (file, "\n\thitbox set: %d\n", i + 1);
			//	mstudiohitboxset_t *pbboxesset = (mstudiohitboxset_t *) (phdr + hdr->hitboxsetindex);
				mstudiohitboxset_t *pbboxesset = hdr->pHitboxSet( i );
				fprintf (file, "\tnumhitboxes: %d\n", pbboxesset->numhitboxes);
				for ( int j = 0; j < pbboxesset->numhitboxes; j++)
				{
				//	mstudiobbox_t *pbboxes = (mstudiobbox_t *) (phdr + pbboxesset->hitboxindex);
				//	fprintf (file, "\n\t\thitbox %d.bone: %d\n", i + 1, pbboxes[i].bone);
				//	fprintf (file, "\t\thitbox %d.group: %d\n", i + 1, pbboxes[i].group);

					mstudiobbox_t *pbboxes = pbboxesset->pHitbox( j );
					mstudiobone_t *pBone = hdr->pBone(pbboxes->bone);
					fprintf (file, "\n\t\thitbox %d.name: \"%s\"\n", i + 1, pBone->pszName());
				//	fprintf (file, "\n\t\thitbox %d.name2: %s\n", j + 1, pbboxes->pszHitboxName()); // VXP: Crashing
					fprintf (file, "\t\thitbox %d.surface: \"%s\"\n", i + 1, pBone->pszSurfaceProp());

					fprintf (file, "\t\thitbox %d.bbmin: %f %f %f\n", j + 1, pbboxes[j].bbmin[0], pbboxes[j].bbmin[1], pbboxes[j].bbmin[2]);
					fprintf (file, "\t\thitbox %d.bbmax: %f %f %f\n", j + 1, pbboxes[j].bbmax[0], pbboxes[j].bbmax[1], pbboxes[j].bbmax[2]);
				}
			}

			fprintf (file, "\nnumseq: %d\n", hdr->numseq);
			for (i = 0; i < hdr->numseq; i++)
			{
				mstudioseqdesc_t *pseqdescs = (mstudioseqdesc_t *) (phdr + hdr->seqindex);
			//	fprintf (file, "\nseqdesc %d.label: \"%s\"\n", i + 1, pseqdescs[i].label);
				fprintf (file, "\n\tseqdesc %d.label: \"%s\"\n", i + 1, pseqdescs[i].pszLabel());
			//	fprintf (file, "seqdesc %d.fps: %f\n", i + 1, pseqdescs[i].fps); // VXP: Fix later
				fprintf (file, "\tseqdesc %d.activity: \"%s\"\n", i + 1, pseqdescs[i].pszActivityName());
				fprintf (file, "\tseqdesc %d.flags: %d\n", i + 1, pseqdescs[i].flags);
				fprintf (file, "\tseqdesc %d.numevents: %d\n", i + 1, pseqdescs[i].numevents);

				for (int j = 0; j < pseqdescs[i].numevents; j++)
				{
					mstudioevent_t *pevent = pseqdescs[i].pEvent( j );
				//	mstudioevent_t *pevent = (mstudioevent_t *) (phdr + pseqdescs[i].eventindex);
					fprintf (file, "\t\tevent %d.cycle: %f\n", j + 1, pevent->cycle);
					fprintf (file, "\t\tevent %d.event: %d\n", j + 1, pevent->event); // VXP: Translate from npcevent.h?
					fprintf (file, "\t\tevent %d.type: %d\n", j + 1, pevent->type);
					fprintf (file, "\t\tevent %d.options: %s\n\n", j + 1, pevent->options);

				}

				fprintf (file, "\tseqdesc %d.numblends: %d\n", i + 1, pseqdescs[i].numblends);
				fprintf (file, "\tseqdesc %d.numikrules: %d\n", i + 1, pseqdescs[i].numikrules);
				fprintf (file, "\tseqdesc %d.numautolayers: %d\n", i + 1, pseqdescs[i].numautolayers);
				fprintf (file, "\tseqdesc %d.numiklocks: %d\n", i + 1, pseqdescs[i].numiklocks);
				fprintf (file, "\tseqdesc %d.keyvaluesize: %d\n", i + 1, pseqdescs[i].keyvaluesize);
				if ( pseqdescs[i].keyvaluesize > 0 )
					fprintf (file, "\tseqdesc %d.KeyValueText: %s\n", i + 1, pseqdescs[i].KeyValueText());
			//	fprintf (file, "<...>\n");
			}
		/*
			fprintf (file, "\nnumseqgroups: %d\n", hdr->numseqgroups);
			for (i = 0; i < hdr->numseqgroups; i++)
			{
				mstudioseqgroup_t *pseqgroups = (mstudioseqgroup_t *) (phdr + hdr->seqgroupindex);
				fprintf (file, "\nseqgroup %d.label: \"%s\"\n", i + 1, pseqgroups[i].label);
				fprintf (file, "\nseqgroup %d.namel: \"%s\"\n", i + 1, pseqgroups[i].name);
				fprintf (file, "\nseqgroup %d.data: %d\n", i + 1, pseqgroups[i].data);
			}
		*/

			fprintf (file, "\nnumcdtextures: %d\n", hdr->numcdtextures);
			for( i = 0; i < hdr->numcdtextures; i++ )
			{
				fprintf (file, "\n\ttexturepath %d.name: \"%s\"\n", i + 1, hdr->pCdtexture( i ));
			}

		//	hdr = g_pStudioModel->getTextureHeader ();
			fprintf (file, "\nnumtextures: %d\n", hdr->numtextures);
		//	fprintf (file, "textureindex: %d\n", hdr->textureindex); // VXP: Useless
		//	fprintf (file, "texturedataindex: %d\n", hdr->texturedataindex);
			for (i = 0; i < hdr->numtextures; i++)
			{
			//	mstudiotexture_t *ptextures = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex);
			//	fprintf (file, "\n\ttexture %d.name: \"%s%s.vmt\"\n", i + 1, hdr->pCdtexture( i ), ptextures[i].pszName());
			//	fprintf (file, "\ntexture %d.name: \"%s\"\n", i + 1, ptextures[i].name);
			//	fprintf (file, "\ntexture %d.name: \"%s\"\n", i + 1, ptextures[i].pszName());
				mstudiotexture_t *ptextures = hdr->pTexture( i );
			//	fprintf (file, "\n\ttexture %d.name: \"%s%s.vmt\"\n", i + 1, hdr->pCdtexture( i ), ptextures->pszName());
				fprintf (file, "\n\ttexture %d.name: \"%s.vmt\"\n", i + 1, ptextures->pszName());
			//	for( i = 0; i < hdr->numcdtextures; i++ ) // VXP: FIXME
			//	{
			//		fprintf (file, "\n\ttexture %d.name: \"%s%s\"\n", i + 1, hdr->pCdtexture( j ), ptextures->pszName());
			//	}
			//	fprintf (file, "\ttexture %d.flags: %d\n", i + 1, ptextures[i].flags);
			//	if ( ptextures[i].flags > 0 )
			//	{
			//		fprintf (file, "\ttexture %d.flags: %d (maybe, mouth?)\n", i + 1, ptextures[i].flags);
			//	}
			//	fprintf (file, "\ttexture %d.width: %d\n", i + 1, ptextures[i].width);
			//	fprintf (file, "\ttexture %d.height: %d\n", i + 1, ptextures[i].height);
			//	fprintf (file, "texture %d.index: %d\n", i + 1, ptextures[i].index);
			}

		//	hdr = g_pStudioModel->getStudioHeader ();
			fprintf (file, "\nnumskinref: %d\n", hdr->numskinref);
		//	fprintf (file, "numskinfamilies: %d\n", hdr->numskinfamilies);
		/*
			for (i = 0; i < hdr->numskinref; i++) // VXP: FIXME: Unfinished!
			{
				short *pskinref = hdr->pSkinref( i );
			//	mstudiotexture_t *ptexture = hdr->pTexture(pskinref[pStudioMesh->material]);
				mstudiotexture_t *ptexture = hdr->pTexture(pskinref[0]);
				fprintf (file, "\t\tskin %d.name: \"%s\"\n", i + 1, ptexture->pszName());
			}
		*/

			fprintf (file, "\nnumbodyparts: %d\n", hdr->numbodyparts);
			for (i = 0; i < hdr->numbodyparts; i++)
			{
				mstudiobodyparts_t *pbodyparts = (mstudiobodyparts_t *) ((byte *) hdr + hdr->bodypartindex);
			//	fprintf (file, "\nbodypart %d.name: \"%s\"\n", i + 1, pbodyparts[i].name);
				fprintf (file, "\n\tbodypart %d.name: \"%s\"\n", i + 1, pbodyparts[i].pszName());
				fprintf (file, "\tbodypart %d.nummodels: %d\n", i + 1, pbodyparts[i].nummodels);
				fprintf (file, "\tbodypart %d.base: %d\n", i + 1, pbodyparts[i].base);
				fprintf (file, "\tbodypart %d.modelindex: %d\n", i + 1, pbodyparts[i].modelindex);
			}

			fprintf (file, "\nnumattachments: %d\n", hdr->numattachments);
			for (i = 0; i < hdr->numattachments; i++)
			{
				mstudioattachment_t *pattachments = (mstudioattachment_t *) ((byte *) hdr + hdr->attachmentindex);
			//	fprintf (file, "attachment %d.name: \"%s\"\n", i + 1, pattachments[i].name);
				fprintf (file, "\n\tattachment %d.name: \"%s\"\n", i + 1, pattachments[i].pszName());

				mstudiobone_t *pBone = hdr->pBone(pattachments->bone);
				fprintf (file, "\tattachment %d.bone: \"%s\"\n", i + 1, pBone->pszName());
			}

			fprintf (file, "\nsurfaceprop: \"%s\"\n", hdr->pszSurfaceProp());

			fclose (file);

			ShellExecute ((HWND) getHandle (), "open", "midump.txt", 0, 0, SW_SHOW);
		}
	}
//#endif
}


LoadModelResult_t ControlPanel::loadModel(const char *filename)
{
	SaveViewerSettings( g_viewerSettings.modelFile );

	g_pStudioModel->FreeModel( false );

	if (g_pStudioModel->LoadModel( filename ))
	{
		if (g_pStudioModel->PostLoadModel( filename ))
		{
			initSequences();
			initBodyparts();
			initBoneControllers();
			initSkins();
			initTextures();
			initPhysicsBones();
			initLODs();
			initFlexes();

			setModelInfo();

			if (!LoadViewerSettings( filename ))
			{
				InitViewerSettings( "hlmv" );
				centerView();
				g_viewerSettings.sequence = 0;
				setSpeedScale( 1.0 );
			}

			strcpy( g_viewerSettings.modelFile, filename );
			resetControlPanel();

			int i;
			for (i = 0; i < 32; i++)
				g_viewerSettings.submodels[i] = 0;
			for (i = 0; i < 8; i++)
				g_viewerSettings.controllers[i] = 0;

			setupPhysics();
			m_pBoneWindow->OnLoadModel();
			m_pAttachmentsWindow->OnLoadModel();

			mx_setcwd (mx_getpath (filename));

			// VXP
			for (i = 0; i < 4; i++)
			{
				if (g_pStudioExtraModel[i])
				{
					g_pStudioExtraModel[i]->FreeModel( false );
					delete g_pStudioExtraModel[i];
					g_pStudioExtraModel[i] = NULL;
				}
				if (strlen( g_viewerSettings.mergeModelFile[i] ) != 0)
				{
					loadModel( g_viewerSettings.mergeModelFile[i], i );
				}
			}
		}
		else
		{
			return LoadModel_PostLoadFail;
		}
	}
	else
	{
		return LoadModel_LoadFail;
	}

	return LoadModel_Success;
}


LoadModelResult_t ControlPanel::loadModel(const char *filename, int slot ) // VXP
{
	if (slot == -1)
	{
		return loadModel( filename );
	}

	if (g_pStudioExtraModel[slot] == NULL)
	{
		g_pStudioExtraModel[slot] = new StudioModel;
	}
	else
	{
		g_pStudioExtraModel[slot]->FreeModel( false );
	}

	if (g_pStudioExtraModel[slot]->LoadModel( filename ))
	{
		if (g_pStudioExtraModel[slot]->PostLoadModel( filename ))
		{
			return LoadModel_Success;
		}
		else
		{
			return LoadModel_PostLoadFail;
		}
	}

	return LoadModel_LoadFail;
}


void
ControlPanel::resetControlPanel( void )
{
	setSequence( g_viewerSettings.sequence );
	setOverlaySequence( 1, g_viewerSettings.overlaySequence[0], g_viewerSettings.overlayWeight[0] );
	setOverlaySequence( 2, g_viewerSettings.overlaySequence[1], g_viewerSettings.overlayWeight[1] );
	setOverlaySequence( 3, g_viewerSettings.overlaySequence[2], g_viewerSettings.overlayWeight[2] );
	setOverlaySequence( 4, g_viewerSettings.overlaySequence[3], g_viewerSettings.overlayWeight[3] );
	setSpeedScale( g_viewerSettings.speedScale );

	cbGround->setChecked(  g_viewerSettings.showGround );
	cbMovement->setChecked( g_viewerSettings.showMovement );

	cbHitBoxes->setChecked( g_viewerSettings.showHitBoxes );
	cbBones->setChecked( g_viewerSettings.showBones );
	cbSequenceBoxes->setChecked( g_viewerSettings.showSequenceBoxes );
	cbRunIK->setChecked( g_viewerSettings.enableIK );

	cbBackground->setChecked( g_viewerSettings.showBackground );
	cbSoftwareSkin->setChecked( g_viewerSettings.softwareSkin );
	cbOverbright2->setChecked( g_viewerSettings.overbright );
	cbAttachments->setChecked( g_viewerSettings.showAttachments );
	cbNormals->setChecked( g_viewerSettings.showNormals );
	cbEnableHead->setChecked( g_viewerSettings.solveHeadTurn ? 1 : 0 );
}


void
ControlPanel::setRenderMode (int mode)
{
	g_viewerSettings.renderMode = mode;
	d_MatSysWindow->redraw ();
}


void 
ControlPanel::setHighlightBone( int index )
{
	if ( index >= 0 )
	{
		g_viewerSettings.highlightPhysicsBone = index;
	}
}

void
ControlPanel::setLOD( int index, bool setLODchoice, bool force )
{
#if 1
	if( !force && ( g_viewerSettings.lod == index ) )
	{
		return;
	}
#endif
	g_viewerSettings.lod = index;
	float lodSwitch = g_pStudioModel->GetLODSwitchValue( index );
	char tmp[128];
	sprintf( tmp, "%0.0f", lodSwitch );
	leLODSwitch->setLabel( tmp );
	HWND wnd = ( HWND )leLODSwitch->getHandle();
	if( setLODchoice )
	{
		cLODChoice->select( index );
	}
	InvalidateRect( wnd, NULL, TRUE );
	UpdateWindow( wnd );
}

void
ControlPanel::setLODMetric( float metric )
{
	static int saveMetric = -10;
	int intMetric = ( int )metric;
	if( intMetric == saveMetric )
	{
		return;
	}
	saveMetric = intMetric;
	char tmp[128];
	sprintf( tmp, "%d", intMetric );
	lLODMetric->setLabel( tmp );
}

void
ControlPanel::setPolycount( int polycount )
{
	static int savePolycount = -10;
	if( polycount == savePolycount )
	{
		return;
	}
	savePolycount = polycount;
	char tmp[128];
	sprintf( tmp, "Polycount: %d", polycount );
	lModelInfo3->setLabel( tmp );
}

void
ControlPanel::updatePoseParameters( )
{
	for (int i = 0; i < NUM_POSEPARAMETERS; i++)
	{
		if (slPoseParameter[i]->isEnabled())
		{
			int j = cPoseParameter[i]->getSelectedIndex();
			float value = g_pStudioModel->GetPoseParameter( j );

			slPoseParameter[i]->setValue( value );
			laPoseParameter[i]->setLabel( "%.1f", value );
		}
	}
}

void
ControlPanel::setShowGround (bool b)
{
	g_viewerSettings.showGround = b;
	cbGround->setChecked (b);
}

void
ControlPanel::setAutoLOD( bool b )
{
	g_viewerSettings.autoLOD = b;
	cbAutoLOD->setChecked( b );
}

void
ControlPanel::setSoftwareSkin( bool b )
{
	g_viewerSettings.softwareSkin = b;
	cbSoftwareSkin->setChecked( b );
}

void
ControlPanel::setOverbright( bool b )
{
	g_viewerSettings.overbright = b;
	cbOverbright2->setChecked( b );
}

void
ControlPanel::setShowMovement (bool b)
{
	g_viewerSettings.showMovement = b;
	cbMovement->setChecked (b);
}



void
ControlPanel::setShowBackground (bool b)
{
	g_viewerSettings.showBackground = b;
	cbBackground->setChecked (b);
}



void
ControlPanel::initSequences ()
{
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
	if (hdr)
	{
		for (int i = 0; i < MAX_SEQUENCES; i++)
		{
			cSequence[i]->removeAll ();
			for (int j = 0; j < hdr->numseq; j++)
			{
				// VXP: TODO: Implement proper Source 2007 "Show Hidden" HLMV option
				if ( !(hdr->pSeqdesc(j)->flags & STUDIO_HIDDEN) )
				{
				//	cSequence[i]->add( hdr->pSeqdesc(j)->pszLabel() );
					if ( Q_strcmp( hdr->pSeqdesc(j)->pszActivityName(), "" ) == 0 )
					{
						cSequence[i]->add( hdr->pSeqdesc(j)->pszLabel() );
					}
					else
					{
						char buf[256];
						Q_snprintf( buf, sizeof( buf ), "%s - %s", hdr->pSeqdesc(j)->pszLabel(), hdr->pSeqdesc(j)->pszActivityName() );
						cSequence[i]->add( buf );
					}
				}
			}
			cSequence[i]->select( 0 );
			slSequence[i]->setValue( 0 );
		}
	}

	float flMin, flMax;
	for (int i = 0; i < NUM_POSEPARAMETERS; i++)
	{
		if (g_pStudioModel->GetPoseParameterRange( i, &flMin, &flMax ))
		{
			cPoseParameter[i]->removeAll();
			for (int j = 0; j < hdr->numposeparameters; j++)
			{
				cPoseParameter[i]->add( hdr->pPoseParameter(j)->pszName() );
			}
			cPoseParameter[i]->select( i );
			cPoseParameter[i]->setEnabled( true );
			cPoseParameter[i]->setVisible( true );

			slPoseParameter[i]->setEnabled( true );
			slPoseParameter[i]->setRange( flMin, flMax );
			mxToolTip::add (slPoseParameter[i], hdr->pPoseParameter(i)->pszName() );
			slPoseParameter[i]->setVisible( true );
			laPoseParameter[i]->setVisible( true );
			laPoseParameter[i]->setLabel( "%.1f", 0.0 );
		}
		else
		{
			cPoseParameter[i]->setEnabled( false );
			cPoseParameter[i]->setVisible( false );
			slPoseParameter[i]->setEnabled( false );
			slPoseParameter[i]->setVisible( false );
			laPoseParameter[i]->setVisible( false );
		}
		slPoseParameter[i]->setValue( 0.0 );
		setBlend( i, 0.0 );
	}

	for (i = 0; i < hdr->numposeparameters; i++)
	{
		setBlend( i, 0.0 );
	}

}


void ControlPanel::setSequence(int index)
{
	cSequence[0]->select (index);
	g_pStudioModel->SetSequence(index);
	g_viewerSettings.sequence = index;

	updateFrameSelection();

	char sz[100];
	float flGroundSpeed = g_pStudioModel->GetGroundSpeed();
	sprintf( sz, "Speed: %.2f", flGroundSpeed );
	laGroundSpeed->setLabel( sz );
}


void
ControlPanel::setOverlaySequence(int num, int index, float weight)
{
	cSequence[num]->select( index );
	g_pStudioModel->SetOverlaySequence( num-1, index, weight );
	g_viewerSettings.overlaySequence[num-1] = index;
	g_viewerSettings.overlayWeight[num-1] = weight;
	slSequence[num]->setValue( weight );

	updateFrameSelection();
}


void ControlPanel::startBlending( void )
{
	g_pStudioModel->StartBlending();
}

void ControlPanel::updateTransitionAmount( void )
{
	char sz[ 256 ];
	sprintf( sz, "%.3f %%", 100.0f * g_pStudioModel->GetTransitionAmount() );
	laBlendAmount->setLabel( sz );
}


int ControlPanel::getFrameSelection( void )
{
	for ( int i = 0; i < MAX_SEQUENCES; i++ )
		if ( rbFrameSelection[i]->isChecked() )
			return i;
	return 0;
}

void ControlPanel::setFrame( float frame )
{
	int iLayer = getFrameSelection();
	int iFrame = g_pStudioModel->SetFrame( iLayer, frame );
	char buf[128];
	sprintf(buf, "%3d", iFrame );
	lForcedFrame->setLabel( buf );
}


void ControlPanel::updateFrameSelection( void )
{
	int iLayer = getFrameSelection();
	int maxFrame = g_pStudioModel->GetMaxFrame( iLayer );
	slForceFrame->setRange( 0, maxFrame, maxFrame );
	slForceFrame->setSteps(1,1);
}

void ControlPanel::updateFrameSlider( void )
{
	int iLayer = getFrameSelection();
	float flFrame = g_pStudioModel->GetFrame( iLayer );
	char buf[128];
	sprintf(buf, "%3.1f", flFrame );
	lForcedFrame->setLabel( buf );
	slForceFrame->setValue( flFrame );
}

void ControlPanel::setSpeedScale( float scale )
{
	slSpeedScale->setValue( scale );
	g_viewerSettings.speedScale = scale;

	updateSpeedScale( );
}


void ControlPanel::updateSpeedScale( void )
{
	char szFPS[32];
	sprintf( szFPS, "x %.2f = %.1f fps", g_viewerSettings.speedScale, g_viewerSettings.speedScale * g_pStudioModel->GetFPS( ) );
	laFPS->setLabel( szFPS );
}


void ControlPanel::setBlend(int index, float value )
{
	g_pStudioModel->SetPoseParameter( index, value );
	// reset number of frames....
	slForceFrame->setRange( 0, g_pStudioModel->GetMaxFrame() );
}

void
ControlPanel::initBodyparts ()
{
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
	if (hdr)
	{
		int i;
		mstudiobodyparts_t *pbodyparts = hdr->pBodypart(0);

		cBodypart->removeAll ();
		if (hdr->numbodyparts > 0)
		{
			for (i = 0; i < hdr->numbodyparts; i++)
				cBodypart->add (pbodyparts[i].pszName());

			cBodypart->select (0);

			cSubmodel->removeAll ();
			for (i = 0; i < pbodyparts[0].nummodels; i++)
			{
				char str[64];
				sprintf (str, "Submodel %d", i + 1);
				cSubmodel->add (str);
			}
			cSubmodel->select (0);
		}
	}
}



void
ControlPanel::setBodypart (int index)
{
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
	if (hdr)
	{
		//cBodypart->setEn
		cBodypart->select (index);
		if (index < hdr->numbodyparts)
		{
			mstudiobodyparts_t *pbodyparts = hdr->pBodypart(0);
			cSubmodel->removeAll ();
		
			for (int i = 0; i < pbodyparts[index].nummodels; i++)
			{
				char str[64];
				sprintf (str, "Submodel %d", i + 1);
				cSubmodel->add (str);
			}
			cSubmodel->select (0);
			//g_pStudioModel->SetBodygroup (index, 0);
		}
	}
}



void
ControlPanel::setSubmodel (int index)
{
	g_pStudioModel->SetBodygroup (cBodypart->getSelectedIndex (), index);
	g_viewerSettings.submodels[cBodypart->getSelectedIndex ()] = index;
}



void 
ControlPanel::initPhysicsBones()
{
	cHighlightBone->removeAll();
	cHighlightBone->add( "None" );
	for ( int i = 0; i < g_pStudioModel->Physics_GetBoneCount(); i++ )
	{
		cHighlightBone->add (g_pStudioModel->Physics_GetBoneName( i ) );
	}
	cHighlightBone->select (0);
}

void 
ControlPanel::initLODs()
{
	cLODChoice->removeAll();
	for ( int i = 0; i < g_pStudioModel->GetNumLODs(); i++ )
	{
		char tmp[10];
		sprintf( tmp, "%d", i );
		cLODChoice->add( tmp );
	}
	setLOD( 0, true, true );
}

void
ControlPanel::initBoneControllers ()
{
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
	if (hdr)
	{
		cController->setEnabled (hdr->numbonecontrollers > 0);
		slController->setEnabled (hdr->numbonecontrollers > 0);
		cController->removeAll ();

		mstudiobonecontroller_t *pbonecontrollers = hdr->pBonecontroller(0);
		for (int i = 0; i < hdr->numbonecontrollers; i++)
		{
			char str[32];
			sprintf (str, "Controller %d", pbonecontrollers[i].inputfield);
			cController->add (str);
		}

		if (hdr->numbonecontrollers > 0)
		{
			cController->select (0);
			slController->setRange (pbonecontrollers[0].start, pbonecontrollers[0].end);
			slController->setValue (0);
		}

	}
}



void
ControlPanel::setBoneController (int index)
{
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
	if (hdr)
	{
		mstudiobonecontroller_t *pbonecontrollers = hdr->pBonecontroller(0);
		slController->setRange ( pbonecontrollers[index].start, pbonecontrollers[index].end);
		slController->setValue (0);
	}
}



void
ControlPanel::setBoneControllerValue (int index, float value)
{
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
	if (hdr)
	{
		mstudiobonecontroller_t *pbonecontrollers = hdr->pBonecontroller(0);
		g_pStudioModel->SetController (pbonecontrollers[index].inputfield, value);
	
		g_viewerSettings.controllers[index] = value;
	}
}



void
ControlPanel::initSkins ()
{
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
	if (hdr)
	{
		cSkin->setEnabled (hdr->numskinfamilies > 0);
		cSkin->removeAll ();

		for (int i = 0; i < hdr->numskinfamilies; i++)
		{
			char str[32];
			sprintf (str, "Skin %d", i + 1);
			cSkin->add (str);
		}

		cSkin->select (0);
		g_pStudioModel->SetSkin (0);
		g_viewerSettings.skin = 0;
	}
}



void
ControlPanel::setModelInfo ()
{
	static char str[2048];
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();

	if (!hdr)
		return;

	int hbcount = 0;
	for ( int s = 0; s < hdr->numhitboxsets; s++ )
	{
		hbcount += hdr->iHitboxCount( s );
	}

	sprintf (str,
		"Bones: %d\n"
		"Bone Controllers: %d\n"
		"Hit Boxes: %d in %d sets\n"
		"Sequences: %d\n"
		"Sequence Groups: %d\n",
		hdr->numbones,
		hdr->numbonecontrollers,
		hbcount,
		hdr->numhitboxsets,
		hdr->numseq,
		hdr->numseqgroups
		);

	lModelInfo1->setLabel (str);

	sprintf (str,
		"Textures: %d\n"
		"Skin Families: %d\n"
		"Bodyparts: %d\n"
		"Attachments: %d\n"
		"Transitions: %d\n",
		hdr->numtextures,
		hdr->numskinfamilies,
		hdr->numbodyparts,
		hdr->numattachments,
		hdr->numtransitions);

	lModelInfo2->setLabel (str);
}


void
ControlPanel::initTextures ()
{
#if 0
	studiohdr_t *hdr = g_pStudioModel->getTextureHeader ();
	if (hdr)
	{
		cTextures->removeAll ();
		mstudiotexture_t *ptextures = hdr->pTexture(0);
		for (int i = 0; i < hdr->numtextures; i++)
			cTextures->add ( hdr->pTexturename(i) );
		cTextures->select (0);
		g_viewerSettings.texture = 0;
		if (hdr->numtextures > 0)
			cbChrome->setChecked ((ptextures[0].flags & STUDIO_NF_CHROME) == STUDIO_NF_CHROME);
	}
#endif
}


void
ControlPanel::centerView ()
{
	Vector min, max;
	g_pStudioModel->ExtractBbox(min, max);

	float dx = max[0] - min[0];
	float dy = max[1] - min[1];
	float dz = max[2] - min[2];

	// Determine the distance from the model such that it will fit in the screen.
	float d = dx;
	if (dy > d)
		d = dy;
	if (dz > d)
		d = dz;

	g_viewerSettings.trans[0] = d * 1.34f;
	g_viewerSettings.trans[1] = 0.0f;
	g_viewerSettings.trans[2] = min[2] + dz / 2;
	g_viewerSettings.rot[0] = 0.0f;
	g_viewerSettings.rot[1] = 0.0f;
	g_viewerSettings.rot[2] = 0.0f;
	g_viewerSettings.lightrot[0] = 0.0f;
	g_viewerSettings.lightrot[1] = 180.0f; // light should aim at models front
	g_viewerSettings.lightrot[2] = 0.0f;
	setFOV( 65.0f );
	d_MatSysWindow->redraw ();
}

void ControlPanel::viewmodelView()
{
	// Sit the camera at the origin with a 54 degree FOV for viewmodels
	g_viewerSettings.trans[0] = 0.0f;
	g_viewerSettings.trans[1] = 0.0f;
	g_viewerSettings.trans[2] = 0.0f;
	g_viewerSettings.rot[0] = 0.0f;
	g_viewerSettings.rot[1] = 180.0f;
	g_viewerSettings.rot[2] = 0.0f;
	g_viewerSettings.lightrot[0] = 0.0f;
	g_viewerSettings.lightrot[1] = 180.0f; // light should aim at models front
	g_viewerSettings.lightrot[2] = 0.0f;
	setFOV( 54.0f );
	d_MatSysWindow->redraw ();
}

void ControlPanel::setFOV( float fov )
{
	char buf[64];
	g_viewerSettings.fov = fov;
	sprintf( buf, "%.0f", fov );
	leFOV->setLabel( buf );
}

void
ControlPanel::initFlexes ()
{
	studiohdr_t *hdr = g_pStudioModel->getStudioHeader ();
	if (hdr)
	{
		for (int j = 0; j < NUM_FLEX_SLIDERS; j++)
		{
			cFlex[j]->removeAll ();
			for (int i = 0; i < hdr->numflexcontrollers; i++)
			{
				cFlex[j]->add( hdr->pFlexcontroller(i)->pszName() );
			}
			cFlex[j]->select( hdr->numflexcontrollers - NUM_FLEX_SLIDERS + j );
		}
	}

	for (int i = 0; i < hdr->numflexcontrollers; i++)
	{
		g_pStudioModel->SetFlexController( i, 0.0 );
	}
}


static void UpdateSliderLabel( mxSlider *slider, mxLabel *label )
{
	float value = slider->getValue();
	char buf[80];
	sprintf( buf, "%0.2f", value );
	label->setLabel( buf );
}

void ControlPanel::UpdateConstraintSliders( int clamp )
{
	float vmin = slPhysicsConMin->getValue();
	float vmax = slPhysicsConMax->getValue();

	// reflect mins/maxs
	if ( cbLinked->isChecked() )
	{
		if ( clamp == IDC_PHYS_CON_MIN && vmin <= 0 )
		{
			vmax = -vmin;
		}
		else if ( vmax >= 0 )
		{
			vmin = -vmax;
		}
	}

	if ( vmax < vmin )
	{
		if ( clamp == IDC_PHYS_CON_MIN )
			vmin = vmax;
		else
			vmax = vmin;
	}

	slPhysicsConMin->setValue( vmin );
	slPhysicsConMax->setValue( vmax );
	UpdateSliderLabel( slPhysicsConMin, lPhysicsConMin );
	UpdateSliderLabel( slPhysicsConMax, lPhysicsConMax );
	UpdateSliderLabel( slPhysicsFriction, lPhysicsFriction );
}


int ControlPanel::handlePhysicsEvent( mxEvent *event )
{
	switch( event->action )
	{
	case IDC_PHYS_BONE:
		setupPhysicsBone( cPhysicsBone->getSelectedIndex() );
		break;
	case IDC_PHYS_CON_AXIS_X:
	case IDC_PHYS_CON_AXIS_Y:
	case IDC_PHYS_CON_AXIS_Z:
		setupPhysicsAxis( cPhysicsBone->getSelectedIndex(), event->action - IDC_PHYS_CON_AXIS_X );
		break;
	case IDC_PHYS_CON_TYPE_FREE:
	case IDC_PHYS_CON_TYPE_FIXED:
	case IDC_PHYS_CON_TYPE_LIMIT:
		break;
	case IDC_PHYS_CON_MIN:
	case IDC_PHYS_CON_MAX:
	case IDC_PHYS_CON_FRICTION:
		UpdateConstraintSliders( event->action );
		break;
	case IDC_PHYS_CON_TEST:
		g_pStudioModel->Physics_SetPreview( cPhysicsBone->getSelectedIndex(), getPhysicsAxis(), slPhysicsConTest->getValue() );
		break;
	case IDC_PHYS_P_MASSBIAS:
		UpdateSliderLabel( slPhysicsParamMassBias, lPhysicsParamMassBias );
		break;
	case IDC_PHYS_P_INERTIA:
		UpdateSliderLabel( slPhysicsParamInertia, lPhysicsParamInertia );
		break;
	case IDC_PHYS_P_DAMPING:
		UpdateSliderLabel( slPhysicsParamDamping, lPhysicsParamDamping );
		break;
	case IDC_PHYS_P_ROT_DAMPING:
		UpdateSliderLabel( slPhysicsParamRotDamping, lPhysicsParamRotDamping );
		break;
	case IDC_PHYS_QCFILE:
		{
			writePhysicsData();
			char *pOut = g_pStudioModel->Physics_DumpQC();
			if ( pOut )
			{
				Sys_CopyStringToClipboard( pOut );
			}
			delete[] pOut;
			return 1;
		}
		break;

	}
	writePhysicsData();
	return 1;
}


void ControlPanel::handlePhysicsKey( mxEvent *event )
{
	if ( event->key == '[' || event->key == ']' )
	{
		int boneIndex = cPhysicsBone->getSelectedIndex();
		int boneCount = cPhysicsBone->getItemCount();
		int axisIndex = getPhysicsAxis();
		int axisCount = 3;

		if ( event->key == '[' )
		{
			axisIndex = (axisIndex+axisCount-1) % axisCount;
			if ( axisIndex == (axisCount-1) )
			{
				boneIndex = (boneIndex+boneCount-1) % boneCount;
			}
		}
		else
		{
			axisIndex = (axisIndex+1) % axisCount;
			if ( axisIndex == 0 )
			{
				boneIndex = (boneIndex+1) % boneCount;
			}
		}
		setPhysicsAxis( axisIndex );
		cPhysicsBone->select( boneIndex );
		setupPhysicsBone( boneIndex );
	}
}


void ControlPanel::setupPhysics( void )
{
	if (!PopulatePhysicsBoneList( cPhysicsBone) )
		return;

	cPhysicsBone->select (0);
	setPhysicsAxis(0);
	setupPhysicsBone( 0 );

	char buf[64];
	sprintf( buf, "%.2f", g_pStudioModel->Physics_GetMass() );
	leMass->setLabel( buf );

	// Default to "None"
	setHighlightBone(0);
}

static void SetSlider( mxSlider *slider, mxLabel *label, float value )
{
	slider->setValue( value );
	UpdateSliderLabel( slider, label );
}

int ControlPanel::getPhysicsAxis( void )
{
	for ( int i = 0; i < 3; i++ )
		if ( rbConstraintAxis[i]->isChecked() )
			return i;

	return 0;
}


void ControlPanel::setPhysicsAxis( int axisIndex )
{
	for ( int i = 0; i < 3; i++ )
	{
		rbConstraintAxis[i]->setChecked( (i==axisIndex)?true : false );
	}
}


void ControlPanel::setupPhysicsBone( int boneIndex )
{
	if (!g_pStudioModel->m_pPhysics)
		return;

	int axisIndex = getPhysicsAxis();
	setupPhysicsAxis( boneIndex, axisIndex );
	// read per-bone data
	hlmvsolid_t solid;
	g_pStudioModel->Physics_GetData( boneIndex, &solid, NULL );
	
	SetSlider( slPhysicsParamMassBias, lPhysicsParamMassBias, solid.massBias );
	SetSlider( slPhysicsParamInertia, lPhysicsParamInertia, solid.params.inertia );
	SetSlider( slPhysicsParamDamping, lPhysicsParamDamping, solid.params.damping );
	SetSlider( slPhysicsParamRotDamping, lPhysicsParamRotDamping, solid.params.rotdamping );

	// Find the bone
	studiohdr_t* pHdr = g_pStudioModel->getStudioHeader();
	for ( int i = 0; i < pHdr->numbones; i++ )
	{
		mstudiobone_t* pBone = pHdr->pBone(i);
		if (!stricmp(pBone->pszName(), solid.name ))
		{
			// Once found, set the surface property accordingly
			lPhysicsMaterial->setLabel( g_pStudioModel->m_SurfaceProps[i].String() );
			break;
		}
	}

	// select the render/highlight bone
	setHighlightBone( boneIndex + 1 );

	redraw();
}


void ControlPanel::setupPhysicsAxis( int boneIndex, int axis )
{
	// read the per-axis data
	constraint_ragdollparams_t constraint;

	g_pStudioModel->Physics_GetData( boneIndex, NULL, &constraint );
	setPhysicsAxis( axis );
	SetSlider( slPhysicsConMin, lPhysicsConMin, constraint.axes[axis].minRotation );
	SetSlider( slPhysicsConMax, lPhysicsConMax, constraint.axes[axis].maxRotation );
	SetSlider( slPhysicsFriction, lPhysicsFriction, constraint.axes[axis].torque );
	g_pStudioModel->Physics_SetPreview( -1, 0, 0 );
	redraw();
}

void ControlPanel::writePhysicsData( void )
{
	int boneIndex = cPhysicsBone->getSelectedIndex();
	int axis = getPhysicsAxis();

	hlmvsolid_t	solid;
	constraint_ragdollparams_t	constraint;
	// read the existing data
	g_pStudioModel->Physics_GetData( boneIndex, &solid, &constraint );

	const char *pMass = leMass->getLabel();
	float mass = 0;
	if ( pMass )
	{
		mass = atof( pMass );
	}

	g_pStudioModel->Physics_SetMass( mass );

	// write back the data we're editing on this dialog
	constraint.axes[axis].minRotation = slPhysicsConMin->getValue();
	constraint.axes[axis].maxRotation = slPhysicsConMax->getValue();
	constraint.axes[axis].torque = slPhysicsFriction->getValue();

	solid.massBias = slPhysicsParamMassBias->getValue();
	solid.index = boneIndex;
	//solid.params.mass;
	solid.params.inertia = slPhysicsParamInertia->getValue();
	solid.params.damping = slPhysicsParamDamping->getValue();
	solid.params.rotdamping = slPhysicsParamRotDamping->getValue();

	// store it in the model
	g_pStudioModel->Physics_SetData( boneIndex, &solid, &constraint );
}
