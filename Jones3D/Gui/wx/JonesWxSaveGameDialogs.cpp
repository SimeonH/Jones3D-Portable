// Save & load game dialogs.
// Replicates the original Explorer-style common file dialog (GetOpenFileName / GetSaveFileName)
// with the game's custom template (154, 159) which adds the save game thumbnail and icon on the right.
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

#include <wx/artprov.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/gbsizer.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/numformatter.h>
#include <wx/odcombo.h>
#include <wx/stdpaths.h>
#include <wx/toolbar.h>

#ifdef __WXMSW__
#include <shellapi.h>
#include <shlobj.h>
#endif

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace
{
    constexpr const char* saveGameExt = "nds";

    // Note, the original dialog text comes from Windows, the game has no localized strings for these
    constexpr const char* aInvalidNameChars = "/\\:*?\"<>|";

    enum FileColumn
    {
        FILE_COLUMN_NAME = 0,
        FILE_COLUMN_SIZE = 1,
        FILE_COLUMN_TYPE = 2,
        FILE_COLUMN_DATE = 3,
    };

    enum FileViewMode
    {
        FILE_VIEW_LARGEICONS = 0,
        FILE_VIEW_SMALLICONS = 1,
        FILE_VIEW_LIST       = 2,
        FILE_VIEW_DETAILS    = 3,
    };

    constexpr int TOOL_BACK       = wxID_HIGHEST + 1;
    constexpr int TOOL_UP         = wxID_HIGHEST + 2;
    constexpr int TOOL_NEWFOLDER  = wxID_HIGHEST + 3;
    constexpr int TOOL_VIEW       = wxID_HIGHEST + 4;
    constexpr int MENU_VIEW_FIRST = wxID_HIGHEST + 5; // + FileViewMode

    // View and sort order are kept between dialogs for the session. Default sort is newest first.
    int fileViewMode       = FILE_VIEW_LIST;
    int fileSortColumn     = FILE_COLUMN_DATE;
    bool bFileSortAscending = false;

    bool IsSamePath(const wxString& a, const wxString& b)
    {
    #ifdef __WXMSW__
        return a.IsSameAs(b, /*caseSensitive=*/false);
    #else
        return a == b;
    #endif
    }

    // Returns absolute directory path with trailing separator
    wxString NormalizeDir(const wxString& path)
    {
        wxFileName fn = wxFileName::DirName(path);
        fn.MakeAbsolute();
        return fn.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
    }

    wxString GetSaveGamesDir()
    {
        return NormalizeDir(JonesWx::NativePath(sithGetSaveGamesDir()));
    }

    wxBitmap ScaleBitmap(const wxBitmap& bmp, int size)
    {
        if ( !bmp.IsOk() || (bmp.GetWidth() == size && bmp.GetHeight() == size) ) {
            return bmp;
        }

        wxImage image = bmp.ConvertToImage();
        image.Rescale(size, size, wxIMAGE_QUALITY_HIGH);
        return wxBitmap(image);
    }

    // Shell icons, same as the original dialog shows
    enum class IconKind { Folder, SaveFile, Drive, Desktop, Computer, Home };

#ifdef __WXMSW__
    wxBitmap ShellIconFromInfo(SHFILEINFOW& sfi, int size)
    {
        if ( !sfi.hIcon ) {
            return wxBitmap();
        }

        wxIcon icon;
        if ( !icon.CreateFromHICON(sfi.hIcon) ) // Takes ownership
        {
            ::DestroyIcon(sfi.hIcon);
            return wxBitmap();
        }

        wxBitmap bmp;
        bmp.CopyFromIcon(icon);
        return ScaleBitmap(bmp, size);
    }

    wxBitmap ShellIcon(const wxString& path, DWORD fileAttributes, bool bUseAttributes, int size)
    {
        SHFILEINFOW sfi = {};
        const UINT flags = SHGFI_ICON | (size > 16 ? SHGFI_LARGEICON : SHGFI_SMALLICON) | (bUseAttributes ? SHGFI_USEFILEATTRIBUTES : 0);
        if ( !SHGetFileInfoW(path.wc_str(), fileAttributes, &sfi, sizeof(sfi), flags) ) {
            return wxBitmap();
        }
        return ShellIconFromInfo(sfi, size);
    }

    // Returns icon and display name of special shell folder, e.g. CSIDL_DESKTOP
    wxBitmap ShellSpecialFolderIcon(int csidl, int size, wxString* pDisplayName)
    {
        PIDLIST_ABSOLUTE pidl = nullptr;
        if ( FAILED(SHGetSpecialFolderLocation(nullptr, csidl, &pidl)) || !pidl ) {
            return wxBitmap();
        }

        SHFILEINFOW sfi = {};
        const UINT flags = SHGFI_PIDL | SHGFI_ICON | SHGFI_DISPLAYNAME | (size > 16 ? SHGFI_LARGEICON : SHGFI_SMALLICON);
        wxBitmap bmp;
        if ( SHGetFileInfoW((LPCWSTR)pidl, 0, &sfi, sizeof(sfi), flags) )
        {
            if ( pDisplayName ) {
                *pDisplayName = sfi.szDisplayName;
            }
            bmp = ShellIconFromInfo(sfi, size);
        }

        CoTaskMemFree(pidl);
        return bmp;
    }

    wxString ShellDisplayName(const wxString& path)
    {
        SHFILEINFOW sfi = {};
        if ( SHGetFileInfoW(path.wc_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME) && sfi.szDisplayName[0] ) {
            return sfi.szDisplayName;
        }
        return path;
    }

    std::vector<wxString> GetDrives()
    {
        std::vector<wxString> aDrives;
        wchar_t aBuf[512] = { 0 };
        const DWORD len = GetLogicalDriveStringsW((DWORD)STD_ARRAYLEN(aBuf), aBuf);
        if ( len == 0 || len >= STD_ARRAYLEN(aBuf) ) {
            return aDrives;
        }

        for ( const wchar_t* pDrive = aBuf; *pDrive; pDrive += wcslen(pDrive) + 1 ) {
            aDrives.push_back(pDrive);
        }
        return aDrives;
    }
#endif

    wxBitmap ArtIcon(const wxArtID& id, int size, const wxArtID& fallbackID = wxART_FOLDER)
    {
        wxBitmap bmp = wxArtProvider::GetBitmap(id, wxART_OTHER, wxSize(size, size));
        if ( !bmp.IsOk() && id != fallbackID ) {
            bmp = wxArtProvider::GetBitmap(fallbackID, wxART_OTHER, wxSize(size, size));
        }
        return ScaleBitmap(bmp, size);
    }

    wxBitmap GetShellIcon(IconKind kind, int size, const wxString& path = wxString())
    {
    #ifdef __WXMSW__
        wxBitmap bmp;
        switch ( kind )
        {
            case IconKind::Folder:   bmp = ShellIcon("folder", FILE_ATTRIBUTE_DIRECTORY, true, size); break;
            case IconKind::SaveFile: bmp = ShellIcon(wxString("file.") + saveGameExt, FILE_ATTRIBUTE_NORMAL, true, size); break;
            case IconKind::Drive:    bmp = ShellIcon(path, 0, false, size); break;
            case IconKind::Desktop:  bmp = ShellSpecialFolderIcon(CSIDL_DESKTOP, size, nullptr); break;
            case IconKind::Computer: bmp = ShellSpecialFolderIcon(CSIDL_DRIVES, size, nullptr); break;
            case IconKind::Home:     bmp = ShellIcon(path, 0, false, size); break;
        }
        if ( bmp.IsOk() ) {
            return bmp;
        }
    #else
        (void)path;
    #endif

        switch ( kind )
        {
            case IconKind::SaveFile: return ArtIcon(wxART_NORMAL_FILE, size);
            case IconKind::Drive:    return ArtIcon(wxART_HARDDISK, size);
            case IconKind::Desktop:  return ArtIcon("user-desktop", size);
            case IconKind::Computer: return ArtIcon("computer", size, wxART_HARDDISK);
            case IconKind::Home:     return ArtIcon("user-home", size);
            default:                 return ArtIcon(wxART_FOLDER, size);
        }
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

    bool HasSaveGameExt(const wxString& name)
    {
        return name.Lower().EndsWith(wxString(".") + saveGameExt);
    }

    // "Look in" folder combo box, shows the folder hierarchy indented with icons as the original
    class LookInComboBox : public wxOwnerDrawnComboBox
    {
    public:
        struct Item
        {
            wxString path; // Empty = computer (drive list)
            wxString label;
            int indent;
            wxBitmap icon;
        };

        LookInComboBox(wxWindow* pParent, int indentStep, int iconSize) :
            wxOwnerDrawnComboBox(pParent, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY),
            indentStep(indentStep),
            iconSize(iconSize)
        {}

        void SetFolders(std::vector<Item>&& aNewItems, int selection)
        {
            aItems = std::move(aNewItems);
            Clear();
            for ( const Item& item : aItems ) {
                Append(item.label);
            }
            SetSelection(selection);
        }

        const Item* GetFolder(int n) const
        {
            return n >= 0 && (size_t)n < aItems.size() ? &aItems[n] : nullptr;
        }

    protected:
        void OnDrawItem(wxDC& dc, const wxRect& rect, int n, int flags) const override
        {
            const Item* pItem = GetFolder(n);
            if ( !pItem ) {
                return;
            }

            int x = rect.x + 2;
            if ( (flags & wxODCB_PAINTING_CONTROL) == 0 ) {
                x += pItem->indent * indentStep;
            }

            if ( pItem->icon.IsOk() )
            {
                dc.DrawBitmap(pItem->icon, x, rect.y + (rect.height - pItem->icon.GetHeight()) / 2, /*useMask=*/true);
                x += pItem->icon.GetWidth() + 4;
            }

            wxColour textColor;
            if ( !IsEnabled() ) {
                textColor = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
            }
            else if ( flags & wxODCB_PAINTING_SELECTED ) {
                textColor = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
            }
            else {
                textColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
            }

            dc.SetTextForeground(textColor);
            dc.DrawText(pItem->label, x, rect.y + (rect.height - dc.GetCharHeight()) / 2);
        }

        wxCoord OnMeasureItem(size_t) const override
        {
            return (std::max)(iconSize, GetCharHeight()) + 4;
        }

        wxCoord OnMeasureItemWidth(size_t) const override
        {
            return -1; // Default
        }

    private:
        std::vector<Item> aItems;
        int indentStep;
        int iconSize;
    };

    class SaveGameFileDialog : public JonesWxDialog
    {
    public:
        SaveGameFileDialog(HWND hwndParent, bool bSave) :
            JonesWxDialog(hwndParent, bSave ? "JONES_STR_SAVEGM" : "JONES_STR_LOADGM"),
            bSave(bSave)
        {
            smallIconSize = Scaled(16);
            largeIconSize = Scaled(32);

            // Folder row
            wxStaticText* pLookInLabel = new wxStaticText(this, wxID_ANY, bSave ? "Save &in:" : "Look &in:");
            pLookIn = new LookInComboBox(this, Scaled(10), smallIconSize);
            pLookIn->SetMinSize(wxSize(Scaled(210), -1));
            pLookIn->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& event) {
                if ( const LookInComboBox::Item* pItem = pLookIn->GetFolder(event.GetSelection()) ) {
                    CallAfter([this, path = pItem->path]() { Navigate(path); });
                }
            });

            pToolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER);
            const wxSize toolSize(smallIconSize, smallIconSize);
            pToolBar->SetToolBitmapSize(toolSize);
            pToolBar->AddTool(TOOL_BACK, wxString(), wxArtProvider::GetBitmapBundle(wxART_GO_BACK, wxART_TOOLBAR, toolSize), "Back");
            pToolBar->AddTool(TOOL_UP, wxString(), wxArtProvider::GetBitmapBundle(wxART_GO_DIR_UP, wxART_TOOLBAR, toolSize), "Up One Level");
            pToolBar->AddTool(TOOL_NEWFOLDER, wxString(), wxArtProvider::GetBitmapBundle(wxART_NEW_DIR, wxART_TOOLBAR, toolSize), "Create New Folder");

            pViewMenu = new wxMenu();
            pViewMenu->AppendRadioItem(MENU_VIEW_FIRST + FILE_VIEW_LARGEICONS, "Lar&ge Icons");
            pViewMenu->AppendRadioItem(MENU_VIEW_FIRST + FILE_VIEW_SMALLICONS, "S&mall Icons");
            pViewMenu->AppendRadioItem(MENU_VIEW_FIRST + FILE_VIEW_LIST, "&List");
            pViewMenu->AppendRadioItem(MENU_VIEW_FIRST + FILE_VIEW_DETAILS, "&Details");
            pViewMenu->Check(MENU_VIEW_FIRST + fileViewMode, true);

            const wxBitmap viewBitmap = wxArtProvider::GetBitmap(wxART_REPORT_VIEW, wxART_TOOLBAR, toolSize);
            pToolBar->AddTool(TOOL_VIEW, wxString(), viewBitmap, "View Menu", wxITEM_DROPDOWN);
            pToolBar->SetDropdownMenu(TOOL_VIEW, pViewMenu); // Takes ownership
            pToolBar->Realize();

            pToolBar->Bind(wxEVT_TOOL, &SaveGameFileDialog::OnTool, this);
            Bind(wxEVT_MENU, [this](wxCommandEvent& event) { SetViewMode(event.GetId() - MENU_VIEW_FIRST); }, MENU_VIEW_FIRST, MENU_VIEW_FIRST + FILE_VIEW_DETAILS);

            wxBoxSizer* pFolderRow = new wxBoxSizer(wxHORIZONTAL);
            pFolderRow->Add(pLookInLabel, wxSizerFlags().CenterVertical());
            pFolderRow->AddSpacer(Scaled(8));
            pFolderRow->Add(pLookIn, wxSizerFlags(1).CenterVertical());
            pFolderRow->AddSpacer(Scaled(6));
            pFolderRow->Add(pToolBar, wxSizerFlags().CenterVertical());

            // File list
            pFileList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(Scaled(410), Scaled(122)), GetListStyle(fileViewMode) | wxLC_SINGLE_SEL | wxLC_EDIT_LABELS);
            pFileList->Bind(wxEVT_LIST_COL_CLICK, [this](wxListEvent& event) { OnColumnClick(event.GetColumn()); });
            pFileList->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event) { OnItemSelected(event.GetIndex()); });
            pFileList->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event) { OnItemActivated(event.GetIndex()); });
            pFileList->Bind(wxEVT_LIST_BEGIN_LABEL_EDIT, &SaveGameFileDialog::OnBeginLabelEdit, this);
            pFileList->Bind(wxEVT_LIST_END_LABEL_EDIT, &SaveGameFileDialog::OnEndLabelEdit, this);
            pFileList->Bind(wxEVT_LIST_KEY_DOWN, &SaveGameFileDialog::OnListKeyDown, this);

            // Name & type rows
            pNameEdit = new wxTextCtrl(this, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
            pNameEdit->SetMaxLength(60); // Same limit as original
            pNameEdit->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { OnButton(1); });

            const wxString filterText = JonesWx::FormatStr("JONES_STR_FILTER", "(.nds)");
            wxChoice* pTypeChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 1, &filterText);
            pTypeChoice->SetSelection(0);

            const wxSize buttonSize(Scaled(75), -1);
            pOkButton = new wxButton(this, wxID_ANY, bSave ? "&Save" : "&Open", wxDefaultPosition, buttonSize);
            pOkButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnButton(1); });
            pOkButton->SetDefault();

            wxButton* pCancelButton = new wxButton(this, wxID_ANY, "Cancel", wxDefaultPosition, buttonSize);
            pCancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnButton(2); });

            // Template part, preview of the save game and the dialog icon
            const wxSize thumbSize(Scaled(SITHSAVEGAME_THUMB_WIDTH), Scaled(SITHSAVEGAME_THUMB_HEIGHT));
            pThumbnail = new wxStaticBitmap(this, wxID_ANY, wxBitmap(thumbSize), wxDefaultPosition, thumbSize, wxBORDER_SUNKEN);

            wxFlexGridSizer* pNameGrid = new wxFlexGridSizer(2, 3, Scaled(6), Scaled(8));
            pNameGrid->Add(new wxStaticText(this, wxID_ANY, "File &name:"), wxSizerFlags().CenterVertical());
            pNameGrid->Add(pNameEdit, wxSizerFlags().Expand().CenterVertical());
            pNameGrid->Add(pOkButton, wxSizerFlags().CenterVertical());
            pNameGrid->Add(new wxStaticText(this, wxID_ANY, bSave ? "Save as &type:" : "Files of &type:"), wxSizerFlags().CenterVertical());
            pNameGrid->Add(pTypeChoice, wxSizerFlags().Expand().CenterVertical());
            pNameGrid->Add(pCancelButton, wxSizerFlags().CenterVertical());
            pNameGrid->AddGrowableCol(1);

            // Standard dialog controls on the left, template controls on the right
            wxGridBagSizer* pGrid = new wxGridBagSizer(Scaled(6), Scaled(10));
            pGrid->Add(pFolderRow, wxGBPosition(0, 0), wxDefaultSpan, wxEXPAND);
            pGrid->Add(pFileList, wxGBPosition(1, 0), wxDefaultSpan, wxEXPAND);
            pGrid->Add(pThumbnail, wxGBPosition(1, 1), wxDefaultSpan, wxALIGN_TOP);
            pGrid->Add(pNameGrid, wxGBPosition(2, 0), wxDefaultSpan, wxEXPAND);
            pGrid->Add(CreateResourceIcon(this, bSave ? 141 : 139, 64), wxGBPosition(2, 1), wxDefaultSpan, wxALIGN_CENTER); // Shown in real size as original
            pGrid->AddGrowableCol(0);
            pGrid->AddGrowableRow(1);

            // Initial folder and file
            wxString startDir = GetSaveGamesDir();
            wxString selName;
            if ( bSave )
            {
                // Preview of the game being saved.
                // Note, starts with empty name same as original, so a save isn't overwritten by accident.
                SetThumbnail(sithGamesave_CreateThumbnail(), /*bOwned=*/false);
            }
            else
            {
                // Select last saved/loaded game
                const char* pLastFile = sithGamesave_GetLastFilename();
                if ( pLastFile && pLastFile[0] )
                {
                    wxFileName lastFile(JonesWx::NativePath(pLastFile));
                    lastFile.MakeAbsolute();
                    if ( lastFile.FileExists() )
                    {
                        startDir = lastFile.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
                        selName  = lastFile.GetFullName();
                    }
                }
            }

            if ( !wxDirExists(startDir) ) {
                startDir = NormalizeDir(wxGetCwd());
            }

            curDir = startDir;
            RefreshFolder(selName, /*bSelectFirst=*/!bSave);
            pNameEdit->SetFocus();

            FinishLayout(pGrid);
        }

        // Returns selected save game file path
        std::string GetFilePath() const { return filePath; }

    protected:
        void OnButton(int resultID) override
        {
            // Enter & Escape finish label editing instead of closing the dialog
            if ( pFileList->GetEditControl() )
            {
                pFileList->EndEditLabel(/*cancel=*/resultID != 1);
                return;
            }

            if ( resultID == 1 && !OnOk() ) {
                return;
            }

            EndModal(resultID);
        }

    private:
        struct FileEntry
        {
            wxString name; // File name with extension, folder name or drive label
            wxString path; // Full path, folders and drives with trailing separator
            bool bFolder;
            bool bDrive;
            wxULongLong size;
            wxDateTime modTime;
            int image;
        };

        static long GetListStyle(int viewMode)
        {
            switch ( viewMode )
            {
                case FILE_VIEW_LARGEICONS: return wxLC_ICON;
                case FILE_VIEW_SMALLICONS: return wxLC_SMALL_ICON;
                case FILE_VIEW_DETAILS:    return wxLC_REPORT;
                default:                   return wxLC_LIST;
            }
        }

        bool IsComputerFolder() const { return curDir.empty(); }

        // Returns parent folder of curDir or false if there is none
        bool GetParentDir(wxString& parent) const
        {
            if ( IsComputerFolder() ) {
                return false;
            }

            wxFileName fn = wxFileName::DirName(curDir);
            if ( fn.GetDirCount() == 0 )
            {
            #ifdef __WXMSW__
                parent.clear(); // Drive root -> computer
                return true;
            #else
                return false;
            #endif
            }

            fn.RemoveLastDir();
            parent = fn.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
            return true;
        }

        void Navigate(const wxString& path)
        {
            const wxString dir = path.empty() ? path : NormalizeDir(path);
            if ( IsSamePath(dir, curDir) )
            {
                UpdateLookIn(); // Restore selection
                return;
            }

            if ( !dir.empty() && !wxDirExists(dir) )
            {
                wxBell();
                UpdateLookIn();
                return;
            }

            aBackHistory.push_back(curDir);
            curDir = dir;
            RefreshFolder(wxString(), /*bSelectFirst=*/false);
        }

        void OnTool(wxCommandEvent& event)
        {
            switch ( event.GetId() )
            {
                case TOOL_BACK:
                {
                    if ( !aBackHistory.empty() )
                    {
                        curDir = aBackHistory.back();
                        aBackHistory.pop_back();
                        RefreshFolder(wxString(), /*bSelectFirst=*/false);
                    }
                    break;
                }
                case TOOL_UP:
                    GoUp();
                    break;
                case TOOL_NEWFOLDER:
                    CreateNewFolder();
                    break;
                case TOOL_VIEW:
                {
                    // Clicking the button itself also opens the menu, same as original
                    wxPoint pt = pToolBar->ScreenToClient(wxGetMousePosition());
                    pt.y = pToolBar->GetClientSize().y;
                    pToolBar->PopupMenu(pViewMenu, pt);
                    break;
                }
            }
        }

        void GoUp()
        {
            wxString parent;
            if ( GetParentDir(parent) )
            {
                // Select the folder we came from
                wxString childName = wxFileName::DirName(curDir).GetDirCount() > 0 ? wxFileName::DirName(curDir).GetDirs().Last() : wxString();
                aBackHistory.push_back(curDir);
                curDir = parent;
                RefreshFolder(childName, /*bSelectFirst=*/false);
            }
        }

        void CreateNewFolder()
        {
            if ( IsComputerFolder() ) {
                return;
            }

            wxString name = "New Folder";
            for ( int num = 2; wxDirExists(curDir + name) || wxFileExists(curDir + name); ++num ) {
                name = wxString::Format("New Folder (%d)", num);
            }

            if ( !wxFileName::Mkdir(curDir + name) )
            {
                wxBell();
                return;
            }

            PopulateFileList(name, /*bSelectFirst=*/false);
            const long idx = pFileList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
            if ( idx >= 0 )
            {
                pFileList->SetFocus();
                pFileList->EditLabel(idx);
            }
        }

        void SetViewMode(int viewMode)
        {
            if ( viewMode == fileViewMode ) {
                return;
            }

            fileViewMode = viewMode;
            pViewMenu->Check(MENU_VIEW_FIRST + fileViewMode, true);

            const wxString selName = GetSelectedEntry() ? GetSelectedEntry()->name : wxString();
            pFileList->ClearAll();
            pFileList->SetSingleStyle(GetListStyle(fileViewMode));
            PopulateFileList(selName, /*bSelectFirst=*/false);
        }

        // Rebuilds folder combo, toolbar state and file list for curDir
        void RefreshFolder(const wxString& selName, bool bSelectFirst)
        {
            UpdateLookIn();

            wxString parent;
            pToolBar->EnableTool(TOOL_BACK, !aBackHistory.empty());
            pToolBar->EnableTool(TOOL_UP, GetParentDir(parent));
            pToolBar->EnableTool(TOOL_NEWFOLDER, !IsComputerFolder());

            if ( !bSave ) {
                SetThumbnail(nullptr, false);
            }

            ListFolder();
            PopulateFileList(selName, bSelectFirst);
        }

        void UpdateLookIn()
        {
            std::vector<LookInComboBox::Item> aItems;

            // Adds folder chain of curDir below its root
            auto addFolderChain = [&](const wxString& root, int indent) {
                wxFileName fn = wxFileName::DirName(curDir);
                wxString path = root;
                for ( const wxString& dir : fn.GetDirs() )
                {
                    path += dir + wxFILE_SEP_PATH;
                    aItems.push_back({ path, dir, ++indent, GetShellIcon(IconKind::Folder, smallIconSize) });
                }
            };

        #ifdef __WXMSW__
            wxString desktopName  = "Desktop";
            wxBitmap desktopIcon  = ShellSpecialFolderIcon(CSIDL_DESKTOP, smallIconSize, &desktopName);
            aItems.push_back({ NormalizeDir(wxStandardPaths::Get().GetUserDir(wxStandardPaths::Dir_Desktop)), desktopName, 0, desktopIcon });

            wxString computerName = "This PC";
            wxBitmap computerIcon = ShellSpecialFolderIcon(CSIDL_DRIVES, smallIconSize, &computerName);
            aItems.push_back({ wxString(), computerName, 1, computerIcon });

            bool bFound = IsComputerFolder();
            for ( const wxString& drive : GetDrives() )
            {
                aItems.push_back({ drive, ShellDisplayName(drive), 2, GetShellIcon(IconKind::Drive, smallIconSize, drive) });
                if ( !bFound && curDir.Upper().StartsWith(drive.Upper()) )
                {
                    addFolderChain(drive, 2);
                    bFound = true;
                }
            }

            if ( !bFound ) {
                aItems.push_back({ curDir, curDir, 2, GetShellIcon(IconKind::Folder, smallIconSize) }); // e.g. network path
            }
        #else
            const wxString homeDir = NormalizeDir(wxGetHomeDir());
            aItems.push_back({ homeDir, wxFileName::DirName(homeDir).GetDirs().empty() ? homeDir : wxFileName::DirName(homeDir).GetDirs().Last(), 0, GetShellIcon(IconKind::Home, smallIconSize) });
            aItems.push_back({ "/", "/", 0, GetShellIcon(IconKind::Drive, smallIconSize) });
            addFolderChain("/", 0);
        #endif

            // Select the deepest match, i.e. the current folder in the chain
            int selection = -1;
            for ( int i = (int)aItems.size() - 1; i >= 0; --i )
            {
                if ( IsSamePath(aItems[i].path, curDir) )
                {
                    selection = i;
                    break;
                }
            }

            pLookIn->SetFolders(std::move(aItems), selection);
        }

        void ListFolder()
        {
            aEntries.clear();

            if ( IsComputerFolder() )
            {
            #ifdef __WXMSW__
                for ( const wxString& drive : GetDrives() ) {
                    aEntries.push_back({ ShellDisplayName(drive), drive, /*bFolder=*/true, /*bDrive=*/true, 0, wxDateTime(), -1 });
                }
            #endif
                return;
            }

            wxDir dir(curDir);
            if ( !dir.IsOpened() ) {
                return;
            }

            wxString name;
            for ( bool bFound = dir.GetFirst(&name, wxString(), wxDIR_DIRS); bFound; bFound = dir.GetNext(&name) )
            {
                const wxString path = curDir + name + wxFILE_SEP_PATH;
                aEntries.push_back({ name, path, true, false, 0, wxFileName::DirName(path).GetModificationTime(), -1 });
            }

            for ( bool bFound = dir.GetFirst(&name, wxString("*.") + saveGameExt, wxDIR_FILES); bFound; bFound = dir.GetNext(&name) )
            {
                wxFileName fn(curDir, name);
                aEntries.push_back({ name, fn.GetFullPath(), false, false, fn.GetSize(), fn.GetModificationTime(), -1 });
            }
        }

        static wxString GetTypeName(const FileEntry& entry)
        {
            if ( entry.bDrive ) {
                return "Drive";
            }
            return entry.bFolder ? "File folder" : wxString(saveGameExt).Upper() + " File";
        }

        static wxString GetSizeText(const FileEntry& entry)
        {
            if ( entry.bFolder || entry.size == wxInvalidSize ) {
                return wxString();
            }

            const wxULongLong kb = (entry.size + 1023) / 1024;
            return wxNumberFormatter::ToString((long)kb.GetValue(), wxNumberFormatter::Style_WithThousandsSep) + " KB";
        }

        void SortEntries()
        {
            const int column = fileSortColumn;
            const bool bAscending = bFileSortAscending;
            std::stable_sort(aEntries.begin(), aEntries.end(), [column, bAscending](const FileEntry& a, const FileEntry& b) {
                // Folders first
                if ( a.bFolder != b.bFolder ) {
                    return a.bFolder;
                }

                int cmp = 0;
                switch ( column )
                {
                    case FILE_COLUMN_DATE:
                        if ( a.modTime.IsValid() && b.modTime.IsValid() ) {
                            cmp = a.modTime.IsEarlierThan(b.modTime) ? -1 : (a.modTime.IsLaterThan(b.modTime) ? 1 : 0);
                        }
                        break;
                    case FILE_COLUMN_SIZE:
                        cmp = a.size < b.size ? -1 : (a.size > b.size ? 1 : 0);
                        break;
                    case FILE_COLUMN_TYPE:
                        cmp = GetTypeName(a).CmpNoCase(GetTypeName(b));
                        break;
                }

                if ( cmp == 0 ) {
                    cmp = a.name.CmpNoCase(b.name);
                }

                return bAscending ? cmp < 0 : cmp > 0;
            });
        }

        // Sorts entries and fills the list, selecting the entry named selName
        void PopulateFileList(const wxString& selName, bool bSelectFirst)
        {
            SortEntries();

            // Icons
            wxImageList* pSmallImages = new wxImageList(smallIconSize, smallIconSize, true);
            wxImageList* pLargeImages = new wxImageList(largeIconSize, largeIconSize, true);
            std::map<wxString, int> imageMap;
            auto getImage = [&](IconKind kind, const wxString& key, const wxString& path) {
                auto it = imageMap.find(key);
                if ( it != imageMap.end() ) {
                    return it->second;
                }

                const int image = pSmallImages->Add(GetShellIcon(kind, smallIconSize, path));
                pLargeImages->Add(GetShellIcon(kind, largeIconSize, path));
                imageMap[key] = image;
                return image;
            };

            for ( FileEntry& entry : aEntries )
            {
                if ( entry.bDrive ) {
                    entry.image = getImage(IconKind::Drive, entry.path, entry.path);
                }
                else {
                    entry.image = entry.bFolder ? getImage(IconKind::Folder, "folder", wxString()) : getImage(IconKind::SaveFile, "file", wxString());
                }
            }

            pFileList->Freeze();
            pFileList->DeleteAllItems();
            pFileList->AssignImageList(pSmallImages, wxIMAGE_LIST_SMALL);
            pFileList->AssignImageList(pLargeImages, wxIMAGE_LIST_NORMAL);

            const bool bDetails = fileViewMode == FILE_VIEW_DETAILS;
            if ( bDetails && pFileList->GetColumnCount() == 0 )
            {
                pFileList->AppendColumn("Name", wxLIST_FORMAT_LEFT, Scaled(150));
                pFileList->AppendColumn("Size", wxLIST_FORMAT_RIGHT, Scaled(64));
                pFileList->AppendColumn("Type", wxLIST_FORMAT_LEFT, Scaled(70));
                pFileList->AppendColumn("Modified", wxLIST_FORMAT_LEFT, Scaled(120));
            }

            long selIdx = -1;
            for ( size_t i = 0; i < aEntries.size(); ++i )
            {
                const FileEntry& entry = aEntries[i];
                const long idx = pFileList->InsertItem((long)i, entry.name, entry.image);
                if ( bDetails )
                {
                    pFileList->SetItem(idx, FILE_COLUMN_SIZE, GetSizeText(entry));
                    pFileList->SetItem(idx, FILE_COLUMN_TYPE, GetTypeName(entry));
                    pFileList->SetItem(idx, FILE_COLUMN_DATE, entry.modTime.IsValid() ? entry.modTime.Format("%x %H:%M") : wxString());
                }

                if ( selIdx < 0 && !selName.empty() && IsSamePath(entry.name, selName) ) {
                    selIdx = idx;
                }
            }

            if ( bDetails ) {
                pFileList->ShowSortIndicator(fileSortColumn, bFileSortAscending);
            }

            pFileList->Thaw();

            if ( bSelectFirst && selIdx < 0 )
            {
                // First file, skipping folders
                for ( size_t i = 0; i < aEntries.size() && selIdx < 0; ++i ) {
                    selIdx = aEntries[i].bFolder ? -1 : (long)i;
                }
            }

            if ( selIdx >= 0 )
            {
                pFileList->SetItemState(selIdx, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
                pFileList->EnsureVisible(selIdx);
                OnItemSelected(selIdx); // Not all ports send selection event for programmatic change
            }
        }

        const FileEntry* GetEntry(long idx) const
        {
            return idx >= 0 && (size_t)idx < aEntries.size() ? &aEntries[idx] : nullptr;
        }

        const FileEntry* GetSelectedEntry() const
        {
            return GetEntry(pFileList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED));
        }

        // Clicking a column header sorts by it, clicking the sorted column again reverses the order
        void OnColumnClick(int column)
        {
            if ( column < FILE_COLUMN_NAME || column > FILE_COLUMN_DATE ) {
                return;
            }

            if ( column == fileSortColumn ) {
                bFileSortAscending = !bFileSortAscending;
            }
            else
            {
                fileSortColumn     = column;
                bFileSortAscending = column == FILE_COLUMN_NAME || column == FILE_COLUMN_TYPE; // Newest / largest first
            }

            // Keep the selected file selected, but don't change the typed name
            const FileEntry* pSel = GetSelectedEntry();
            const wxString selName   = pSel ? pSel->name : wxString();
            const wxString typedName = pNameEdit->GetValue();
            PopulateFileList(selName, /*bSelectFirst=*/false);
            pNameEdit->ChangeValue(typedName);
        }

        void OnItemSelected(long idx)
        {
            const FileEntry* pEntry = GetEntry(idx);
            if ( !pEntry || pEntry->bFolder )
            {
                if ( !bSave ) {
                    SetThumbnail(nullptr, false);
                }
                return;
            }

            pNameEdit->ChangeValue(pEntry->name);

            if ( !bSave )
            {
                // Preview of the selected save game
                const std::string path = JonesWx::ToGame(pEntry->path);
                SetThumbnail(sithGamesave_LoadThumbnail(path.c_str()), /*bOwned=*/true);
            }
        }

        void OnItemActivated(long idx)
        {
            const FileEntry* pEntry = GetEntry(idx);
            if ( !pEntry ) {
                return;
            }

            if ( pEntry->bFolder )
            {
                const wxString path = pEntry->path;
                CallAfter([this, path]() { Navigate(path); });
                return;
            }

            pNameEdit->ChangeValue(pEntry->name);
            OnButton(1);
        }

        void OnListKeyDown(wxListEvent& event)
        {
            switch ( event.GetKeyCode() )
            {
                case WXK_BACK:
                    CallAfter([this]() { GoUp(); });
                    return;
                case WXK_F2:
                    if ( event.GetIndex() >= 0 ) {
                        pFileList->EditLabel(event.GetIndex());
                    }
                    return;
                case WXK_F5:
                {
                    const FileEntry* pSel = GetSelectedEntry();
                    RefreshFolder(pSel ? pSel->name : wxString(), /*bSelectFirst=*/false);
                    return;
                }
            }

            event.Skip();
        }

        void OnBeginLabelEdit(wxListEvent& event)
        {
            const FileEntry* pEntry = GetEntry(event.GetIndex());
            if ( !pEntry || pEntry->bDrive ) {
                event.Veto();
            }
        }

        // Renames file or folder
        void OnEndLabelEdit(wxListEvent& event)
        {
            event.Veto(); // List is refilled after rename
            const FileEntry* pEntry = GetEntry(event.GetIndex());
            if ( event.IsEditCancelled() || !pEntry ) {
                return;
            }

            wxString newName = event.GetLabel().Strip(wxString::both);
            if ( newName.empty() || newName == pEntry->name ) {
                return;
            }

            if ( !pEntry->bFolder && !HasSaveGameExt(newName) ) {
                newName += wxString(".") + saveGameExt;
            }

            const wxString newPath = curDir + newName;
            const bool bCaseRename = IsSamePath(newName, pEntry->name);
            if ( newName.find_first_of(aInvalidNameChars) != wxString::npos
                || (!bCaseRename && (wxFileExists(newPath) || wxDirExists(newPath)))
                || !wxRenameFile(pEntry->bFolder ? pEntry->path.Left(pEntry->path.length() - 1) : pEntry->path, newPath, /*overwrite=*/false) )
            {
                ShowInvalidFileMessage("JONES_STR_INVALIDFILE", JonesWx::ToGame(newName));
                return;
            }

            CallAfter([this, newName]() {
                ListFolder();
                PopulateFileList(newName, /*bSelectFirst=*/false);
            });
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

        // Handles Open/Save button. Returns true when a file was chosen.
        bool OnOk()
        {
            const wxString text = pNameEdit->GetValue().Strip(wxString::both);
            if ( text.empty() )
            {
                // Opens the selected folder, same as original
                const FileEntry* pSel = GetSelectedEntry();
                if ( pSel && pSel->bFolder )
                {
                    const wxString path = pSel->path;
                    CallAfter([this, path]() { Navigate(path); });
                }
                return false;
            }

            if ( IsComputerFolder() && !wxFileName(text).IsAbsolute() )
            {
                wxBell();
                return false;
            }

            // Typed folder name or path opens the folder
            wxFileName fn(text);
            if ( !fn.IsAbsolute() ) {
                fn.MakeAbsolute(curDir);
            }

            if ( wxDirExists(fn.GetFullPath()) )
            {
                pNameEdit->Clear();
                Navigate(fn.GetFullPath());
                return false;
            }

            if ( fn.GetExt().empty() ) {
                fn.SetExt(saveGameExt);
            }

            const std::string filename = JonesWx::ToGame(fn.GetFullName());
            return bSave ? ValidateSave(fn, filename) : ValidateLoad(fn, filename);
        }

        bool ValidateSave(const wxFileName& fn, const std::string& filename)
        {
            if ( fn.GetName().empty()
                || fn.GetFullName().find_first_of(aInvalidNameChars) != wxString::npos
                || !fn.GetExt().IsSameAs(saveGameExt, /*caseSensitive=*/false)
                || !wxDirExists(fn.GetPath())
                || IsReservedSaveName(filename) )
            {
                ShowInvalidFileMessage("JONES_STR_INVALIDFILE", JonesWx::ToGame(pNameEdit->GetValue()));
                return false;
            }

            if ( fn.FileExists() && jonesConfig_ShowOverwriteSaveGameDlg((HWND)GetHandle(), filename.c_str()) != 1 ) {
                return false;
            }

            return SetResultPath(fn, filename);
        }

        bool ValidateLoad(const wxFileName& fn, const std::string& filename)
        {
            if ( !fn.FileExists() || !fn.GetExt().IsSameAs(saveGameExt, /*caseSensitive=*/false) )
            {
                ShowInvalidFileMessage("JONES_STR_NOFILE", filename);
                return false;
            }

            // Warn about losing current progress, unless player is dead
            if ( JonesMain_HasStarted()
                && (sithPlayer_g_pLocalPlayerThing->flags & (SITH_TF_DYING | SITH_TF_DESTROYED)) == 0
                && jonesConfig_ShowLoadGameWarningMsgBox((HWND)GetHandle()) != 1 )
            {
                return false;
            }

            return SetResultPath(fn, filename);
        }

        // Sets game file path, relative to current dir when shorter as the game's path buffer is small
        bool SetResultPath(const wxFileName& fn, const std::string& filename)
        {
            std::string path = JonesWx::ToGame(fn.GetFullPath());

            wxFileName relative(fn);
            if ( relative.MakeRelativeTo(wxGetCwd()) )
            {
                const std::string relPath = JonesWx::ToGame(relative.GetFullPath());
                if ( relPath.size() < path.size() ) {
                    path = relPath;
                }
            }

            if ( path.size() >= JONESCONFIG_GAMESAVE_FILEPATHSIZE )
            {
                ShowInvalidFileMessage(bSave ? "JONES_STR_INVALIDFILE" : "JONES_STR_NOFILE", filename);
                return false;
            }

            filePath = path;
            return true;
        }

        bool bSave;
        int smallIconSize;
        int largeIconSize;

        wxString curDir; // Absolute with trailing separator, empty = computer (drive list)
        std::vector<wxString> aBackHistory;
        std::vector<FileEntry> aEntries;
        std::string filePath;

        LookInComboBox* pLookIn;
        wxToolBar* pToolBar;
        wxMenu* pViewMenu;
        wxListCtrl* pFileList;
        wxTextCtrl* pNameEdit;
        wxButton* pOkButton;
        wxStaticBitmap* pThumbnail;
    };

    int ShowSaveGameFileDialog(HWND hWnd, bool bSave, char* pOutFilePath)
    {
        if ( !JonesWx::EnsureInit() ) {
            return 2;
        }

        SaveGameFileDialog dlg(hWnd, bSave);
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
    return ShowSaveGameFileDialog(hWnd, /*bSave=*/true, pOutFilePath);
}

int J3DAPI jonesConfig_GetLoadGameFilePath(HWND hWnd, char* pDestNdsPath)
{
    return ShowSaveGameFileDialog(hWnd, /*bSave=*/false, pDestNdsPath);
}

} // extern "C"
