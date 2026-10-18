// Simple message dialogs: generic message (121), exit game (211), overwrite save game (214) and load game warning (163)
#include "JonesWx.h"

#include <Jones3D/Display/jonesConfig.h>

namespace
{
    struct ButtonInfo
    {
        int resultID;
        const char* pTextKey;
        bool bDefault;
    };

    // Icon, text and row of buttons
    class MessageDialog : public JonesWxDialog
    {
    public:
        MessageDialog(HWND hwndParent, const char* pTitleKey, const wxString& text, int iconID, int iconSize, std::initializer_list<ButtonInfo> buttons) :
            JonesWxDialog(hwndParent, pTitleKey)
        {
            wxStaticText* pText = new wxStaticText(this, wxID_ANY, text);
            pText->Wrap(Scaled(320));

            wxBoxSizer* pMsgRow = new wxBoxSizer(wxHORIZONTAL);
            pMsgRow->Add(CreateResourceIcon(this, iconID, iconSize), wxSizerFlags().Top().Border(wxRIGHT, Scaled(12)));
            pMsgRow->Add(pText, wxSizerFlags(1).CenterVertical());

            wxBoxSizer* pButtonRow = new wxBoxSizer(wxHORIZONTAL);
            pButtonRow->AddStretchSpacer();
            for ( const ButtonInfo& btn : buttons ) {
                pButtonRow->Add(CreateButton(this, btn.resultID, btn.pTextKey, btn.bDefault), wxSizerFlags().Border(wxLEFT));
            }

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pMsgRow, wxSizerFlags(1).Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(pButtonRow, wxSizerFlags().Expand());
            FinishLayout(pContent);
        }
    };

    // Yes/No message box used by save & load game dialogs
    int ShowYesNoMessageBox(HWND hWnd, const char* pTitleKey, const wxString& text, int iconID)
    {
        MessageDialog dlg(hWnd, pTitleKey, text, iconID, 32, { { 1, "JONES_STR_YES", true }, { 2, "JONES_STR_NO", false } });
        return dlg.ShowGameModal();
    }

    class ExitGameDialog : public JonesWxDialog
    {
    public:
        ExitGameDialog(HWND hwndParent, char* pSaveGameFilePath) :
            JonesWxDialog(hwndParent, "JONES_STR_EXIT"),
            pSaveGameFilePath(pSaveGameFilePath)
        {
            wxImage image = JonesWx::LoadImage("misc\\exit.bmp", "misc\\exitmask.bmp");
            if ( image.IsOk() ) {
                image.Rescale(Scaled(96), Scaled(96), wxIMAGE_QUALITY_HIGH);
            }

            wxBoxSizer* pButtons = new wxBoxSizer(wxVERTICAL);
            pButtons->Add(CreateButton(this, 2, "JONES_STR_EXIT3", /*bDefault=*/true), wxSizerFlags().Expand()); // Resume game
            pButtons->AddSpacer(Scaled(6));
            pButtons->Add(CreateButton(this, 1187, "JONES_STR_EXIT2"), wxSizerFlags().Expand());                 // Save game
            pButtons->AddSpacer(Scaled(6));
            pButtons->Add(CreateButton(this, 1, "JONES_STR_EXIT1"), wxSizerFlags().Expand());                    // Exit game

            wxBoxSizer* pContent = new wxBoxSizer(wxHORIZONTAL);
            pContent->Add(new wxStaticBitmap(this, wxID_ANY, wxBitmap(image)), wxSizerFlags().CenterVertical().Border(wxRIGHT, Scaled(16)));
            pContent->Add(pButtons, wxSizerFlags(1).CenterVertical());
            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            if ( resultID == 1187 && jonesConfig_GetSaveGameFilePath((HWND)GetHandle(), pSaveGameFilePath) != 1 )
            {
                // Save game was canceled, resume game
                EndModal(2);
                return;
            }

            EndModal(resultID);
        }

    private:
        char* pSaveGameFilePath;
    };
}

extern "C" {

int J3DAPI jonesConfig_ShowMessageDialog(HWND hWnd, const char* pTitle, const char* pText, int iconID)
{
    if ( !JonesWx::EnsureInit() ) {
        return -1;
    }

    MessageDialog dlg(hWnd, pTitle, JonesWx::FromGame(pText), iconID, 64, { { 1, "JONES_STR_OK", true } });
    return dlg.ShowGameModal();
}

int J3DAPI jonesConfig_ShowExitGameDialog(HWND hWnd, char* pSaveGameFilePath)
{
    if ( !JonesWx::EnsureInit() ) {
        return -1;
    }

    ExitGameDialog dlg(hWnd, pSaveGameFilePath);
    return dlg.ShowGameModal();
}

int J3DAPI jonesConfig_ShowOverwriteSaveGameDlg(HWND hWnd, const char* aFilePath)
{
    if ( !JonesWx::EnsureInit() ) {
        return -1;
    }

    // Show only the file name
    const char* pFilename = aFilePath;
    for ( const char* pCur = aFilePath; *pCur; ++pCur )
    {
        if ( *pCur == '\\' || *pCur == '/' ) {
            pFilename = pCur + 1;
        }
    }

    return ShowYesNoMessageBox(hWnd, "JONES_STR_SAVEGM", JonesWx::FormatStr("JONES_STR_OVERWRITE", pFilename), 141);
}

int J3DAPI jonesConfig_ShowLoadGameWarningMsgBox(HWND hWnd)
{
    if ( !JonesWx::EnsureInit() ) {
        return -1;
    }

    return ShowYesNoMessageBox(hWnd, "JONES_STR_LOADGM", JonesWx::Str("JONES_STR_PROGRESS1"), 139);
}

} // extern "C"
