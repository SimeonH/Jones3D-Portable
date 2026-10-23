// Launcher of the installed game, a recreation of the original game launcher Jones3D.exe (LucasArts' CJonesLauncher)
// in its installed mode: a picture of the page above a column of buttons on the Launch, Options and Advanced Options pages.
// The launcher's buttons that ran Windows tools, uninstalled the game or opened the LucasArts web site are left out.
//
// Start New Game and Resume Play set the start mode the game starts with, like the original launcher did before it ran Indy3D.exe.
// The launcher is shown by JonesMain_Startup before the start mode is handled, so the sound and display settings
// dialogs of the game, which the original launcher opened by starting the game, are opened over the launcher.
//
// The page pictures, sounds and texts are the files of the original launcher in the Install folder of the install dir,
// copied from the CD by the data installer. The Steam version has the Launch picture and the sounds in its Jones3D.exe.
#include "JonesWx.h"
#include <Jones3D/Display/JonesHud.h>
#include <Jones3D/Gui/wx/JonesWxDialogs.h>
#include <Jones3D/Main/JonesMain.h>

#include <wx/dcbuffer.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/mstream.h>
#include <wx/textfile.h>

#ifdef __WXMSW__
#include <mmsystem.h>
#else
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_timer.h>
#endif

#include <map>
#include <string.h>
#include <vector>

namespace
{
    // Layout of the original launcher dialog, in pixels at 96 DPI
    constexpr int clientWidth    = 322;
    constexpr int clientHeight   = 376;
    constexpr int pictureHeight  = 242;
    constexpr int buttonsTop     = 245;
    constexpr int buttonX        = 1;
    constexpr int buttonWidth    = 320;
    constexpr int buttonHeight   = 26;

    constexpr const char* steamLauncherExe = "Jones3D.exe";

    enum Page
    {
        PAGE_LAUNCH,
        PAGE_OPTIONS,
        PAGE_ADVANCED
    };

    enum Action
    {
        ACTION_START_NEW_GAME,
        ACTION_RESUME_PLAY,
        ACTION_HELP,
        ACTION_OPTIONS,
        ACTION_EXIT,
        ACTION_ADVANCED,
        ACTION_BACK_TO_LAUNCH,
        ACTION_SOUND_OPTIONS,
        ACTION_DISPLAY_OPTIONS,
        ACTION_BACK_TO_OPTIONS
    };

    // Sounds of the original launcher: OPEN when it's shown, START before the game starts,
    // BACK for Exit and Go Back and CLICK for the other buttons
    enum Sound
    {
        SOUND_OPEN,
        SOUND_START,
        SOUND_BACK,
        SOUND_CLICK,
        NUM_SOUNDS
    };

    struct SoundInfo
    {
        const char* pFilename;
        uint32_t steamResourceID; // WAVE resource in the Steam version's Jones3D.exe
    };

    const SoundInfo aSounds[NUM_SOUNDS] = {
        { "open.wav",  JonesWx::steamSoundOpenID },
        { "start.wav", JonesWx::steamSoundStartID },
        { "back.wav",  JonesWx::steamSoundBackID },
        { "click.wav", JonesWx::steamSoundClickID },
    };

    struct ButtonInfo
    {
        const char* pTextID; // ID in setup.tab
        const char* pDefaultText;
        Action action;
    };

    struct PageInfo
    {
        const char* pTitleID;
        const char* pDefaultTitle;
        const char* pPicture; // Picture file in the Install folder
        std::vector<ButtonInfo> aButtons;
        size_t defaultButton;
        Action cancelAction; // Escape and the close box
    };

