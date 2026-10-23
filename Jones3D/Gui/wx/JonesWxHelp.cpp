// Help viewer (Added): the game's help is a WinHelp file (Jones3D.hlp), or an HTML Help file (Jones3D.chm) in the GOG release.
// Current systems don't come with a WinHelp viewer, the help file is shown in the wxHTML help viewer: the WinHelp file
// converted to an HTML book (JonesWxHlp.cpp), the CHM file unpacked with libmspack (JonesWxChm.cpp). On Windows too, as
// Windows' own HTML Help viewer refuses CHM files in many places. It runs in its own process (the game executable with -help <file>),
// as the game's event loop doesn't run wx events while the game is played.
#include "JonesWx.h"
#include "JonesWxHlp.h"

#include <Jones3D/Gui/wx/JonesWxDialogs.h>

#if wxUSE_WXHTML_HELP
#include <wx/filename.h>
#include <wx/html/helpctrl.h>
#include <wx/html/helpfrm.h>
#include <wx/html/helpwnd.h>
#include <wx/treectrl.h>
#endif

#include <stdio.h>

extern "C" int JonesWx_ShowHelp(const char* pFilePath)
{
#if wxUSE_WXHTML_HELP
    if ( !JonesWx::EnsureInit() ) {
        return 1;
    }

    const wxString filePath = wxString::FromUTF8(pFilePath);
    int result = 0;
    {
        // Contents, index and search in a frame, closing it ends the main loop
        wxHtmlHelpController help(wxHF_TOOLBAR | wxHF_CONTENTS | wxHF_INDEX | wxHF_SEARCH | wxHF_PRINT, nullptr);
        help.SetTitleFormat("%s");
        help.SetShouldPreventAppExit(false);

        wxLogNull noLog; // The books have no full-text search index, wx reports that
        bool bLoaded = false;
        wxString error;
        if ( wxFileName(filePath).GetExt().IsSameAs("hlp", false) )
        {
            const wxString book = JonesWxHlp::LoadBook(filePath, &error);
            bLoaded = !book.empty() && help.AddBook(book);
        }
        else
        {
            const wxString book = JonesWxHlp::LoadChmBook(filePath, &error);
            bLoaded = !book.empty() && help.AddBook(book);
        }

        if ( !bLoaded )
        {
            fprintf(stderr, "ERROR: Couldn't open the help file %s%s%s\n", pFilePath, error.empty() ? "" : ": ", (const char*)error.utf8_str());
            result = 1;
        }
        else if ( help.DisplayContents() )
        {
            if ( wxHtmlHelpFrame* pFrame = help.GetFrame() )
            {
                pFrame->SetSize(pFrame->FromDIP(wxSize(900, 650)));
                pFrame->Centre();

                // Show the chapters of the book
                wxTreeCtrl* pTree = pFrame->GetHelpWindow() ? pFrame->GetHelpWindow()->GetTreeCtrl() : nullptr;
                wxTreeItemIdValue cookie;
                for ( wxTreeItemId item = pTree ? pTree->GetFirstChild(pTree->GetRootItem(), cookie) : wxTreeItemId(); item.IsOk(); item = pTree->GetNextChild(pTree->GetRootItem(), cookie) ) {
                    pTree->Expand(item);
                }
            }

            wxTheApp->OnRun();
        }
    }

    JonesWx::Shutdown();
    return result;
#else
    (void)pFilePath;
    return 1;
#endif
}
