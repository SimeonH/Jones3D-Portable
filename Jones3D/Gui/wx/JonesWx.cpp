#include "JonesWx.h"

#include <Jones3D/Gui/JonesDialog.h>
#include <Jones3D/Gui/wx/JonesWxDialogs.h>
#include <Jones3D/Main/JonesMain.h>
#include <Jones3D/Main/jonesString.h>

#include <std/General/std.h>
#include <std/General/stdPlatform.h>
#include <std/Win95/stdDisplay.h>
#include <wkernel/wkernel.h>

#include <wx/display.h>
#include <wx/mstream.h>

#ifdef __WXMSW__
#include <wx/msw/private.h> // wxSetInstance, wxFindWinFromHandle
#endif

#include <stdarg.h>
#include <stdio.h>
#include <vector>

namespace
{
    class JonesWxApp : public wxApp
    {
    public:
        bool OnInit() override { return true; }
    };

    bool bInited = false;

#ifdef __WXMSW__
    HINSTANCE GetThisModule()
    {
        // Could be either OpenJones3D.exe or Jones3D.dll
        HMODULE hModule = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&GetThisModule, &hModule);
        return hModule;
    }
#endif

    bool ReadGameFile(const char* pFilePath, std::vector<uint8_t>& data)
    {
        tFileHandle fh = std_g_pHS->pFileOpen(pFilePath, "rb");
        if ( !fh )
        {
            STDLOG_ERROR("JonesWx: Couldn't open file '%s'.\n", pFilePath);
            return false;
        }

        data.clear();
        uint8_t aBuf[4096];
        size_t nRead;
        while ( (nRead = std_g_pHS->pFileRead(fh, aBuf, sizeof(aBuf))) > 0 )
        {
            data.insert(data.end(), aBuf, aBuf + nRead);
        }

        std_g_pHS->pFileClose(fh);
        return !data.empty();
    }
}

wxIMPLEMENT_APP_NO_MAIN(JonesWxApp);

namespace
{
    // Logs wx assertions instead of showing a modal alert over the game
    void LogAssert(const wxString& file, int line, const wxString& func, const wxString& cond, const wxString& msg)
    {
        if ( !std_g_pHS )
        {
            // Before the game started, e.g. in the data installer
            fprintf(stderr, "wxWidgets assert: %s(%d) %s: %s %s\n", (const char*)file.mb_str(), line, (const char*)func.mb_str(), (const char*)cond.mb_str(), (const char*)msg.mb_str());
            return;
        }

        STDLOG_ERROR("wxWidgets assert: %s(%d) %s: %s %s\n", (const char*)file.mb_str(), line, (const char*)func.mb_str(), (const char*)cond.mb_str(), (const char*)msg.mb_str());
    }
}

bool JonesWx::EnsureInit()
{
    if ( bInited ) {
        return true;
    }

#ifdef __WXMSW__
    wxSetInstance(GetThisModule());
#endif

    wxSetAssertHandler(LogAssert);

    int argc = 0;
    if ( !wxEntryStart(argc, (wxChar**)nullptr) )
    {
        STDLOG_ERROR("JonesWx: Failed to initialize wxWidgets.\n");
        return false;
    }

    if ( !wxTheApp || !wxTheApp->CallOnInit() )
    {
        STDLOG_ERROR("JonesWx: Failed to initialize wxWidgets application.\n");
        wxEntryCleanup();
        return false;
    }

    wxInitAllImageHandlers();
    bInited = true;
    return true;
}

void JonesWx::Shutdown()
{
    if ( !bInited ) {
        return;
    }

    wxEntryCleanup();
    bInited = false;
}

wxWindow* JonesWx::GetParent(HWND hwnd)
{
#ifdef __WXMSW__
    // Nested dialog. Note, native windows (e.g. the game window) are not wrapped
    // as wx would subclass them, they're set as native owner of the dialog instead.
    if ( hwnd ) {
        return wxFindWinFromHandle((WXHWND)hwnd);
    }
#else
    // HWND is the SDL window of the game, or a wx dialog passed as HWND (e.g. the launcher, JonesWxLauncher.cpp)
    for ( wxWindow* pWindow : wxTopLevelWindows )
    {
        if ( hwnd && reinterpret_cast<HWND>(pWindow) == hwnd ) {
            return pWindow;
        }
    }
#endif
    return nullptr;
}