    const PageInfo aPages[] = {
        { "IDBAR_LAUNCHER_LAUNCH", "Launch", "launch.bmp", {
            { "IDBTN_LAUNCHER_START_NEW_GAME", "Start New Game", ACTION_START_NEW_GAME },
            { "IDBTN_LAUNCHER_RESUME_PLAY",    "Resume Play",    ACTION_RESUME_PLAY },
            { "IDBTN_LAUNCHER_HELP",           "Help",           ACTION_HELP },
            { "IDBTN_LAUNCHER_OPTIONS",        "Options",        ACTION_OPTIONS },
            { "IDBTN_LAUNCHER_EXIT",           "Exit",           ACTION_EXIT },
        }, 0, ACTION_EXIT },
        { "IDBAR_LAUNCHER_OPTIONS", "Options", "options.bmp", {
            { "IDBTN_LAUNCHER_ADVANCED",       "Advanced Options", ACTION_ADVANCED },
            { "IDBTN_LAUNCHER_BACK",           "Go Back",          ACTION_BACK_TO_LAUNCH },
        }, 0, ACTION_BACK_TO_LAUNCH },
        { "IDBAR_LAUNCHER_ADVANCED", "Advanced Options", "system.bmp", {
            { "IDBTN_LAUNCHER_AUDIO",          "Sound Options",   ACTION_SOUND_OPTIONS },
            { "IDBTN_LAUNCHER_VIDEO",          "Display Options", ACTION_DISPLAY_OPTIONS },
            { "IDBTN_LAUNCHER_BACK",           "Go Back",         ACTION_BACK_TO_OPTIONS },
        }, 0, ACTION_BACK_TO_OPTIONS },
    };

    // Returns the entry of dir named name (case-insensitive like the original Windows file system), empty if not found
    wxString FindEntry(const wxString& dir, const wxString& name)
    {
        wxDir d;
        if ( !wxDir::Exists(dir) || !d.Open(dir) ) {
            return wxString();
        }

        wxString entry;
        for ( bool bCont = d.GetFirst(&entry); bCont; bCont = d.GetNext(&entry) )
        {
            if ( entry.CmpNoCase(name) == 0 ) {
                return wxFileName(dir, entry).GetFullPath();
            }
        }

        return wxString();
    }

    // Finds relPath ('/' separated) under root, case-insensitive
    wxString FindPath(const wxString& root, const wxString& relPath)
    {
        wxString path = root;
        for ( const wxString& part : wxSplit(relPath, '/', 0) )
        {
            path = FindEntry(path, part);
            if ( path.empty() ) {
                break;
            }
        }

        return path;
    }

    std::vector<uint8_t> ReadFile(const wxString& path)
    {
        wxFile file;
        std::vector<uint8_t> data;
        if ( !path.empty() && file.Open(path) )
        {
            data.resize((size_t)file.Length());
            if ( file.Read(data.data(), data.size()) != (ssize_t)data.size() ) {
                data.clear();
            }
        }

        return data;
    }

    void SoundPlay(const std::vector<uint8_t>& wav, bool bWait);

    class LauncherDialog : public wxDialog
    {
    public:
        explicit LauncherDialog(const wxString& installDir);
        ~LauncherDialog();

        int GetResult() const { return result; }
        int GetStartMode() const { return startMode; }

    private:
        void LoadTexts();
        wxString Text(const char* pID, const char* pDefault) const;
        wxImage LoadPicture(const char* pFilename) const;
        std::vector<uint8_t> LoadSound(const SoundInfo& info) const;
        wxString FindSteamLauncher() const;

        void ShowPage(Page page);
        void PlaySound(Sound sound, bool bWait);
        void OnAction(Action action);
        void StartGame(int mode);
        void ShowSettings(int (J3DAPI* pfShowDialog)(HWND));

        void OnPaintPicture(wxPaintEvent& event);
        void OnCharHook(wxKeyEvent& event);
        void OnClose(wxCloseEvent& event);

        wxString installDir;
        std::map<std::string, wxString> texts;

        wxPanel* pPicturePanel = nullptr;
        wxPanel* pButtonPanel  = nullptr;
        wxBitmap picture;
        wxImage launchPicture;
        Page curPage = PAGE_LAUNCH;

        std::vector<uint8_t> aSoundData[NUM_SOUNDS];

        int result    = JONESLAUNCHER_EXIT;
        int startMode = JONES_STARTMODE_STARTGAME;
    };

