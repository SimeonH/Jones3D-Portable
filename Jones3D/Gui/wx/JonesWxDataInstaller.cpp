// Game data installer: copies the data files of the original game to the install dir (default ~/.Jones3D)
// from a Steam installation, the GOG offline installer (unpacked with innoextract) or the original CDs.
// Shown before the game starts when the data files are not found, see Indy3D_EnsureGameData.
#include "JonesWx.h"
#include <Jones3D/Gui/wx/JonesWxDialogs.h>

#include <wx/dir.h>
#include <wx/dirdlg.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/process.h>
#include <wx/progdlg.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include <stdlib.h>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace
{
    // Required files are checked the same as in Indy3D_HasGameData
    struct DataFile
    {
        const char* pName;
        bool bRequired;
    };

    const DataFile aResourceFiles[] = {
        { "Jones3D.GOB",    true  },
        { "CD1.GOB",        true  },
        { "CD2.GOB",        true  },
        { "Indy3D.exe",     false }, // Dialog icons
        { "jonesopn.snm",   false }, // Intro movies
        { "jonesopn_3.snm", false },
        { "jonesopn_4.snm", false },
        { "jonesopn_5.snm", false },
        { "jonesopn_8.snm", false },
    };

    const char* aRootFiles[]   = { "Jones3D.hlp", "Jones3D.cnt" };

    // Pictures, sounds and texts of the launcher (JonesWxLauncher.cpp) in the Install folder of the CD
    const char* aLauncherFiles[] = {
        "launch.bmp", "options.bmp", "system.bmp",
        "open.wav", "click.wav", "back.wav", "start.wav",
        "setup.tab",
    };
    const char* aGogSetupGlob  = "setup_indiana_jones_and_the_infernal_machine*.exe";
    const char* aGogExtractDir = ".gog-extract";

    struct CopyItem
    {
        wxString srcPath;
        wxString dstRelPath; // Relative to install dir, '/' separated
        wxULongLong size;
    };

    wxString JoinPath(const wxString& dir, const wxString& name)
    {
        return dir.EndsWith(wxFILE_SEP_PATH) ? dir + name : dir + wxFILE_SEP_PATH + name;
    }

    // Finds directory entry case-insensitively, CD file systems and Windows installs don't have consistent case
    wxString FindEntry(const wxString& dir, const wxString& name, bool bDir)
    {
        wxLogNull noLog; // Unreadable dirs are expected while probing
        if ( dir.empty() || !wxDir::Exists(dir) ) {
            return wxString();
        }

        const wxString path = JoinPath(dir, name);
        if ( bDir ? wxDir::Exists(path) : wxFileExists(path) ) {
            return path;
        }

        wxDir dirList(dir);
        if ( !dirList.IsOpened() ) {
            return wxString();
        }

        wxString entry;
        const int flags = (bDir ? wxDIR_DIRS : wxDIR_FILES) | wxDIR_HIDDEN;
        for ( bool bCont = dirList.GetFirst(&entry, wxEmptyString, flags); bCont; bCont = dirList.GetNext(&entry) )
        {
            if ( entry.CmpNoCase(name) == 0 ) {
                return JoinPath(dir, entry);
            }
        }

        return wxString();
    }

    // Resolves '/' separated relPath under root case-insensitively
    wxString FindPath(const wxString& root, const wxString& relPath, bool bDir = false)
    {
        const wxArrayString parts = wxSplit(relPath, '/', 0);
        wxString path = root;
        for ( size_t i = 0; i < parts.size() && !path.empty(); ++i ) {
            path = FindEntry(path, parts[i], i + 1 < parts.size() || bDir);
        }
        return path;
    }

    wxString CanonicalDir(const wxString& dir)
    {
#ifndef __WXMSW__
        char* pReal = realpath(dir.fn_str(), nullptr);
        if ( pReal )
        {
            wxString result = wxString(pReal, *wxConvFileName);
            free(pReal);
            return result;
        }
#endif
        wxFileName fn = wxFileName::DirName(dir);
        fn.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_TILDE);
        return fn.GetPath();
    }

    // A game installation (or CD) root has the GOB files in its Resource dir
    bool HasResourceFile(const wxString& root, const char* pName)
    {
        return !FindPath(root, wxString("Resource/") + pName).empty() || !FindPath(root, pName).empty();
    }

    void AddItem(std::vector<CopyItem>& items, const wxString& srcPath, const wxString& dstRelPath)
    {
        for ( const CopyItem& item : items )
        {
            if ( item.dstRelPath.CmpNoCase(dstRelPath) == 0 ) {
                return; // Already found, e.g. on the first CD
            }
        }

        items.push_back({ srcPath, dstRelPath, wxFileName::GetSize(srcPath) });
    }

    void AddDirFiles(std::vector<CopyItem>& items, const wxString& srcDir, const wxString& dstRelDir, const wxString& ext = wxString())
    {
        if ( srcDir.empty() ) {
            return;
        }

        wxLogNull noLog;
        wxDir dir(srcDir);
        if ( !dir.IsOpened() ) {
            return;
        }

        wxString name;
        for ( bool bCont = dir.GetFirst(&name, wxEmptyString, wxDIR_FILES); bCont; bCont = dir.GetNext(&name) )
        {
            if ( ext.empty() || wxFileName(name).GetExt().CmpNoCase(ext) == 0 ) {
                AddItem(items, JoinPath(srcDir, name), dstRelDir + "/" + name);
            }
        }
    }

    // Collects the data files found under root (game installation, extracted installer or CD)
    void CollectDataFiles(const wxString& root, std::vector<CopyItem>& items, bool bSaveGames, const wxString& installDir)
    {
        for ( const DataFile& file : aResourceFiles )
        {
            wxString path = FindPath(root, wxString("Resource/") + file.pName);
            if ( path.empty() ) {
                path = FindPath(root, file.pName);
            }

            if ( !path.empty() ) {
                AddItem(items, path, wxString("Resource/") + file.pName);
            }
        }

        for ( const char* pName : aRootFiles )
        {
            const wxString path = FindPath(root, pName);
            if ( !path.empty() ) {
                AddItem(items, path, pName);
            }
        }

        for ( const char* pName : aLauncherFiles )
        {
            const wxString path = FindPath(root, wxString("Install/") + pName);
            if ( !path.empty() ) {
                AddItem(items, path, wxString("Install/") + pName);
            }
        }

        AddDirFiles(items, FindPath(root, "Resource/Cog", /*bDir=*/true), "Resource/Cog"); // Steam version COG fixes
        AddDirFiles(items, FindPath(root, "KeySets", /*bDir=*/true), "KeySets", "kfg");

        if ( bSaveGames )
        {
            // Existing save games are never overwritten
            std::vector<CopyItem> saves;
            AddDirFiles(saves, FindPath(root, "SaveGames", /*bDir=*/true), "SaveGames", "nds");
            for ( const CopyItem& item : saves )
            {
                if ( !wxFileExists(JoinPath(installDir, item.dstRelPath)) ) {
                    AddItem(items, item.srcPath, item.dstRelPath);
                }
            }
        }
    }

    wxArrayString GetMissingRequiredFiles(const std::vector<CopyItem>& items)
    {
        wxArrayString missing;
        for ( const DataFile& file : aResourceFiles )
        {
            if ( !file.bRequired ) {
                continue;
            }

            bool bFound = false;
            for ( const CopyItem& item : items ) {
                bFound |= item.dstRelPath.CmpNoCase(wxString("Resource/") + file.pName) == 0;
            }

            if ( !bFound ) {
                missing.push_back(file.pName);
            }
        }
        return missing;
    }

    // Reads text file as UTF-8 (GOG EULA) or Windows-1252 (CD license)
    wxString ReadTextFile(const wxString& path)
    {
        if ( path.empty() || !wxFileExists(path) ) {
            return wxString();
        }

        wxLogNull noLog;
        wxFile file(path);
        if ( !file.IsOpened() ) {
            return wxString();
        }

        std::string data((size_t)file.Length(), '\0');
        if ( data.empty() || file.Read(&data[0], data.size()) != (ssize_t)data.size() ) {
            return wxString();
        }

        if ( data.compare(0, 3, "\xEF\xBB\xBF") == 0 ) {
            data.erase(0, 3);
        }

        wxString text = wxString::FromUTF8(data.c_str(), data.size());
        if ( text.empty() ) {
            text = wxString(data.c_str(), wxCSConv(wxFONTENCODING_CP1252), data.size());
        }

        text.Replace("\r\n", "\n");
        return text;
    }

    // Returns the GOG offline installer found in the downloads folder, if any
    wxString FindGogInstaller()
    {
        wxLogNull noLog;
        const wxString aDirs[] = { wxStandardPaths::Get().GetUserDir(wxStandardPaths::Dir_Downloads), JoinPath(wxGetHomeDir(), "Downloads") };
        for ( const wxString& dir : aDirs )
        {
            wxArrayString files;
            if ( wxDir::Exists(dir) ) {
                wxDir::GetAllFiles(dir, &files, aGogSetupGlob, wxDIR_FILES);
            }

            if ( !files.empty() )
            {
                files.Sort();
                return files.back(); // Newest version
            }
        }
        return wxString();
    }

    bool IsDiscRoot(const wxString& dir, int discNum)
    {
        if ( discNum == 1 ) {
            return !FindPath(dir, "install/license.txt").empty() && HasResourceFile(dir, "CD1.GOB");
        }
        return HasResourceFile(dir, "CD2.GOB");
    }

    // Returns the mount points of game discs, discNum 0 finds both discs
    wxArrayString FindMountedDiscs(int discNum)
    {
        wxArrayString result;
#ifndef __WXMSW__
        wxLogNull noLog;
        const wxString user = wxGetUserId();
        const wxString aMountBases[] = { "/run/media/" + user, "/media/" + user, "/media", "/mnt" };
        for ( const wxString& base : aMountBases )
        {
            if ( !wxDir::Exists(base) ) {
                continue;
            }

            wxDir dir(base);
            if ( !dir.IsOpened() ) {
                continue;
            }

            wxString name;
            for ( bool bCont = dir.GetFirst(&name, wxEmptyString, wxDIR_DIRS); bCont; bCont = dir.GetNext(&name) )
            {
                const wxString path = JoinPath(base, name);
                const bool bDisc = discNum ? IsDiscRoot(path, discNum) : IsDiscRoot(path, 1) || IsDiscRoot(path, 2);
                if ( bDisc && result.Index(path) == wxNOT_FOUND ) {
                    result.push_back(path);
                }
            }
        }
#else
        (void)discNum;
#endif
        return result;
    }

    wxString FormatSize(wxULongLong size)
    {
        return wxFileName::GetHumanReadableSize(size, "0 B", 1, wxSIZE_CONV_IEC);
    }


    bool CheckDiskSpace(wxWindow* pParent, const wxString& installDir, wxULongLong needed)
    {
        wxDiskspaceSize_t freeBytes = 0;
        if ( !wxGetDiskSpace(installDir, nullptr, &freeBytes) || wxULongLong((wxULongLong_t)freeBytes.GetValue()) >= needed ) {
            return true;
        }

        wxMessageBox(wxString::Format("Not enough free disk space in %s.\n\n%s is needed, only %s is available.",
            installDir, FormatSize(needed), FormatSize(wxULongLong((wxULongLong_t)freeBytes.GetValue()))),
            "Install Game Data", wxOK | wxICON_ERROR, pParent);
        return false;
    }

    // Copies (or moves, when bMove) the files to the install dir. Returns false on error or when canceled.
    bool InstallFiles(wxWindow* pParent, const std::vector<CopyItem>& items, const wxString& installDir, const wxString& title, bool bMove)
    {
        wxULongLong total = 0;
        for ( const CopyItem& item : items ) {
            total += item.size;
        }

        if ( !bMove && !CheckDiskSpace(pParent, installDir, total) ) {
            return false;
        }

        constexpr int progressRange = 1000;
        wxProgressDialog progress(title, wxString::Format("Preparing to copy %s...", FormatSize(total)), progressRange, pParent,
            wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME);

        wxLogNull noLog; // Errors are reported below
        std::vector<char> buffer(1024 * 1024);
        wxULongLong done      = 0;
        wxLongLong lastUpdate = 0;

        for ( const CopyItem& item : items )
        {
            const wxString dstPath = JoinPath(installDir, item.dstRelPath);
            const wxString dstDir  = wxFileName(dstPath).GetPath();
            if ( !wxFileName::Mkdir(dstDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) )
            {
                wxMessageBox("Couldn't create folder " + dstDir, title, wxOK | wxICON_ERROR, pParent);
                return false;
            }

            const wxString message = wxString::Format("Installing %s (%s)", item.dstRelPath, FormatSize(item.size));
            if ( bMove && wxRenameFile(item.srcPath, dstPath, /*overwrite=*/true) )
            {
                done += item.size;
                if ( !progress.Update((int)(done.ToDouble() * progressRange / (total > 0 ? total.ToDouble() : 1.0)), message) ) {
                    return false;
                }
                continue;
            }

            // Copied to temp file first so canceling never leaves a truncated data file
            wxFile src(item.srcPath, wxFile::read);
            const wxString tmpPath = dstPath + ".part";
            wxFile dst;
            if ( !src.IsOpened() || !dst.Create(tmpPath, /*overwrite=*/true, wxS_DEFAULT) )
            {
                wxMessageBox(wxString::Format("Couldn't copy %s to %s.", item.srcPath, dstPath), title, wxOK | wxICON_ERROR, pParent);
                return false;
            }

            for ( ;; )
            {
                const ssize_t numRead = src.Read(buffer.data(), buffer.size());
                if ( numRead == wxInvalidOffset || (numRead > 0 && dst.Write(buffer.data(), (size_t)numRead) != (size_t)numRead) )
                {
                    dst.Close();
                    wxRemoveFile(tmpPath);
                    wxMessageBox(wxString::Format("Couldn't copy %s to %s.\n\nIs the disc readable and is there enough disk space?", item.srcPath, dstPath),
                        title, wxOK | wxICON_ERROR, pParent);
                    return false;
                }

                if ( numRead == 0 ) {
                    break;
                }

                done += (wxULongLong_t)numRead;
                const wxLongLong now = wxGetLocalTimeMillis();
                if ( now - lastUpdate >= 100 )
                {
                    lastUpdate = now;
                    if ( !progress.Update((int)(done.ToDouble() * progressRange / (total > 0 ? total.ToDouble() : 1.0)), message) )
                    {
                        dst.Close();
                        wxRemoveFile(tmpPath);
                        return false;
                    }
                }
            }

            dst.Close();
            if ( !wxRenameFile(tmpPath, dstPath, /*overwrite=*/true) )
            {
                wxRemoveFile(tmpPath);
                wxMessageBox("Couldn't create " + dstPath, title, wxOK | wxICON_ERROR, pParent);
                return false;
            }
        }

        progress.Update(progressRange);
        return true;
    }

    // Child process with redirected output, owned by the caller
    class ChildProcess : public wxProcess
    {
    public:
        ChildProcess() : wxProcess(wxPROCESS_REDIRECT) {}

        void OnTerminate(int /*pid*/, int status) override
        {
            exitCode = status;
            bDone    = true;
        }

        void ReadOutput()
        {
            Read(GetInputStream(), output);
            Read(GetErrorStream(), errors);
        }

        bool bDone   = false;
        int exitCode = -1;
        std::string output;
        std::string errors;

    private:
        static void Read(wxInputStream* pStream, std::string& out)
        {
            char aBuf[4096];
            while ( pStream && pStream->CanRead() )
            {
                pStream->Read(aBuf, sizeof(aBuf));
                const size_t numRead = pStream->LastRead();
                if ( !numRead ) {
                    break;
                }
                out.append(aBuf, numRead);
            }
        }
    };

    enum ProcessResult
    {
        PROCESS_OK,
        PROCESS_FAILED,
        PROCESS_CANCELED,
        PROCESS_NOTFOUND,
    };

    // Runs program and keeps the UI responsive until it exits. pfPoll is called periodically with the output so far,
    // it returns false to cancel. pErrors receives the error output on failure.
    ProcessResult RunProcess(const std::vector<wxString>& args, const std::function<bool(const std::string&)>& pfPoll, wxString* pErrors)
    {
        std::vector<wxWCharBuffer> argBufs;
        for ( const wxString& arg : args ) {
            argBufs.push_back(arg.wc_str());
        }

        std::vector<const wchar_t*> argv;
        for ( const wxWCharBuffer& arg : argBufs ) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        ChildProcess* pProcess = new ChildProcess();
        const long pid = wxExecute(argv.data(), wxEXEC_ASYNC, pProcess);
        if ( pid <= 0 )
        {
            delete pProcess;
            return PROCESS_NOTFOUND;
        }

        bool bCanceled = false;
        while ( !pProcess->bDone )
        {
            wxMilliSleep(20);
            wxTheApp->Yield(/*onlyIfNeeded=*/true); // Also dispatches the process termination notification
            pProcess->ReadOutput();

            if ( !bCanceled && !pfPoll(pProcess->output) )
            {
                bCanceled = true;
                wxProcess::Kill((int)pid, wxSIGTERM, wxKILL_CHILDREN);
            }
        }

        pProcess->ReadOutput();
        const int exitCode = pProcess->exitCode;
        if ( pErrors ) {
            *pErrors = wxString::FromUTF8(pProcess->errors.c_str());
        }
        delete pProcess;

        if ( bCanceled ) {
            return PROCESS_CANCELED;
        }
        return exitCode == 0 ? PROCESS_OK : PROCESS_FAILED;
    }

    // Parses overall progress percentage from the last "] 45.3%" of innoextract's progress bar
    double ParseInnoextractProgress(const std::string& output)
    {
        const size_t percent = output.rfind('%');
        if ( percent == std::string::npos ) {
            return 0.0;
        }

        size_t start = percent;
        while ( start > 0 && (isdigit((unsigned char)output[start - 1]) || output[start - 1] == '.') ) {
            --start;
        }
        return start < percent ? atof(output.substr(start, percent - start).c_str()) : 0.0;
    }

    // Returns the name of the file innoextract is extracting, from its last ' - "Resource/CD1.GOB"' line
    wxString ParseInnoextractFile(const std::string& output)
    {
        const size_t start = output.rfind(" - \"");
        if ( start == std::string::npos ) {
            return wxString();
        }

        const size_t end = output.find('"', start + 4);
        return end == std::string::npos ? wxString() : wxString::FromUTF8(output.substr(start + 4, end - start - 4).c_str());
    }

    bool IsProgramInPath(const wxString& name)
    {
        wxString path;
        wxGetEnv("PATH", &path);
        for ( const wxString& dir : wxSplit(path, wxPATH_SEP[0], 0) )
        {
            if ( !dir.empty() && wxFileName::IsFileExecutable(JoinPath(dir, name)) ) {
                return true;
            }
        }
        return false;
    }

    // Shows license agreement, returns true when accepted
    bool ShowLicenseAgreement(wxWindow* pParent, const wxString& text)
    {
        wxDialog dialog(pParent, wxID_ANY, "License Agreement", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

        wxBoxSizer* pSizer = new wxBoxSizer(wxVERTICAL);
        pSizer->Add(new wxStaticText(&dialog, wxID_ANY, "Please read the following license agreement. You must accept it to install the game."),
            wxSizerFlags().Border(wxALL));

        wxTextCtrl* pText = new wxTextCtrl(&dialog, wxID_ANY, text, wxDefaultPosition, dialog.FromDIP(wxSize(640, 440)),
            wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
        pSizer->Add(pText, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT));

        wxBoxSizer* pButtons = new wxBoxSizer(wxHORIZONTAL);
        pButtons->AddStretchSpacer();
        wxButton* pAccept = new wxButton(&dialog, wxID_OK, "I &Accept");
        pButtons->Add(pAccept, wxSizerFlags().Border(wxRIGHT));
        pButtons->Add(new wxButton(&dialog, wxID_CANCEL, "&Decline"));
        pSizer->Add(pButtons, wxSizerFlags().Expand().Border(wxALL));

        dialog.SetSizerAndFit(pSizer);
        dialog.SetEscapeId(wxID_CANCEL);
        pAccept->SetFocus();
        dialog.Centre();
        return dialog.ShowModal() == wxID_OK;
    }

    class DataInstallerDialog : public wxDialog
    {
    public:
        explicit DataInstallerDialog(const wxString& installDir);

    private:
        enum Source { SOURCE_STEAM, SOURCE_GOG, SOURCE_CD };

        wxSizer* CreateSourceRow(wxWindow* pControl, std::function<void()> pfBrowse, wxButton** ppBrowse);
        void UpdateControls();
        void OnInstall();

        bool InstallFromFolder();
        bool InstallFromGog();
        bool InstallFromCDs();
        bool Finish(const std::vector<CopyItem>& items, const wxString& sourceDescription);

        void ShowError(const wxString& message) { wxMessageBox(message, "Install Game Data", wxOK | wxICON_ERROR, this); }

        wxString installDir;
        bool bInnoextract = false;

        wxRadioButton* pSteamRadio = nullptr;
        wxRadioButton* pGogRadio   = nullptr;
        wxRadioButton* pCdRadio    = nullptr;
        wxComboBox* pSteamPath     = nullptr;
        wxTextCtrl* pGogPath       = nullptr;
        wxComboBox* pCdPath        = nullptr;
        wxCheckBox* pCopySaves     = nullptr;
        wxButton* pSteamBrowse     = nullptr;
        wxButton* pGogBrowse       = nullptr;
        wxButton* pCdBrowse        = nullptr;
    };

    DataInstallerDialog::DataInstallerDialog(const wxString& installDir_) :
        wxDialog(nullptr, wxID_ANY, "OpenJones3D - Install Game Data"),
        installDir(installDir_)
    {
        bInnoextract = IsProgramInPath("innoextract");

        wxBoxSizer* pSizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* pIntro = new wxStaticText(this, wxID_ANY,
            "OpenJones3D needs the data files of Indiana Jones and the Infernal Machine, which were not found.\n"
            "Choose where to install them from. The files will be copied to:");
        pSizer->Add(pIntro, wxSizerFlags().Border(wxBOTTOM));

        wxStaticText* pInstallDir = new wxStaticText(this, wxID_ANY, installDir);
        pInstallDir->SetFont(pInstallDir->GetFont().Bold());
        pSizer->Add(pInstallDir, wxSizerFlags().Border(wxLEFT | wxBOTTOM, FromDIP(16)));

        const int indent = FromDIP(24);

        // Steam
        pSteamRadio = new wxRadioButton(this, wxID_ANY, "&Steam (or another installed copy of the game)", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
        pSizer->Add(pSteamRadio, wxSizerFlags().Border(wxTOP));

        const wxArrayString steamInstalls = JonesWx::FindSteamInstalls();
        pSteamPath = new wxComboBox(this, wxID_ANY, steamInstalls.empty() ? wxString() : steamInstalls[0], wxDefaultPosition, FromDIP(wxSize(420, -1)), steamInstalls);
        pSteamPath->SetHint("Game folder containing the Resource folder");
        pSizer->Add(CreateSourceRow(pSteamPath, [this]() {
            wxDirDialog dialog(this, "Choose the game folder (it contains the Resource folder)", pSteamPath->GetValue(), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
            if ( dialog.ShowModal() == wxID_OK ) {
                pSteamPath->SetValue(dialog.GetPath());
            }
        }, &pSteamBrowse), wxSizerFlags().Expand().Border(wxLEFT, indent));

        pCopySaves = new wxCheckBox(this, wxID_ANY, "Also copy saved &games");
        pCopySaves->SetValue(true);
        pSizer->Add(pCopySaves, wxSizerFlags().Border(wxLEFT | wxTOP, indent));

        if ( steamInstalls.empty() )
        {
            wxStaticText* pNote = new wxStaticText(this, wxID_ANY, "No Steam installation of the game was found.");
            pNote->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
            pSizer->Add(pNote, wxSizerFlags().Border(wxLEFT | wxTOP, indent));
        }

        // GOG
        pGogRadio = new wxRadioButton(this, wxID_ANY, "&GOG offline installer (setup_indiana_jones_and_the_infernal_machine_*.exe)");
        pSizer->Add(pGogRadio, wxSizerFlags().Border(wxTOP, FromDIP(14)));

        const wxString gogInstaller = FindGogInstaller();
        pGogPath = new wxTextCtrl(this, wxID_ANY, gogInstaller, wxDefaultPosition, FromDIP(wxSize(420, -1)));
        pGogPath->SetHint("Path to the GOG installer .exe");
        pSizer->Add(CreateSourceRow(pGogPath, [this]() {
            wxFileDialog dialog(this, "Choose the GOG offline installer", wxFileName(pGogPath->GetValue()).GetPath(), wxString(),
                "GOG installer (setup_*.exe)|setup_*.exe;SETUP_*.EXE|All files|*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if ( dialog.ShowModal() == wxID_OK ) {
                pGogPath->SetValue(dialog.GetPath());
            }
        }, &pGogBrowse), wxSizerFlags().Expand().Border(wxLEFT, indent));

        if ( !bInnoextract )
        {
            wxStaticText* pNote = new wxStaticText(this, wxID_ANY, "Requires innoextract, which was not found. Install it with your package manager.");
            pNote->SetForegroundColour(*wxRED);
            pSizer->Add(pNote, wxSizerFlags().Border(wxLEFT | wxTOP, indent));
        }

        // CDs
        pCdRadio = new wxRadioButton(this, wxID_ANY, "Original &CD-ROMs");
        pSizer->Add(pCdRadio, wxSizerFlags().Border(wxTOP, FromDIP(14)));

        const wxArrayString discs = FindMountedDiscs(1);
        pCdPath = new wxComboBox(this, wxID_ANY, discs.empty() ? wxString() : discs[0], wxDefaultPosition, FromDIP(wxSize(420, -1)), discs);
        pCdPath->SetHint("Folder where disc 1 is mounted");
        pSizer->Add(CreateSourceRow(pCdPath, [this]() {
            wxDirDialog dialog(this, "Choose the folder where disc 1 is mounted", pCdPath->GetValue().empty() ? "/run/media" : pCdPath->GetValue(),
                wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
            if ( dialog.ShowModal() == wxID_OK ) {
                pCdPath->SetValue(dialog.GetPath());
            }
        }, &pCdBrowse), wxSizerFlags().Expand().Border(wxLEFT, indent));

        // Buttons
        wxBoxSizer* pButtons = new wxBoxSizer(wxHORIZONTAL);
        pButtons->AddStretchSpacer();
        wxButton* pInstall = new wxButton(this, wxID_ANY, "&Install");
        pInstall->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnInstall(); });
        pButtons->Add(pInstall, wxSizerFlags().Border(wxRIGHT));
        pButtons->Add(new wxButton(this, wxID_CANCEL, "&Quit"));
        pSizer->Add(pButtons, wxSizerFlags().Expand().Border(wxTOP, FromDIP(20)));

        // Preselect the source that was found
        if ( !steamInstalls.empty() ) {
            pSteamRadio->SetValue(true);
        }
        else if ( !gogInstaller.empty() && bInnoextract ) {
            pGogRadio->SetValue(true);
        }
        else if ( !discs.empty() ) {
            pCdRadio->SetValue(true);
        }
        else {
            pSteamRadio->SetValue(true);
        }

        for ( wxRadioButton* pRadio : { pSteamRadio, pGogRadio, pCdRadio } ) {
            pRadio->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { UpdateControls(); });
        }
        UpdateControls();

        wxBoxSizer* pTop = new wxBoxSizer(wxVERTICAL);
        pTop->Add(pSizer, wxSizerFlags(1).Expand().DoubleBorder());
        SetSizerAndFit(pTop);
        SetEscapeId(wxID_CANCEL);
        pInstall->SetDefault();
        Centre();
    }

    wxSizer* DataInstallerDialog::CreateSourceRow(wxWindow* pControl, std::function<void()> pfBrowse, wxButton** ppBrowse)
    {
        wxBoxSizer* pRow = new wxBoxSizer(wxHORIZONTAL);
        pRow->Add(pControl, wxSizerFlags(1).CenterVertical());

        wxButton* pBrowse = new wxButton(this, wxID_ANY, "Browse...");
        pBrowse->Bind(wxEVT_BUTTON, [pfBrowse](wxCommandEvent&) { pfBrowse(); });
        pRow->Add(pBrowse, wxSizerFlags().CenterVertical().Border(wxLEFT));

        *ppBrowse = pBrowse;
        return pRow;
    }

    void DataInstallerDialog::UpdateControls()
    {
        const bool bSteam = pSteamRadio->GetValue();
        pSteamPath->Enable(bSteam);
        pSteamBrowse->Enable(bSteam);
        pCopySaves->Enable(bSteam);

        pGogPath->Enable(pGogRadio->GetValue());
        pGogBrowse->Enable(pGogRadio->GetValue());

        pCdPath->Enable(pCdRadio->GetValue());
        pCdBrowse->Enable(pCdRadio->GetValue());
    }

    void DataInstallerDialog::OnInstall()
    {
        if ( !wxFileName::Mkdir(installDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) )
        {
            ShowError("Couldn't create folder " + installDir);
            return;
        }

        bool bInstalled = false;
        if ( pSteamRadio->GetValue() ) {
            bInstalled = InstallFromFolder();
        }
        else if ( pGogRadio->GetValue() ) {
            bInstalled = InstallFromGog();
        }
        else {
            bInstalled = InstallFromCDs();
        }

        if ( bInstalled ) {
            EndModal(wxID_OK);
        }
    }

    bool DataInstallerDialog::Finish(const std::vector<CopyItem>& items, const wxString& sourceDescription)
    {
        const wxArrayString missing = GetMissingRequiredFiles(items);
        if ( !missing.empty() )
        {
            ShowError(wxString::Format("Some game data files are still missing after installing from %s:\n\n%s",
                sourceDescription, wxJoin(missing, '\n', 0)));
            return false;
        }

        wxMessageBox("The game data was installed to " + installDir + ".", "Install Game Data", wxOK | wxICON_INFORMATION, this);
        return true;
    }

    bool DataInstallerDialog::InstallFromFolder()
    {
        const wxString root = pSteamPath->GetValue().Strip(wxString::both);
        if ( root.empty() || !wxDir::Exists(root) )
        {
            ShowError("Choose the folder of the installed game.");
            return false;
        }

        if ( CanonicalDir(root) == CanonicalDir(installDir) )
        {
            ShowError("The game is already installed to " + installDir + ", choose another folder.");
            return false;
        }

        std::vector<CopyItem> items;
        CollectDataFiles(root, items, pCopySaves->GetValue(), installDir);

        const wxArrayString missing = GetMissingRequiredFiles(items);
        if ( !missing.empty() )
        {
            ShowError(wxString::Format("The game data files were not found in %s:\n\n%s\n\nChoose the game folder, it contains the Resource folder.",
                root, wxJoin(missing, '\n', 0)));
            return false;
        }

        return InstallFiles(this, items, installDir, "Copying Game Data", /*bMove=*/false) && Finish(items, root);
    }

    bool DataInstallerDialog::InstallFromGog()
    {
        const wxString setupPath = pGogPath->GetValue().Strip(wxString::both);
        if ( !bInnoextract )
        {
            ShowError("Installing from the GOG installer requires innoextract (https://constexpr.org/innoextract/).\n\n"
                "Install it with your package manager, e.g. 'innoextract' on Arch, Debian, Fedora and Ubuntu.");
            return false;
        }

        if ( setupPath.empty() || !wxFileExists(setupPath) )
        {
            ShowError("Choose the GOG offline installer .exe file.");
            return false;
        }

        // Extracted to the install dir, so files are moved instead of copied
        const wxString extractDir = JoinPath(installDir, aGogExtractDir);
        struct ExtractDirCleanup
        {
            wxString dir;
            ~ExtractDirCleanup() { if ( wxDir::Exists(dir) ) wxFileName::Rmdir(dir, wxPATH_RMDIR_RECURSIVE); }
        } cleanup { extractDir };

        if ( wxDir::Exists(extractDir) ) {
            wxFileName::Rmdir(extractDir, wxPATH_RMDIR_RECURSIVE);
        }

        if ( !wxFileName::Mkdir(extractDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) )
        {
            ShowError("Couldn't create folder " + extractDir);
            return false;
        }

        // Extract the license agreement first, it's stored as a temporary setup file
        wxString errors;
        ProcessResult result;
        {
            wxProgressDialog progress("Reading GOG Installer", "Reading the license agreement...", 100, this, wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT);
            result = RunProcess({ "innoextract", "--silent", "--output-dir", extractDir, "--include", "/tmp/EULA_en-US.txt", "--include", "/tmp/EULA.txt", setupPath },
                [&progress](const std::string&) { return progress.Pulse(); }, &errors);
        }

        if ( result == PROCESS_CANCELED ) {
            return false;
        }

        if ( result != PROCESS_OK )
        {
            ShowError("Couldn't read the GOG installer " + setupPath + ".\n\n" + errors +
                "\n\nNewer GOG installers might require a newer version of innoextract.");
            return false;
        }

        wxString eulaPath = FindPath(extractDir, "tmp/EULA_en-US.txt");
        if ( eulaPath.empty() ) {
            eulaPath = FindPath(extractDir, "tmp/EULA.txt");
        }

        const wxString eula = ReadTextFile(eulaPath);
        if ( eula.empty() )
        {
            ShowError("The license agreement was not found in the GOG installer " + setupPath + ".");
            return false;
        }

        if ( !ShowLicenseAgreement(this, eula) ) {
            return false;
        }

        // Extract the game data. The extracted files take about the same space as the copied ones.
        if ( !CheckDiskSpace(this, installDir, wxULongLong(900ull * 1024 * 1024)) ) {
            return false;
        }

        {
            wxProgressDialog progress("Extracting Game Data", "Extracting the GOG installer...", 1000, this,
                wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME);
            result = RunProcess({ "innoextract", "--progress=1", "--color=0", "--exclude-temp", "--output-dir", extractDir,
                    "--include", "/Resource", "--include", "/KeySets", setupPath },
                [&progress](const std::string& output) {
                    const wxString file = ParseInnoextractFile(output);
                    return progress.Update((int)(ParseInnoextractProgress(output) * 10.0), file.empty() ? wxString("Extracting the GOG installer...") : "Extracting " + file);
                }, &errors);
        }

        if ( result == PROCESS_CANCELED ) {
            return false;
        }

        if ( result != PROCESS_OK )
        {
            ShowError("Extracting the GOG installer failed.\n\n" + errors);
            return false;
        }

        std::vector<CopyItem> items;
        CollectDataFiles(extractDir, items, /*bSaveGames=*/false, installDir);

        const wxArrayString missing = GetMissingRequiredFiles(items);
        if ( !missing.empty() )
        {
            ShowError("The game data files were not found in the GOG installer:\n\n" + wxJoin(missing, '\n', 0));
            return false;
        }

        return InstallFiles(this, items, installDir, "Installing Game Data", /*bMove=*/true) && Finish(items, setupPath);
    }

    bool DataInstallerDialog::InstallFromCDs()
    {
        wxString disc1 = pCdPath->GetValue().Strip(wxString::both);
        if ( disc1.empty() || !wxDir::Exists(disc1) )
        {
            ShowError("Insert disc 1 and choose the folder where it is mounted.");
            return false;
        }

        const wxString licensePath = FindPath(disc1, "install/license.txt");
        if ( licensePath.empty() || !HasResourceFile(disc1, "CD1.GOB") )
        {
            ShowError(disc1 + " doesn't contain disc 1 of Indiana Jones and the Infernal Machine.\n\nInsert disc 1 and choose the folder where it is mounted.");
            return false;
        }

        if ( !ShowLicenseAgreement(this, ReadTextFile(licensePath)) ) {
            return false;
        }

        std::vector<CopyItem> items;
        CollectDataFiles(disc1, items, /*bSaveGames=*/false, installDir);
        if ( !InstallFiles(this, items, installDir, "Copying Game Data from Disc 1", /*bMove=*/false) ) {
            return false;
        }

        // CD2.GOB is on the second disc
        if ( !GetMissingRequiredFiles(items).empty() )
        {
            wxString disc2;
            while ( disc2.empty() )
            {
                if ( wxMessageBox("Insert disc 2 of Indiana Jones and the Infernal Machine, then press OK.", "Insert Disc 2",
                    wxOK | wxCANCEL | wxICON_INFORMATION, this) != wxOK ) {
                    return false;
                }

                // Mount point usually changes with the disc label
                const wxArrayString discs = FindMountedDiscs(2);
                if ( !discs.empty() ) {
                    disc2 = discs[0];
                }
                else if ( IsDiscRoot(disc1, 2) ) {
                    disc2 = disc1;
                }
                else
                {
                    wxDirDialog dialog(this, "Disc 2 was not found. Choose the folder where disc 2 is mounted", wxFileName(disc1).GetPath(),
                        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
                    if ( dialog.ShowModal() == wxID_OK && IsDiscRoot(dialog.GetPath(), 2) ) {
                        disc2 = dialog.GetPath();
                    }
                }
            }

            const size_t numDisc1Files = items.size();
            CollectDataFiles(disc2, items, /*bSaveGames=*/false, installDir);

            const std::vector<CopyItem> disc2Items(items.begin() + numDisc1Files, items.end());
            if ( !InstallFiles(this, disc2Items, installDir, "Copying Game Data from Disc 2", /*bMove=*/false) ) {
                return false;
            }
        }

        return Finish(items, "the CDs");
    }
}

wxArrayString JonesWx::FindSteamInstalls()
{
    wxArrayString result;
#ifndef __WXMSW__
    const wxString home = wxGetHomeDir();
    const wxString aSteamRoots[] = {
        home + "/.local/share/Steam",
        home + "/.steam/steam",
        home + "/.steam/root",
        home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam", // Flatpak
        home + "/.var/app/com.valvesoftware.Steam/data/Steam",
        home + "/snap/steam/common/.local/share/Steam",                 // Snap
    };

    std::vector<wxString> libraries;
    for ( const wxString& root : aSteamRoots )
    {
        if ( !wxDir::Exists(root) ) {
            continue;
        }

        libraries.push_back(root);

        // Additional libraries are listed as: "path"   "/mnt/games/SteamLibrary"
        const wxString vdf = ReadTextFile(FindPath(root, "steamapps/libraryfolders.vdf"));
        for ( const wxString& line : wxSplit(vdf, '\n', 0) )
        {
            const wxString trimmed = wxString(line).Trim(false);
            if ( !trimmed.StartsWith("\"path\"") ) {
                continue;
            }

            wxString value = trimmed.Mid(6).Trim(false);
            if ( value.StartsWith("\"") && value.Len() > 1 )
            {
                value = value.Mid(1).BeforeLast('"');
                value.Replace("\\\\", "\\");
                libraries.push_back(value);
            }
        }
    }

    std::set<wxString> seen;
    for ( const wxString& library : libraries )
    {
        const wxString gameDir = FindPath(library, "steamapps/common/Indiana Jones and the Infernal Machine", /*bDir=*/true);
        if ( gameDir.empty() || !HasResourceFile(gameDir, "CD1.GOB") ) {
            continue;
        }

        const wxString canonical = CanonicalDir(gameDir);
        if ( seen.insert(canonical).second ) {
            result.push_back(canonical);
        }
    }
#endif
    return result;
}

extern "C" int JonesWx_ShowDataInstaller(const char* pInstallPath)
{
    if ( !JonesWx::EnsureInit() ) {
        return 0;
    }

    DataInstallerDialog dialog(wxString(pInstallPath, *wxConvFileName));
    return dialog.ShowModal() == wxID_OK;
}