wxString JonesWx::NativePath(const char* pGamePath)
{
#ifdef _WIN32
    return FromGame(pGamePath);
#else
    // Game paths use backslashes and are case-insensitive
    char aPath[1024];
    return FromGame(stdPlatform_NativePath(pGamePath, aPath, sizeof(aPath)));
#endif
}

wxString JonesWx::FromGame(const char* pText)
{
    if ( !pText ) {
        return wxString();
    }

#ifdef __WXMSW__
    // Game text is in the system ANSI code page, same as the original ANSI Win32 dialogs used
    return wxString(pText, wxConvLocal);
#else
    return wxString(pText, wxCSConv(wxFONTENCODING_CP1252));
#endif
}

std::string JonesWx::ToGame(const wxString& str)
{
#ifdef __WXMSW__
    wxCharBuffer buf = str.mb_str(wxConvLocal);
#else
    wxCharBuffer buf = str.mb_str(wxCSConv(wxFONTENCODING_CP1252));
#endif
    return buf.data() ? std::string(buf.data()) : std::string();
}

wxString JonesWx::Str(const char* pKey)
{
    const char* pText = jonesString_GetString(pKey);
    return FromGame(pText ? pText : pKey);
}

wxString JonesWx::StrOr(const char* pKey, const char* pDefault)
{
    const char* pText = jonesString_GetString(pKey);
    return FromGame(pText ? pText : pDefault);
}

wxString JonesWx::FormatStr(const char* pKey, ...)
{
    const char* pFormat = jonesString_GetString(pKey);
    if ( !pFormat ) {
        return FromGame(pKey);
    }

    char aText[1024] = { 0 };
    va_list args;
    va_start(args, pKey);
    vsnprintf(aText, sizeof(aText) - 1, pFormat, args);
    va_end(args);
    return FromGame(aText);
}

wxImage JonesWx::LoadImage(const char* pFilePath, const char* pMaskFilePath)
{
    std::vector<uint8_t> data;
    if ( !ReadGameFile(pFilePath, data) ) {
        return wxImage();
    }

    wxMemoryInputStream stream(data.data(), data.size());
    wxImage image(stream, wxBITMAP_TYPE_BMP);
    if ( !image.IsOk() || !pMaskFilePath ) {
        return image;
    }

    if ( !ReadGameFile(pMaskFilePath, data) ) {
        return image;
    }

    wxMemoryInputStream maskStream(data.data(), data.size());
    wxImage mask(maskStream, wxBITMAP_TYPE_BMP);
    if ( !mask.IsOk() || mask.GetSize() != image.GetSize() ) {
        return image;
    }

    // Original draws mask with SRCAND then image with SRCPAINT, i.e.: black mask pixel is opaque
    image.InitAlpha();
    const unsigned char* pMask = mask.GetData();
    unsigned char* pAlpha      = image.GetAlpha();
    const size_t numPixels     = (size_t)image.GetWidth() * image.GetHeight();
    for ( size_t i = 0; i < numPixels; ++i )
    {
        pAlpha[i] = (unsigned char)(255 - (pMask[i * 3] + pMask[i * 3 + 1] + pMask[i * 3 + 2]) / 3);
    }

    return image;
}

wxImage JonesWx::ImageFromThumbnail(const tVBuffer* pThumbnail)
{
    if ( !pThumbnail || !pThumbnail->pPixels || pThumbnail->rasterInfo.colorInfo.bpp != 24 ) {
        return wxImage();
    }

    const int width  = (int)pThumbnail->rasterInfo.width;
    const int height = (int)pThumbnail->rasterInfo.height;

    wxImage image(width, height, /*clear=*/false);
    unsigned char* pDst = image.GetData();
    for ( int y = 0; y < height; ++y )
    {
        const unsigned char* pRow = pThumbnail->pPixels + (size_t)y * pThumbnail->rasterInfo.rowSize;
        for ( int x = 0; x < width; ++x )
        {
            const unsigned char* pPixel = pRow + x * 3;
            *pDst++ = pPixel[2];
            *pDst++ = pPixel[1];
            *pDst++ = pPixel[0];
        }
    }

    return image;
}