    LauncherDialog::LauncherDialog(const wxString& installDir_) :
        wxDialog(nullptr, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX),
        installDir(installDir_)
    {
        LoadTexts();
        for ( size_t i = 0; i < NUM_SOUNDS; ++i ) {
            aSoundData[i] = LoadSound(aSounds[i]);
        }

        launchPicture = LoadPicture("launch.bmp");

        SetClientSize(FromDIP(wxSize(clientWidth, clientHeight)));

        pPicturePanel = new wxPanel(this, wxID_ANY, wxPoint(0, 0), FromDIP(wxSize(clientWidth, pictureHeight)));
        pPicturePanel->SetBackgroundStyle(wxBG_STYLE_PAINT);
        pPicturePanel->Bind(wxEVT_PAINT, &LauncherDialog::OnPaintPicture, this);

        Bind(wxEVT_INIT_DIALOG, [this](wxInitDialogEvent& event) { PlaySound(SOUND_OPEN, /*bWait=*/false); event.Skip(); });
        Bind(wxEVT_CHAR_HOOK, &LauncherDialog::OnCharHook, this);
        Bind(wxEVT_CLOSE_WINDOW, &LauncherDialog::OnClose, this);

        ShowPage(PAGE_LAUNCH);
        CentreOnScreen();
    }

    LauncherDialog::~LauncherDialog()
    {
#ifdef __WXMSW__
        ::PlaySoundA(nullptr, nullptr, 0); // Async sounds play from aSoundData
#endif
    }

    // Texts of setup.tab ("ID<tab>number<tab>text" lines), the built-in texts are the ones of the US version
    void LauncherDialog::LoadTexts()
    {
        const std::vector<uint8_t> data = ReadFile(FindPath(installDir, "Install/setup.tab"));
        const wxString text(reinterpret_cast<const char*>(data.data()), wxCSConv(wxFONTENCODING_CP1252), data.size());
        for ( wxString line : wxSplit(text, '\n', 0) )
        {
            line.Trim();
            const wxArrayString fields = wxSplit(line, '\t', 0);
            if ( fields.size() >= 3 )
            {
                wxString value = fields[2];
                value.Replace("\\n", "\n");
                texts[fields[0].ToStdString()] = value;
            }
        }
    }

    wxString LauncherDialog::Text(const char* pID, const char* pDefault) const
    {
        auto it = texts.find(pID);
        return it != texts.end() && !it->second.empty() ? it->second : wxString(pDefault);
    }

    wxString LauncherDialog::FindSteamLauncher() const
    {
        for ( const wxString& dir : JonesWx::FindSteamInstalls() )
        {
            const wxString exe = FindEntry(dir, steamLauncherExe);
            if ( !exe.empty() ) {
                return exe;
            }
        }

        return wxString();
    }

    wxImage LauncherDialog::LoadPicture(const char* pFilename) const
    {
        const wxString path = FindPath(installDir, wxString("Install/") + pFilename);
        if ( !path.empty() )
        {
            wxImage image;
            wxLogNull noLog;
            if ( image.LoadFile(path, wxBITMAP_TYPE_BMP) ) {
                return image;
            }
        }

        if ( strcmp(pFilename, "launch.bmp") != 0 ) {
            return wxImage(); // Other pages show the launch picture
        }

        const wxString steamExe = FindSteamLauncher();
        const std::vector<uint8_t> bmp = steamExe.empty() ? std::vector<uint8_t>() : JonesWx::ReadExeBitmapFile(steamExe, JonesWx::steamLaunchPictureID);
        if ( bmp.empty() ) {
            return wxImage();
        }

        wxMemoryInputStream stream(bmp.data(), bmp.size());
        wxLogNull noLog;
        return wxImage(stream, wxBITMAP_TYPE_BMP);
    }

    std::vector<uint8_t> LauncherDialog::LoadSound(const SoundInfo& info) const
    {
        std::vector<uint8_t> data = ReadFile(FindPath(installDir, wxString("Install/") + info.pFilename));
        if ( data.empty() )
        {
            const wxString steamExe = FindSteamLauncher();
            if ( !steamExe.empty() ) {
                data = JonesWx::ReadExeResource(steamExe, 0, info.steamResourceID, "WAVE");
            }
        }

        return data;
    }

