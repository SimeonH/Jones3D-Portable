// Game flow dialogs: game over (150) and insert CD (167)
#include "JonesWx.h"
#include "JonesWxDialogs.h"

#include <Jones3D/Display/jonesConfig.h>
#include <Jones3D/Main/JonesFile.h>

#include <sith/Devices/sithSoundMixer.h>
#include <std/General/stdUtil.h>

namespace
{
    class GameOverDialog : public JonesWxDialog
    {
    public:
        GameOverDialog(HWND hwndParent, GameOverDialogData* pData) :
            JonesWxDialog(hwndParent, "JONES_STR_DEATH"),
            pData(pData)
        {
            escapeResult = -1; // Player has to choose

            char aAutoSavePath[JONESCONFIG_GAMESAVE_FILEPATHSIZE];
            jonesConfig_GetAutoSaveFilePath(aAutoSavePath, sizeof(aAutoSavePath));

            wxButton* pRestartBtn = CreateButton(this, 1177, "JONES_STR_RESTART");
            pRestartBtn->Enable(aAutoSavePath[0] != 0); // Restart from level auto save

            wxBoxSizer* pButtons = new wxBoxSizer(wxVERTICAL);
            pButtons->Add(pRestartBtn, wxSizerFlags().Expand());
            pButtons->AddSpacer(Scaled(6));
            pButtons->Add(CreateButton(this, 1178, "JONES_STR_LOAD", /*bDefault=*/true), wxSizerFlags().Expand());
            pButtons->AddSpacer(Scaled(6));
            pButtons->Add(CreateButton(this, 1179, "JONES_STR_DEAD"), wxSizerFlags().Expand());

            wxBoxSizer* pContent = new wxBoxSizer(wxHORIZONTAL);
            pContent->Add(CreateResourceIcon(this, 207, 32), wxSizerFlags().CenterVertical().Border(wxRIGHT, Scaled(16)));
            pContent->Add(pButtons, wxSizerFlags(1).CenterVertical());
            FinishLayout(pContent);

            if ( pData->hSndMusic ) {
                pData->sndChnlMusic = sithSoundMixer_PlaySound(pData->hSndMusic, 1.0f, 0.0f, (SoundPlayFlag)0);
            }

            stdUtil_StringCopy(pData->pRestoreFilename, JONESCONFIG_GAMESAVE_FILEPATHSIZE, aAutoSavePath);
        }

    protected:
        void OnButton(int resultID) override
        {
            pData->result = resultID;
            if ( resultID == 1178 && jonesConfig_GetLoadGameFilePath((HWND)GetHandle(), pData->pRestoreFilename) != 1 ) {
                return; // Load game canceled
            }

            EndModal(1);
        }

    private:
        GameOverDialogData* pData;
    };

    class InsertCDDialog : public JonesWxDialog
    {
    public:
        InsertCDDialog(HWND hwndParent, int cdNum) :
            JonesWxDialog(hwndParent, "JONES_STR_INSERTCD"),
            cdNum(cdNum)
        {
            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(CreateLabel(this, cdNum == 2 ? "JONES_STR_INSERT2" : "JONES_STR_INSERT1", wxALIGN_CENTRE_HORIZONTAL), wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(12));

            wxBoxSizer* pButtons = new wxBoxSizer(wxHORIZONTAL);
            pButtons->Add(CreateButton(this, 1, "JONES_STR_OK", /*bDefault=*/true));
            pButtons->AddSpacer(Scaled(6));
            pButtons->Add(CreateButton(this, 2, "JONES_STR_QUIT"));
            pContent->Add(pButtons, wxSizerFlags().Center());
            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            // Close only when the requested CD was inserted
            if ( resultID == 1 && JonesFile_GetCurrentCDNum() != cdNum ) {
                return;
            }

            EndModal(resultID);
        }

    private:
        int cdNum;
    };
}

extern "C" {

int J3DAPI jonesConfig_ShowGameOverDialog(HWND hWnd, char* pRestoreFilename, tSoundHandle hSndGameOVerMus, tSoundChannelHandle* pSndChnlMus)
{
    GameOverDialogData data = {};
    data.pRestoreFilename = pRestoreFilename;
    data.hSndMusic        = hSndGameOVerMus;

    if ( JonesWx::EnsureInit() )
    {
        GameOverDialog dlg(hWnd, &data);
        dlg.ShowGameModal();
    }

    if ( pSndChnlMus ) {
        *pSndChnlMus = data.sndChnlMusic;
    }

    return data.result;
}

int J3DAPI jonesConfig_ShowDialogInsertCD(HWND hWnd, LPARAM dwInitParam)
{
    if ( !JonesWx::EnsureInit() ) {
        return 2; // quit
    }

#ifdef __WXMSW__
    ::ShowCursor(TRUE);
#endif

    InsertCDDialog dlg(hWnd, (int)dwInitParam);
    int btnNum = dlg.ShowGameModal();

#ifdef __WXMSW__
    ::ShowCursor(FALSE);
#endif

    return btnNum; // 1 - ok or 2 - quit
}

} // extern "C"