wxBitmap JonesWx::LoadResourceIcon(int iconID, int size)
{
#ifdef __WXMSW__
    HICON hIcon = (HICON)::LoadImageA(wkernel_GetResourceInstance(), MAKEINTRESOURCEA(iconID), IMAGE_ICON, size, size, LR_DEFAULTCOLOR);
    if ( !hIcon ) {
        return wxBitmap();
    }

    wxIcon icon;
    icon.CreateFromHICON(hIcon); // Takes ownership of hIcon
    return wxBitmap(icon);
#else
    // Read from the original Indy3D.exe
    wxImage image = LoadExeIconImage(iconID, size);
    return image.IsOk() ? wxBitmap(image) : wxBitmap();
#endif
}

double JonesWx::GetDialogScale()
{
#ifndef J3D_RENDERER_DIRECTDRAW
    // The game image is scaled to the window (the display mode never changes), scale the dialogs
    // the same so they keep the size relative to the game they had in the original game.
    if ( !JonesMain_HasStarted() ) {
        return 1.0;
    }

    const double presentScale = stdDisplay_GetPresentScale();
    if ( presentScale <= 0.0 ) {
        return 1.0;
    }

    // Present scale is in physical pixels, dialogs are already scaled by the display scale factor
    const double displayScale = wxDisplay((unsigned int)0).GetScaleFactor();
    return presentScale / (displayScale > 0.0 ? displayScale : 1.0);
#else
    // Shrink dialogs when the game runs fullscreen in a display mode smaller than the desktop
    JonesDisplaySettings* pSettings = JonesMain_GetDisplaySettings();
    if ( !JonesMain_HasStarted() || !pSettings || pSettings->bWindowMode ) {
        return 1.0;
    }

    uint32_t width, height;
    stdDisplay_GetBackBufferSize(&width, &height);

    wxRect rectDisplay = wxDisplay((unsigned int)0).GetGeometry();
    if ( !height || rectDisplay.height <= 0 || (int)height >= rectDisplay.height ) {
        return 1.0;
    }

    return (double)height / rectDisplay.height;
#endif
}

