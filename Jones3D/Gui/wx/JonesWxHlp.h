#ifndef JONES3D_GUI_WX_JONESWXHLP_H
#define JONES3D_GUI_WX_JONESWXHLP_H
#include <wx/string.h>

// WinHelp (.hlp) reader of the help viewer (JonesWxHelp.cpp), added to show the game's Jones3D.hlp without a WinHelp viewer.
// The help file is converted to an HTML help book in wx's memory file system: a page per topic (HTML 3.2 as rendered
// by wxHTML), its pictures as PNG files, the contents from the .cnt file next to it and the keyword index.
// It reads the WinHelp 3.1/4.0 files the game shipped with, the format is documented by Manfred Winterhoff's HELPDECO
// and was cross-checked with Rust HLP Viewer (MIT, https://github.com/NullMagic2/Rust-HLP-Viewer).
namespace JonesWxHlp
{
    // Converts the help file at pFilePath, returns the URL of the book (.hhp) to add to wxHtmlHelpController,
    // or an empty string with pError set
    wxString LoadBook(const wxString& filePath, wxString* pError);
}

#endif // JONES3D_GUI_WX_JONESWXHLP_H
