//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "DemoPage.h"

#include <VGUI_IVGui.h>
#include <VGUI_Controls.h>
#include <VGUI_KeyValues.h>
#include <VGUI_FileOpenDialog.h>

using namespace vgui;


class FileOpenDemo: public DemoPage
{
	public:
		FileOpenDemo(Panel *parent, const char *name);
		~FileOpenDemo();

		void SetVisible(bool status);
	
	private:
		void OnFileSelected(const char *fullpath);

		DHANDLE<FileOpenDialog> m_hFileDialog;

		DECLARE_PANELMAP();		
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
FileOpenDemo::FileOpenDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
FileOpenDemo::~FileOpenDemo()
{
}

//-----------------------------------------------------------------------------
// Purpose: When we make this this demo page visible we make the dialog visible.
//-----------------------------------------------------------------------------
void FileOpenDemo::SetVisible(bool status)
{
	if (status)
	{
		if (!m_hFileDialog.Get())
		{
			// Pop up the dialog
			FileOpenDialog *pFileDialog = new FileOpenDialog (NULL, "Find the TestFile");
			m_hFileDialog = pFileDialog;
			m_hFileDialog->AddActionSignalTarget(this);
		}
		m_hFileDialog->DoModal(false);
	}	
}

//-----------------------------------------------------------------------------
// Purpose: When a file is selected print out its full path in the debugger
//-----------------------------------------------------------------------------
void FileOpenDemo::OnFileSelected(const char *fullpath)
{
	ivgui()->DPrintf("File selected\n");
	ivgui()->DPrintf(fullpath);
	ivgui()->DPrintf("\n");
}

MessageMapItem_t FileOpenDemo::m_MessageMap[] =
{
	MAP_MESSAGE_CONSTCHARPTR(FileOpenDemo, "FileSelected", OnFileSelected, "fullpath"), 
};

IMPLEMENT_PANELMAP(FileOpenDemo, BaseClass);


Panel* FileOpenDemo_Create(Panel *parent)
{
	return new FileOpenDemo(parent, "FileOpenDialogDemo");
}