    void LauncherDialog::ShowPage(Page page)
    {
        curPage = page;
        const PageInfo& info = aPages[page];
        SetTitle(Text(info.pTitleID, info.pDefaultTitle));

        wxImage image = LoadPicture(info.pPicture);
        if ( !image.IsOk() ) {
            image = launchPicture;
        }

        picture = image.IsOk() ? wxBitmap(image) : wxBitmap();
        pPicturePanel->Refresh();

        // Buttons are in a panel below the picture, like the child dialog of the original
        if ( pButtonPanel ) {
            pButtonPanel->Destroy();
        }

        const wxPoint panelPos = FromDIP(wxPoint(0, buttonsTop));
        pButtonPanel = new wxPanel(this, wxID_ANY, panelPos, GetClientSize() - wxSize(0, panelPos.y));

        wxButton* pDefault = nullptr;
        for ( size_t i = 0; i < info.aButtons.size(); ++i )
        {
            const ButtonInfo& button = info.aButtons[i];
            wxButton* pButton = new wxButton(pButtonPanel, wxID_ANY, Text(button.pTextID, button.pDefaultText),
                FromDIP(wxPoint(buttonX, (int)i * buttonHeight)), FromDIP(wxSize(buttonWidth, buttonHeight)));
            const Action action = button.action;
            pButton->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent&) { OnAction(action); });

            if ( i == info.defaultButton ) {
                pDefault = pButton;
            }
        }

        if ( pDefault )
        {
            pDefault->SetDefault();
            pDefault->SetFocus();
        }
    }

    void LauncherDialog::PlaySound(Sound sound, bool bWait)
    {
        SoundPlay(aSoundData[sound], bWait);
    }

    void LauncherDialog::StartGame(int mode)
    {
        PlaySound(SOUND_START, /*bWait=*/true);
        startMode = mode;
        result    = JONESLAUNCHER_START;
        EndModal(wxID_OK);
    }

    void LauncherDialog::OnAction(Action action)
    {
        switch ( action )
        {
            case ACTION_START_NEW_GAME:
                StartGame(JONES_STARTMODE_STARTGAME);
                break;

            case ACTION_RESUME_PLAY:
                StartGame(JONES_STARTMODE_LOADGAME);
                break;

            case ACTION_SOUND_OPTIONS:
                PlaySound(SOUND_CLICK, /*bWait=*/false);
                ShowSettings(JonesMain_ShowLauncherSoundSettings);
                break;

            case ACTION_DISPLAY_OPTIONS:
                PlaySound(SOUND_CLICK, /*bWait=*/false);
                ShowSettings(JonesMain_ShowLauncherDisplaySettings);
                break;

            case ACTION_HELP:
                PlaySound(SOUND_CLICK, /*bWait=*/false);
                JonesHud_OpenHelp(nullptr);
                break;

            case ACTION_OPTIONS:
                PlaySound(SOUND_CLICK, /*bWait=*/false);
                ShowPage(PAGE_OPTIONS);
                break;

            case ACTION_ADVANCED:
                PlaySound(SOUND_CLICK, /*bWait=*/false);
                ShowPage(PAGE_ADVANCED);
                break;

            case ACTION_BACK_TO_LAUNCH:
                PlaySound(SOUND_BACK, /*bWait=*/false);
                ShowPage(PAGE_LAUNCH);
                break;

            case ACTION_BACK_TO_OPTIONS:
                PlaySound(SOUND_BACK, /*bWait=*/false);
                ShowPage(PAGE_OPTIONS);
                break;

            case ACTION_EXIT:
                PlaySound(SOUND_BACK, /*bWait=*/true);
                result = JONESLAUNCHER_EXIT;
                EndModal(wxID_CANCEL);
                break;

        }
    }

    // Opens a settings dialog of the game over the launcher, the launcher is active again when it's closed
    void LauncherDialog::ShowSettings(int (J3DAPI* pfShowDialog)(HWND))
    {
#ifdef __WXMSW__
        pfShowDialog(GetHWND());
#else
        pfShowDialog(reinterpret_cast<HWND>(this)); // See JonesWx::GetParent
#endif

        Raise();
        if ( wxWindow* pDefault = wxDynamicCast(GetDefaultItem(), wxWindow) ) {
            pDefault->SetFocus();
        }
    }

    void LauncherDialog::OnPaintPicture(wxPaintEvent&)
    {
        // Disabled owner-drawn button with the picture, as in the original
        wxAutoBufferedPaintDC dc(pPicturePanel);
        const wxSize size = pPicturePanel->GetClientSize();
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();

        dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW)));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(0, 0, size.x, size.y);

        if ( picture.IsOk() )
        {
            const wxSize inner = size - wxSize(2, 2);
            if ( picture.GetSize() == inner ) {
                dc.DrawBitmap(picture, 1, 1);
            }
            else {
                dc.DrawBitmap(wxBitmap(picture.ConvertToImage().Scale(inner.x, inner.y, wxIMAGE_QUALITY_HIGH)), 1, 1);
            }
        }
    }

    void LauncherDialog::OnCharHook(wxKeyEvent& event)
    {
        if ( event.GetKeyCode() == WXK_ESCAPE ) {
            OnAction(aPages[curPage].cancelAction);
        }
        else {
            event.Skip();
        }
    }

    void LauncherDialog::OnClose(wxCloseEvent&)
    {
        result = JONESLAUNCHER_EXIT;
        EndModal(wxID_CANCEL);
    }