JonesWxDialog::JonesWxDialog(HWND hwndParent, const char* pTitleKey, long style) :
    wxDialog(JonesWx::GetParent(hwndParent), wxID_ANY, pTitleKey ? JonesWx::Str(pTitleKey) : wxString(), wxDefaultPosition, wxDefaultSize, style)
{
#ifdef __WXMSW__
    if ( !GetParent() && hwndParent )
    {
        // Native owner window, e.g. the game window. Same as owner of the original Win32 dialog.
        hwndOwner = hwndParent;
        ::SetWindowLongPtr(GetHWND(), GWLP_HWNDPARENT, (LONG_PTR)hwndOwner);

        // A Win32 dialog created with a topmost owner (the fullscreen game window) is topmost too.
        // Setting the owner afterwards doesn't do that, so the dialog would open behind the game window.
        if ( ::GetWindowLongPtr(hwndOwner, GWL_EXSTYLE) & WS_EX_TOPMOST ) {
            ::SetWindowPos(GetHWND(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
#endif

    scale = JonesWx::GetDialogScale();
    if ( scale != 1.0 )
    {
        // Children created after this inherit the font
        SetFont(GetFont().Scaled((float)scale));
    }

    SetEscapeId(wxID_NONE);
    Bind(wxEVT_CHAR_HOOK, &JonesWxDialog::OnCharHook, this);
    Bind(wxEVT_CLOSE_WINDOW, &JonesWxDialog::OnClose, this);
}

int JonesWxDialog::ShowGameModal()
{
    JonesDialogGameState state;
    const bool bGameState = JonesMain_HasStarted() != 0;
    if ( bGameState )
    {
        if ( stdDisplay_CanRenderWindowed() == -1 ) {
            return -1;
        }

        // Pauses the game, releases input and flips to GDI surface so native windows are visible
        JonesDialog_SetGameState(&state, /*bWindowModeSupported=*/1);
    }

#ifdef __WXMSW__
    // Disable the native owner window while the dialog is shown, same as Win32 modal dialog does
    bOwnerDisabled = hwndOwner && ::IsWindowEnabled(hwndOwner) && !::EnableWindow(hwndOwner, FALSE);
#endif

    int result = ShowModal();

#ifdef __WXMSW__
    EnableOwner(); // In case dialog was closed without EndModal
#endif

    if ( bGameState ) {
        JonesDialog_ResetGameState(&state, /*bWindowModeSupported=*/1);
    }

    return result;
}

wxButton* JonesWxDialog::CreateButton(wxWindow* pParent, int resultID, const char* pTextKey, bool bDefault)
{
    wxButton* pButton = new wxButton(pParent, wxID_ANY, JonesWx::Str(pTextKey));
    pButton->Bind(wxEVT_BUTTON, [this, resultID](wxCommandEvent&) { OnButton(resultID); });
    if ( bDefault )
    {
        pButton->SetDefault();
        pButton->SetFocus();
    }
    return pButton;
}

wxStaticText* JonesWxDialog::CreateLabel(wxWindow* pParent, const char* pTextKey, long style)
{
    return new wxStaticText(pParent, wxID_ANY, JonesWx::Str(pTextKey), wxDefaultPosition, wxDefaultSize, style);
}

wxStaticBitmap* JonesWxDialog::CreateResourceIcon(wxWindow* pParent, int iconID, int size)
{
    return new wxStaticBitmap(pParent, wxID_ANY, JonesWx::LoadResourceIcon(iconID, Scaled(size)));
}

wxSizer* JonesWxDialog::CreateButtonRow(std::initializer_list<wxButton*> buttons)
{
    wxBoxSizer* pRow = new wxBoxSizer(wxHORIZONTAL);
    pRow->AddStretchSpacer();
    for ( wxButton* pButton : buttons ) {
        pRow->Add(pButton, wxSizerFlags().Border(wxLEFT));
    }
    return pRow;
}

void JonesWxDialog::FinishLayout(wxSizer* pContent)
{
    wxBoxSizer* pTop = new wxBoxSizer(wxVERTICAL);
    pTop->Add(pContent, wxSizerFlags(1).Expand().DoubleBorder());
    SetSizerAndFit(pTop);

#ifdef __WXMSW__
    RECT rectOwner;
    if ( hwndOwner && ::GetWindowRect(hwndOwner, &rectOwner) )
    {
        // Center on native owner window
        const wxRect rect(wxPoint(rectOwner.left, rectOwner.top), wxPoint(rectOwner.right - 1, rectOwner.bottom - 1));
        const wxSize size = GetSize();
        Move(rect.x + (rect.width - size.x) / 2, rect.y + (rect.height - size.y) / 2);
        return;
    }
#endif

    Centre();
}

void JonesWxDialog::EndModal(int retCode)
{
    // Same as Win32 EndDialog: enable the owner and, if the dialog is active, activate the owner
    // before the dialog is hidden. Otherwise Windows activates some other window instead of the game.
    EnableOwner();
#ifdef __WXMSW__
    if ( hwndOwner && ::GetForegroundWindow() == GetHWND() )
    {
        // Still the foreground window, so the game window is allowed to become the foreground window
        ::SetForegroundWindow(hwndOwner);
        ::SetActiveWindow(hwndOwner);
    }
#endif
    wxDialog::EndModal(retCode);
}

void JonesWxDialog::EnableOwner()
{
#ifdef __WXMSW__
    if ( bOwnerDisabled )
    {
        ::EnableWindow(hwndOwner, TRUE);
        bOwnerDisabled = false;
    }
#endif
}

void JonesWxDialog::OnButton(int resultID)
{
    EndModal(resultID);
}

void JonesWxDialog::OnCharHook(wxKeyEvent& event)
{
    if ( event.GetKeyCode() == WXK_ESCAPE )
    {
        if ( escapeResult >= 0 ) {
            OnButton(escapeResult); // Same as Win32 dialog IDCANCEL command
        }
        return;
    }

    event.Skip();
}

void JonesWxDialog::OnClose(wxCloseEvent& event)
{
    if ( escapeResult >= 0 ) {
        OnButton(escapeResult);
    }
    else {
        event.Veto();
    }
}

extern "C" void JonesWx_Shutdown(void)
{
    JonesWx::Shutdown();
}

extern "C" void JonesWx_ShowMessageBox(const char* pTitle, const char* pText, int bError)
{
    if ( !JonesWx::EnsureInit() ) {
        return;
    }

    wxMessageBox(JonesWx::FromGame(pText), JonesWx::FromGame(pTitle), wxOK | wxCENTRE | (bError ? wxICON_ERROR : wxICON_INFORMATION));
}

extern "C" void JonesWx_ShowFatalError(const char* pTitle, const char* pText)
{
    // Doesn't require running event loop or initialized wx, safe to call from assert & fatal error handlers
    wxSafeShowMessage(JonesWx::FromGame(pTitle), JonesWx::FromGame(pText));
}
