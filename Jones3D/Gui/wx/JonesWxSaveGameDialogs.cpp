// Save & load game browser, replaces the original common file dialogs with custom templates (154, 159)
#include "JonesWx.h"
#include "JonesWxDialogs.h"

#include <Jones3D/Display/jonesConfig.h>
#include <Jones3D/Main/JonesLevel.h>
#include <Jones3D/Main/JonesMain.h>
#include <Jones3D/Main/jonesString.h>

#include <sith/Dss/sithGamesave.h>
#include <std/Win95/stdDisplay.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Main/sithMain.h>
#include <std/General/std.h>
#include <std/General/stdUtil.h>

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/listctrl.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    constexpr const char* saveGameExt = "nds";

    struct SaveGameFile
    {
        wxString name; // Without extension
        wxDateTime modTime;
    };

    wxString GetSaveGamesDir()
    {
        return JonesWx::NativePath(sithGetSaveGamesDir());
    }

    wxString MakeSaveGamePath(const wxString& name)
    {
        wxString dir = GetSaveGamesDir();
        if ( !dir.empty() && !dir.EndsWith("\\") && !dir.EndsWith("/") ) {
            dir += wxFILE_SEP_PATH;
        }
        return dir + name + "." + saveGameExt;
    }

    std::vector<SaveGameFile> ListSaveGames()
    {
        std::vector<SaveGameFile> aFiles;
        wxDir dir(GetSaveGamesDir());
        if ( !dir.IsOpened() ) {
            return aFiles;
        }

        wxString filename;
        for ( bool bFound = dir.GetFirst(&filename, wxString("*.") + saveGameExt, wxDIR_FILES); bFound; bFound = dir.GetNext(&filename) )
        {
            wxFileName fn(dir.GetNameWithSep() + filename);
            aFiles.push_back({ fn.GetName(), fn.GetModificationTime() });
        }

        // Newest first
        std::sort(aFiles.begin(), aFiles.end(), [](const SaveGameFile& a, const SaveGameFile& b) {
            return a.modTime.IsLaterThan(b.modTime);
        });
        return aFiles;
    }

    // Returns true if name is one of the game's auto or quick save file names
    bool IsReservedSaveName(const std::string& filename)
    {
        for ( size_t levelNum = 0; levelNum < JONESLEVEL_BONUSLEVELNUM; ++levelNum )
        {
            const char* pSaveName = sithGetLevelSaveFilename(levelNum);
            if ( pSaveName )
            {
                char aAutoSaveFilename[128] = { 0 };
                STD_FORMAT(aAutoSaveFilename, "%s%s", sithGetAutoSaveFilePrefix(), pSaveName);
                if ( streqi(filename.c_str(), aAutoSaveFilename) ) {
                    return true;
                }
            }
        }

        char aQuickSaveFilename[128] = { 0 };
        STD_FORMAT(aQuickSaveFilename, "%s.%s", sithGetQuickSaveFilePrefix(), saveGameExt);
        return streqi(filename.c_str(), aQuickSaveFilename);
    }

    class SaveGameBrowser : public JonesWxDialog
    {
    public:
        SaveGameBrowser(HWND hwndParent, bool bSave) :
            JonesWxDialog(hwndParent, bSave ? "JONES_STR_SAVEGM" : "JONES_STR_LOADGM", wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
            bSave(bSave)
        {
            pFileList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(Scaled(360), Scaled(260)), wxLC_REPORT | wxLC_SINGLE_SEL);
            // Note, game has no localized strings for these
            pFileList->AppendColumn("Name", wxLIST_FORMAT_LEFT, Scaled(220));
            pFileList->AppendColumn("Date", wxLIST_FORMAT_LEFT, Scaled(130));
            pFileList->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event) { OnFileSelected(event.GetIndex()); });
            pFileList->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { OnButton(1); });

            const wxSize thumbSize(Scaled(SITHSAVEGAME_THUMB_WIDTH), Scaled(SITHSAVEGAME_THUMB_HEIGHT));
            pThumbnail = new wxStaticBitmap(this, wxID_ANY, wxBitmap(thumbSize), wxDefaultPosition, thumbSize, wxBORDER_SUNKEN);

            wxBoxSizer* pPreviewCol = new wxBoxSizer(wxVERTICAL);
            pPreviewCol->Add(pThumbnail);
            pPreviewCol->AddStretchSpacer();
            pPreviewCol->Add(CreateResourceIcon(this, bSave ? 141 : 139, 32), wxSizerFlags().Center());

            wxBoxSizer* pBrowserRow = new wxBoxSizer(wxHORIZONTAL);
            pBrowserRow->Add(pFileList, wxSizerFlags(1).Expand());
            pBrowserRow->AddSpacer(Scaled(10));
            pBrowserRow->Add(pPreviewCol, wxSizerFlags().Expand());

            pNameEdit = new wxTextCtrl(this, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, bSave ? wxTE_PROCESS_ENTER : wxTE_READONLY);
            pNameEdit->SetMaxLength(60);
            if ( bSave ) {
                pNameEdit->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { OnButton(1); });
            }

            wxBoxSizer* pNameRow = new wxBoxSizer(wxHORIZONTAL);
            if ( bSave ) {
                pNameRow->Add(new wxStaticText(this, wxID_ANY, "File name:"), wxSizerFlags().CenterVertical().Border(wxRIGHT, Scaled(6)));
            }
            pNameRow->Add(pNameEdit, wxSizerFlags(1).CenterVertical());

            wxBoxSizer* pBottomRow = new wxBoxSizer(wxHORIZONTAL);
            pBottomRow->Add(pNameRow, wxSizerFlags(1).CenterVertical());
            pBottomRow->AddSpacer(Scaled(12));
            pBottomRow->Add(CreateButton(this, 1, "JONES_STR_OK", /*bDefault=*/true), wxSizerFlags().CenterVertical());
            pBottomRow->AddSpacer(Scaled(6));
            pBottomRow->Add(CreateButton(this, 2, "JONES_STR_CANCEL"), wxSizerFlags().CenterVertical());

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pBrowserRow, wxSizerFlags(1).Expand());
            pContent->AddSpacer(Scaled(10));
            pContent->Add(pBottomRow, wxSizerFlags().Expand());

            FillFileList();

            if ( bSave )
            {
                // Preview of the game being saved
                SetThumbnail(sithGamesave_CreateThumbnail(), /*bOwned=*/false);
                pNameEdit->SetFocus();
            }

            FinishLayout(pContent);
        }

        // Returns selected save game file path
        std::string GetFilePath() const { return filePath; }

    protected:
        void OnButton(int resultID) override
        {
            if ( resultID == 1 && !(bSave ? ValidateSave() : ValidateLoad()) ) {
                return;
            }

            EndModal(resultID);
        }

    private:
        void FillFileList()
        {
            aFiles = ListSaveGames();

            // In load mode select last saved/loaded game.
            // Note, save mode starts with empty name same as original, so a save isn't overwritten by accident.
            wxString lastName;
            const char* pLastFile = sithGamesave_GetLastFilename();
            if ( !bSave && pLastFile && pLastFile[0] ) {
                lastName = wxFileName(JonesWx::NativePath(pLastFile)).GetName();
            }

            long selIdx = -1;
            for ( size_t i = 0; i < aFiles.size(); ++i )
            {
                pFileList->InsertItem((long)i, aFiles[i].name);
                pFileList->SetItem((long)i, 1, aFiles[i].modTime.IsValid() ? aFiles[i].modTime.Format("%x %H:%M") : wxString());
                if ( selIdx < 0 && aFiles[i].name.IsSameAs(lastName, /*caseSensitive=*/false) ) {
                    selIdx = (long)i;
                }
            }

            if ( !bSave && selIdx < 0 && !aFiles.empty() ) {
                selIdx = 0;
            }

            if ( selIdx >= 0 )
            {
                pFileList->SetItemState(selIdx, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
                pFileList->EnsureVisible(selIdx);
                OnFileSelected(selIdx);
            }
        }

        void OnFileSelected(long idx)
        {
            if ( idx < 0 || (size_t)idx >= aFiles.size() ) {
                return;
            }

            pNameEdit->SetValue(aFiles[idx].name);

            if ( !bSave )
            {
                // Preview of the selected save game
                const std::string path = JonesWx::ToGame(MakeSaveGamePath(aFiles[idx].name));
                SetThumbnail(sithGamesave_LoadThumbnail(path.c_str()), /*bOwned=*/true);
            }
        }

        void SetThumbnail(const tVBuffer* pImage, bool bOwned)
        {
            wxImage image = JonesWx::ImageFromThumbnail(pImage);
            if ( pImage && bOwned ) {
                stdDisplay_VBufferFree(const_cast<tVBuffer*>(pImage));
            }

            const wxSize size = pThumbnail->GetSize();
            if ( image.IsOk() ) {
                image.Rescale(size.x, size.y, wxIMAGE_QUALITY_HIGH);
            }
            else
            {
                image.Create(size.x, size.y);
                image.SetRGB(wxRect(size), 0, 0, 0);
            }

            pThumbnail->SetBitmap(wxBitmap(image));
        }

        void ShowInvalidFileMessage(const char* pErrorKey, const std::string& filename)
        {
            const char* pFormat = jonesString_GetString(pErrorKey);
            if ( pFormat )
            {
                char aErrorText[256] = { 0 };
                STD_FORMAT(aErrorText, pFormat, filename.c_str());
                jonesConfig_ShowMessageDialog((HWND)GetHandle(), bSave ? "JONES_STR_SAVEGM" : "JONES_STR_LOADGM", aErrorText, bSave ? 141 : 139);
            }
        }

        bool ValidateSave()
        {
            wxString name = pNameEdit->GetValue().Strip(wxString::both);
            if ( name.Lower().EndsWith(wxString(".") + saveGameExt) ) {
                name.RemoveLast(strlen(saveGameExt) + 1);
            }

            const std::string filename = JonesWx::ToGame(name + "." + saveGameExt);
            if ( name.empty() || name.find_first_of("/:\\\"?<>*|") != wxString::npos || IsReservedSaveName(filename) )
            {
                ShowInvalidFileMessage("JONES_STR_INVALIDFILE", JonesWx::ToGame(pNameEdit->GetValue()));
                return false;
            }

            const wxString path = MakeSaveGamePath(name);
            if ( wxFileName::FileExists(path) && jonesConfig_ShowOverwriteSaveGameDlg((HWND)GetHandle(), filename.c_str()) != 1 ) {
                return false;
            }

            filePath = JonesWx::ToGame(path);
            return true;
        }

        bool ValidateLoad()
        {
            const wxString name = pNameEdit->GetValue();
            if ( name.empty() ) {
                return false;
            }

            const wxString path = MakeSaveGamePath(name);
            if ( !wxFileName::FileExists(path) )
            {
                ShowInvalidFileMessage("JONES_STR_NOFILE", JonesWx::ToGame(name + "." + saveGameExt));
                return false;
            }

            // Warn about losing current progress, unless player is dead
            if ( JonesMain_HasStarted()
                && (sithPlayer_g_pLocalPlayerThing->flags & (SITH_TF_DYING | SITH_TF_DESTROYED)) == 0
                && jonesConfig_ShowLoadGameWarningMsgBox((HWND)GetHandle()) != 1 )
            {
                return false;
            }

            filePath = JonesWx::ToGame(path);
            return true;
        }

        bool bSave;
        std::vector<SaveGameFile> aFiles;
        std::string filePath;

        wxListCtrl* pFileList;
        wxStaticBitmap* pThumbnail;
        wxTextCtrl* pNameEdit;
    };

    int ShowSaveGameBrowser(HWND hWnd, bool bSave, char* pOutFilePath)
    {
        if ( !JonesWx::EnsureInit() ) {
            return 2;
        }

        SaveGameBrowser dlg(hWnd, bSave);
        if ( dlg.ShowGameModal() != 1 || !pOutFilePath ) {
            return 2;
        }

        stdUtil_StringCopy(pOutFilePath, JONESCONFIG_GAMESAVE_FILEPATHSIZE, dlg.GetFilePath().c_str());
        return 1;
    }
}

extern "C" {

int J3DAPI jonesConfig_GetSaveGameFilePath(HWND hWnd, char* pOutFilePath)
{
    return ShowSaveGameBrowser(hWnd, /*bSave=*/true, pOutFilePath);
}

int J3DAPI jonesConfig_GetLoadGameFilePath(HWND hWnd, char* pDestNdsPath)
{
    return ShowSaveGameBrowser(hWnd, /*bSave=*/false, pDestNdsPath);
}

} // extern "C"