#ifdef __WXMSW__
    void SoundPlay(const std::vector<uint8_t>& wav, bool bWait)
    {
        if ( !wav.empty() ) {
            ::PlaySoundA(reinterpret_cast<LPCSTR>(wav.data()), nullptr, SND_MEMORY | SND_NODEFAULT | (bWait ? SND_SYNC : SND_ASYNC));
        }
    }

    void SoundShutdown() {}
#else
    bool bSoundInit                = false;
    SDL_AudioStream* pSoundStream  = nullptr;

    void SoundShutdown()
    {
        if ( pSoundStream )
        {
            SDL_DestroyAudioStream(pSoundStream);
            pSoundStream = nullptr;
        }

        if ( bSoundInit )
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            bSoundInit = false;
        }
    }

    // Plays a .wav file like PlaySound, a new sound stops the playing one.
    // The stream stays open while the launcher is shown, a newly opened output (or one PipeWire suspended when idle)
    // takes a while before it's audible, which would swallow the short game start sound.
    void SoundPlay(const std::vector<uint8_t>& wav, bool bWait)
    {
        if ( wav.empty() ) {
            return;
        }

        if ( !bSoundInit && !(bSoundInit = SDL_InitSubSystem(SDL_INIT_AUDIO)) ) {
            return;
        }

        SDL_AudioSpec spec;
        Uint8* pBuffer = nullptr;
        Uint32 length  = 0;
        if ( !SDL_LoadWAV_IO(SDL_IOFromConstMem(wav.data(), wav.size()), true, &spec, &pBuffer, &length) ) {
            return;
        }

        if ( !pSoundStream ) {
            pSoundStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        }
        else
        {
            SDL_ClearAudioStream(pSoundStream);
            SDL_SetAudioStreamFormat(pSoundStream, &spec, nullptr);
        }

        if ( pSoundStream )
        {
            SDL_PutAudioStreamData(pSoundStream, pBuffer, (int)length);
            SDL_FlushAudioStream(pSoundStream);
            SDL_ResumeAudioStreamDevice(pSoundStream);

            if ( bWait )
            {
                // Like PlaySound SND_SYNC: return when the sound was heard, i.e. after its duration plus the output latency
                const Uint64 durationMs = (Uint64)length * 1000 / ((Uint64)SDL_AUDIO_FRAMESIZE(spec) * (Uint64)spec.freq);
                const Uint64 endTime    = SDL_GetTicks() + durationMs + 250;
                while ( SDL_GetTicks() < endTime ) {
                    SDL_Delay(10);
                }
            }
        }

        SDL_free(pBuffer);
    }
#endif
}

extern "C" int JonesWx_ShowLauncher(const char* pInstallPath, int* pStartMode)
{

    if ( !JonesWx::EnsureInit() ) {
        return JONESLAUNCHER_START; // Start as without the launcher
    }

    int result;
    {
        LauncherDialog dialog(JonesWx::NativePath(pInstallPath && pInstallPath[0] ? pInstallPath : "."));
        dialog.ShowModal();
        result      = dialog.GetResult();
        *pStartMode = dialog.GetStartMode();
    }

    SoundShutdown();
    return result;
}
