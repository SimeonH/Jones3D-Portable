#ifndef JONES3D_JONESWX_H
#define JONES3D_JONESWX_H
// wxWidgets implementation of the game dialogs.
// Must be included before any other wx header, as it pulls in Windows.h via Jones3D/types.h.
#include <Jones3D/types.h> // Should be included first due to Windows.h / CommCtrl.h conflict

#include <wx/wx.h>

#include <string>

namespace JonesWx
{
    // Initializes wxWidgets on first call. wx is hosted without its own main loop,
    // the game keeps running its message loop and dialogs run a nested modal loop.
    bool EnsureInit();
    void Shutdown();

    // Returns wx window of hwnd, or nullptr if hwnd is not a wx window (e.g. the game window)
    wxWindow* GetParent(HWND hwnd);

    // Converts game text (system ANSI code page) to wxString and back
    wxString FromGame(const char* pText);
    std::string ToGame(const wxString& str);

    // Returns localized text for JONES_STR_* key, or the key itself when not found
    wxString Str(const char* pKey);

    // Returns localized text for JONES_STR_* key, or pDefault when not found
    wxString StrOr(const char* pKey, const char* pDefault);

    // Formats localized JONES_STR_* format string
    wxString FormatStr(const char* pKey, ...);

    // Loads 24bpp BMP from the game file system (e.g. "misc/exit.bmp").
    // When mask file is given, black pixels of the mask are opaque (original SRCAND/SRCPAINT masking).
    wxImage LoadImage(const char* pFilePath, const char* pMaskFilePath = nullptr);

    // Converts 24/32 bpp DIB section bitmap (e.g. save game thumbnail) to image. Doesn't take ownership of hBitmap.
    wxImage ImageFromDIB(HBITMAP hBitmap);

    // Loads icon resource from the Indy3D.exe resource module
    wxBitmap LoadResourceIcon(int iconID, int size);

    // Returns dialog font scale for the current display mode
    double GetDialogScale();
}

// Base class of all game dialogs.
// Handles pausing the game while dialog is shown, localizing text, font scaling and centering on the game window.
class JonesWxDialog : public wxDialog
{
public:
    JonesWxDialog(HWND hwndParent, const char* pTitleKey, long style = wxDEFAULT_DIALOG_STYLE);

    // Shows dialog modally and returns the original dialog result code (control ID, e.g. 1 = OK, 2 = Cancel)
    int ShowGameModal();

    void EndModal(int retCode) override;

protected:
    // Creates push button which by default ends the dialog with resultID
    wxButton* CreateButton(wxWindow* pParent, int resultID, const char* pTextKey, bool bDefault = false);

    // Creates static text with localized JONES_STR_* key
    wxStaticText* CreateLabel(wxWindow* pParent, const char* pTextKey, long style = 0);

    // Creates static bitmap from icon resource
    wxStaticBitmap* CreateResourceIcon(wxWindow* pParent, int iconID, int size);

    // Creates row of buttons aligned to the right
    wxSizer* CreateButtonRow(std::initializer_list<wxButton*> buttons);

    // Sets dialog sizer, fits dialog to content and centers it
    void FinishLayout(wxSizer* pContent);

    // Called when button with resultID is pressed. Default ends the dialog with resultID.
    virtual void OnButton(int resultID);

    // Result returned when dialog is closed by Escape or the close box, -1 ignores both
    int escapeResult = 2;

    // Converts DPI independent pixels to scaled dialog pixels
    int Scaled(int px) const { return (int)(FromDIP(px) * scale + 0.5); }

private:
    void OnCharHook(wxKeyEvent& event);
    void OnClose(wxCloseEvent& event);

    double scale;
    void EnableOwner();

    HWND hwndOwner      = nullptr; // Native owner window when dialog has no wx parent
    bool bOwnerDisabled = false;
};

#endif // JONES3D_JONESWX_H
