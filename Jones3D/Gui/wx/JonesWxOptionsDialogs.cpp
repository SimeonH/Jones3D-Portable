// Options dialogs: game play (112), sound (113), display (114) and advanced display (148) settings
#include "JonesWx.h"
#include "JonesWxDialogs.h"

#include <Jones3D/Display/jonesConfig.h>
#include <Jones3D/Main/JonesMain.h>
#include <Jones3D/Main/jonesString.h>
#include <Jones3D/Play/jonesInventory.h>

#include <sith/Devices/sithControl.h>
#include <sith/Devices/sithSoundMixer.h>
#include <sith/Engine/sithRender.h>
#include <sith/Gameplay/sithOverlayMap.h>
#include <sith/Main/sithMain.h>
#include <sith/World/sithModel.h>
#include <sith/World/sithVoice.h>
#include <sith/World/sithWorld.h>

#include <std/Win95/std3D.h>
#include <sound/Sound.h>
#include <w32util/wuRegistry.h>

#include <wx/slider.h>
#include <wx/statbox.h>

#include <stdarg.h>
#include <stdio.h>
#include <vector>

namespace
{
    ///////////////////////////////////////////////////////////////////////////////
    // Game play options

    class GamePlayOptionsDialog : public JonesWxDialog
    {
    public:
        explicit GamePlayOptionsDialog(HWND hwndParent) :
            JonesWxDialog(hwndParent, "JONES_STR_GMPLY_OPTS")
        {
            // Difficulty
            wxStaticBoxSizer* pDifficultyBox = new wxStaticBoxSizer(wxVERTICAL, this, JonesWx::Str("JONES_STR_DFCLTY"));
            wxStaticBox* pBox = pDifficultyBox->GetStaticBox();

            pDifficultySlider = new wxSlider(pBox, wxID_ANY, sithGetGameDifficulty(), 0, (int)jonesInventory_GetMaxDifficultyLevel(), wxDefaultPosition, wxSize(Scaled(240), -1), wxSL_HORIZONTAL | wxSL_AUTOTICKS);
            pDifficultySlider->SetTickFreq(1);
            pDifficultySlider->SetPageSize(1);
            pDifficultySlider->Bind(wxEVT_SLIDER, [this](wxCommandEvent&) { UpdateDifficultyText(); });

            pDifficultyText = new wxStaticText(pBox, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
            UpdateDifficultyText();

            pDifficultyBox->Add(pDifficultySlider, wxSizerFlags().Expand());
            pDifficultyBox->Add(pDifficultyText, wxSizerFlags().Expand().Border(wxTOP, Scaled(4)));

            // Options
            pShowHintsCB  = new wxCheckBox(this, wxID_ANY, JonesWx::Str("JONES_STR_HINTS"));
            pRotateMapCB  = new wxCheckBox(this, wxID_ANY, JonesWx::Str("JONES_STR_MAPTURN"));
            pDefaultRunCB = new wxCheckBox(this, wxID_ANY, JonesWx::Str("JONES_STR_RUNWALK"));
            pShowTextCB   = new wxCheckBox(this, wxID_ANY, JonesWx::Str("JONES_STR_TEXT"));
            pHiPolyCB     = new wxCheckBox(this, wxID_ANY, JonesWx::StrOr("JONES_STR_HIPOLY", "High Poly Objects")); // Added

            pShowHintsCB->SetValue(sithOverlayMap_GetShowHints() != 0);
            pRotateMapCB->SetValue(sithOverlayMap_GetMapRotation() != 0);
            pDefaultRunCB->SetValue(jonesConfig_GetDefaultRun() != 0);
            pShowTextCB->SetValue(sithVoice_GetShowText() != 0);
            pHiPolyCB->SetValue(sithModel_IsHiPolyEnabled() != 0);

            wxBoxSizer* pOptions = new wxBoxSizer(wxVERTICAL);
            for ( wxCheckBox* pCB : { pShowHintsCB, pRotateMapCB, pDefaultRunCB, pShowTextCB, pHiPolyCB } ) {
                pOptions->Add(pCB, wxSizerFlags().Border(wxTOP | wxBOTTOM, Scaled(2)));
            }

            wxBoxSizer* pOptionsRow = new wxBoxSizer(wxHORIZONTAL);
            pOptionsRow->Add(CreateResourceIcon(this, 137, 32), wxSizerFlags().Top().Border(wxRIGHT, Scaled(16)));
            pOptionsRow->Add(pOptions, wxSizerFlags(1));

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pDifficultyBox, wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(8));
            pContent->Add(pOptionsRow, wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ CreateButton(this, 1, "JONES_STR_OK", /*bDefault=*/true), CreateButton(this, 2, "JONES_STR_CANCEL") }), wxSizerFlags().Expand());
            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            if ( resultID == 1 )
            {
                sithVoice_ShowText(pShowTextCB->GetValue());
                wuRegistry_SaveIntEx("Show Text", sithVoice_GetShowText());

                sithOverlayMap_EnableMapRotation(pRotateMapCB->GetValue());
                wuRegistry_SaveIntEx("Map Rotation", sithOverlayMap_GetMapRotation());

                sithOverlayMap_SetShowHints(pShowHintsCB->GetValue());
                wuRegistry_SaveIntEx("Show Hints", sithOverlayMap_GetShowHints());

                sithSetGameDifficulty(pDifficultySlider->GetValue());
                wuRegistry_SaveInt("Difficulty", sithGetGameDifficulty());

                jonesConfig_SetDefaultRun(pDefaultRunCB->GetValue());

                // Added
                const int bCurHiPoly = sithModel_IsHiPolyEnabled();
                const int bHiPoly    = pHiPolyCB->GetValue();
                sithModel_EnableHiPoly(bHiPoly);
                wuRegistry_SaveIntEx("HiPoly", bHiPoly);

                if ( (bHiPoly != 0) != (bCurHiPoly != 0) )
                {
                    const std::string note = JonesWx::ToGame(JonesWx::StrOr("JONES_STR_HIPOLYGO", "You must quit and restart the game for the high poly option to take effect."));
                    jonesConfig_ShowMessageDialog((HWND)GetHandle(), "JONES_STR_GMPLY_OPTS", note.c_str(), 137);
                }
            }

            EndModal(resultID);
        }

    private:
        void UpdateDifficultyText()
        {
            char aKey[32];
            snprintf(aKey, sizeof(aKey), "JONES_STR_HARD%d", pDifficultySlider->GetValue());
            pDifficultyText->SetLabel(JonesWx::Str(aKey));
        }

        wxSlider* pDifficultySlider;
        wxStaticText* pDifficultyText;
        wxCheckBox* pShowHintsCB;
        wxCheckBox* pRotateMapCB;
        wxCheckBox* pDefaultRunCB;
        wxCheckBox* pShowTextCB;
        wxCheckBox* pHiPolyCB;
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Sound settings

    class SoundSettingsDialog : public JonesWxDialog
    {
    public:
        SoundSettingsDialog(HWND hwndParent, JonesSoundSettings* pSettings) :
            JonesWxDialog(hwndParent, "JONES_STR_SND_OPTS"),
            pSettings(pSettings)
        {
            wxStaticBoxSizer* pVolumeBox = new wxStaticBoxSizer(wxVERTICAL, this, JonesWx::Str("JONES_STR_VOLUME"));
            wxStaticBox* pBox = pVolumeBox->GetStaticBox();

            pVolumeSlider = new wxSlider(pBox, wxID_ANY, (int)(pSettings->maxSoundVolume * 100.0f), 0, 100, wxDefaultPosition, wxSize(Scaled(240), -1), wxSL_HORIZONTAL | wxSL_AUTOTICKS);
            pVolumeSlider->SetTickFreq(17);
            pVolumeSlider->SetPageSize(5);
            pVolumeSlider->Bind(wxEVT_SCROLL_CHANGED, [this](wxScrollEvent&) { PlayTestSound(); });

            wxBoxSizer* pLabels = new wxBoxSizer(wxHORIZONTAL);
            pLabels->Add(CreateLabel(pBox, "JONES_STR_LOW"), wxSizerFlags(1));
            pLabels->Add(CreateLabel(pBox, "JONES_STR_MEDIUM", wxALIGN_CENTRE_HORIZONTAL), wxSizerFlags(1));
            pLabels->Add(CreateLabel(pBox, "JONES_STR_HIGH", wxALIGN_RIGHT), wxSizerFlags(1));

            pVolumeBox->Add(pVolumeSlider, wxSizerFlags().Expand());
            pVolumeBox->Add(pLabels, wxSizerFlags().Expand());

            // Note, original dialog has also hidden reverse sound check box (JONES_STR_REVERSE), the setting is preserved as is
            p3DSoundCB = new wxCheckBox(this, wxID_ANY, JonesWx::Str("JONES_STR_3D_SND"));
            if ( Sound_Has3DHW() ) {
                p3DSoundCB->SetValue(pSettings->b3DHWSupport != 0);
            }
            else {
                p3DSoundCB->Disable();
            }

            wxBoxSizer* pSettingsCol = new wxBoxSizer(wxVERTICAL);
            pSettingsCol->Add(pVolumeBox, wxSizerFlags().Expand());
            pSettingsCol->AddSpacer(Scaled(8));
            pSettingsCol->Add(p3DSoundCB);

            wxBoxSizer* pRow = new wxBoxSizer(wxHORIZONTAL);
            pRow->Add(CreateResourceIcon(this, 142, 32), wxSizerFlags().Top().Border(wxRIGHT, Scaled(16)));
            pRow->Add(pSettingsCol, wxSizerFlags(1).Expand());

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pRow, wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ CreateButton(this, 1, "JONES_STR_OK", /*bDefault=*/true), CreateButton(this, 2, "JONES_STR_CANCEL") }), wxSizerFlags().Expand());
            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            if ( resultID == 1 )
            {
                pSettings->maxSoundVolume = (float)pVolumeSlider->GetValue() / 100.0f;
                wuRegistry_SaveFloat("Sound Volume", pSettings->maxSoundVolume);

                pSettings->b3DHWSupport = p3DSoundCB->GetValue();
                wuRegistry_SaveIntEx("Sound 3D", pSettings->b3DHWSupport);

                wuRegistry_SaveIntEx("ReverseSound", pSettings->bReverseSound);
            }

            EndModal(resultID);
        }

    private:
        void PlayTestSound()
        {
            // Play test sound with new volume
            tSoundHandle hSnd = Sound_GetSoundHandle(SITHWORLD_STATICINDEX(57)); // 57 - mus_find.wav
            if ( hSndChannel && (Sound_GetChannelFlags(hSndChannel) & SOUND_CHANNEL_PLAYING) != 0 ) {
                sithSoundMixer_StopSound(hSndChannel);
            }

            hSndChannel = 0;
            if ( !hSnd ) {
                return;
            }

            const int curPos = pVolumeSlider->GetValue();
            if ( curSoundVolume == curPos ) {
                return;
            }

            hSndChannel    = sithSoundMixer_PlaySound(hSnd, (float)curPos / 100.0f, 0.0f, (SoundPlayFlag)0);
            curSoundVolume = curPos;
        }

        JonesSoundSettings* pSettings;
        wxSlider* pVolumeSlider;
        wxCheckBox* p3DSoundCB;

        static tSoundChannelHandle hSndChannel;
        static int curSoundVolume;
    };

    tSoundChannelHandle SoundSettingsDialog::hSndChannel = 0;
    int SoundSettingsDialog::curSoundVolume              = 0;

    ///////////////////////////////////////////////////////////////////////////////
    // Display settings

    constexpr int colorDepth16bpp = 1 << 2;

    struct VideoMode
    {
        std::string resolutionText;
        int colorDepthFlags            = 0;
        int aColorDepthVideoModes[5]   = { 0 }; // Video mode index for color depth flag bit
    };

    struct ColorDepthItem
    {
        int colorDepthIdx;
        int colorDepthMask;
    };

    std::string FormatGameStr(const char* pKey, ...)
    {
        const char* pFormat = jonesString_GetString(pKey);
        if ( !pFormat ) {
            return std::string();
        }

        char aText[256] = { 0 };
        va_list args;
        va_start(args, pKey);
        vsnprintf(aText, sizeof(aText) - 1, pFormat, args);
        va_end(args);
        return aText;
    }

    class DisplaySettingsDialog;
    int ShowAdvancedDisplaySettings(DisplaySettingsDialog* pDisplayDlg);

    class DisplaySettingsDialog : public JonesWxDialog
    {
    public:
        DisplaySettingsDialog(HWND hwndParent, StdDisplayEnvironment* pDisplayEnv, JonesDisplaySettings* pSettings) :
            JonesWxDialog(hwndParent, "JONES_STR_DPLY_OPTS"),
            pDisplayEnv(pDisplayEnv),
            pSettings(pSettings)
        {
            wxStaticBoxSizer* pSettingsBox = new wxStaticBoxSizer(wxVERTICAL, this, JonesWx::Str("JONES_STR_DSPLY_SET"));
            wxStaticBox* pBox = pSettingsBox->GetStaticBox();

            // Resolution
            wxStaticBoxSizer* pResBox = new wxStaticBoxSizer(wxVERTICAL, pBox, JonesWx::Str("JONES_STR_GMRES"));
            pResSlider = new wxSlider(pResBox->GetStaticBox(), wxID_ANY, 0, 0, 1, wxDefaultPosition, wxSize(Scaled(220), -1), wxSL_HORIZONTAL | wxSL_AUTOTICKS);
            pResSlider->SetPageSize(1);
            pResSlider->Bind(wxEVT_SLIDER, [this](wxCommandEvent&) { UpdateResolutionText(); });
            pResText = new wxStaticText(pResBox->GetStaticBox(), wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
            pResBox->Add(pResSlider, wxSizerFlags().Expand());
            pResBox->Add(pResText, wxSizerFlags().Expand().Border(wxTOP, Scaled(4)));

            // Color depth
            wxStaticBoxSizer* pDepthBox = new wxStaticBoxSizer(wxVERTICAL, pBox, JonesWx::Str("JONES_STR_CLRDPTH"));
            pColorDepthCombo = new wxChoice(pDepthBox->GetStaticBox(), wxID_ANY);
            pColorDepthCombo->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { UpdateResolutions(GetSelectedColorDepth().colorDepthMask, GetResolutionText()); });
            pDepthBox->Add(pColorDepthCombo, wxSizerFlags().Expand());

            // Default and advanced buttons
            wxBoxSizer* pButtonsCol = new wxBoxSizer(wxVERTICAL);
            pButtonsCol->Add(CreateButton(pBox, 1099, "JONES_STR_DEFAULTSET"), wxSizerFlags().Expand());
            pButtonsCol->AddSpacer(Scaled(6));
            pButtonsCol->Add(CreateButton(pBox, 1093, "JONES_STR_ADV"), wxSizerFlags().Expand());

            wxBoxSizer* pBottomRow = new wxBoxSizer(wxHORIZONTAL);
            pBottomRow->Add(CreateResourceIcon(pBox, 136, 32), wxSizerFlags().CenterVertical());
            pBottomRow->AddStretchSpacer();
            pBottomRow->Add(pButtonsCol);

            pSettingsBox->Add(pResBox, wxSizerFlags().Expand());
            pSettingsBox->AddSpacer(Scaled(6));
            pSettingsBox->Add(pDepthBox, wxSizerFlags().Expand());
            pSettingsBox->AddSpacer(Scaled(8));
            pSettingsBox->Add(pBottomRow, wxSizerFlags().Expand());

            pOkBtn = CreateButton(this, 1, "JONES_STR_OK", /*bDefault=*/true);

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pSettingsBox, wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ pOkBtn, CreateButton(this, 2, "JONES_STR_CANCEL") }), wxSizerFlags().Expand());

            InitVideoModes();
            FinishLayout(pContent);
        }

        // Rebuilds video mode list for the current display & 3D device
        void InitVideoModes()
        {
            const std::string curResText = GetResolutionText();

            aVideoModes.clear();
            aColorDepths.clear();
            pColorDepthCombo->Clear();

            StdDisplayInfo* pDisplayInfo = &pDisplayEnv->aDisplayInfos[pSettings->displayDeviceNum];
            pOkBtn->Enable(pDisplayInfo->displayDevice.bHAL != 0);
            pColorDepthCombo->Enable(pDisplayInfo->displayDevice.bHAL != 0);

            const StdVideoMode curVideoMode = pDisplayInfo->aModes[pSettings->videoModeNum];
            bool bHasCurVideoMode  = false;
            size_t selVideoModeNum = 0;
            int selColorDepthMask  = 0;

            for ( size_t i = 0; i < pDisplayInfo->numModes; ++i )
            {
                const StdVideoMode* pMode = &pDisplayInfo->aModes[i];
                if ( pMode->aspectRatio != 1.0f || pMode->rasterInfo.width < 512 || pMode->rasterInfo.height < 384 ) {
                    continue;
                }

                if ( !jonesConfig_DisplaySettings_Get3DDeviceSupportsBPP(pDisplayInfo, pSettings, pMode->rasterInfo.colorInfo.bpp) ) {
                    continue;
                }

                const std::string resText = FormatGameStr("JONES_STR_BYPIXELS", pMode->rasterInfo.width, pMode->rasterInfo.height);

                size_t videoModeNum = 0;
                while ( videoModeNum < aVideoModes.size() && aVideoModes[videoModeNum].resolutionText != resText ) {
                    ++videoModeNum;
                }

                if ( videoModeNum == aVideoModes.size() )
                {
                    aVideoModes.emplace_back();
                    aVideoModes.back().resolutionText = resText;
                }

                int colorDepthIdx = 0;
                std::string depthText;
                switch ( pMode->rasterInfo.colorInfo.bpp )
                {
                    case 16:
                        colorDepthIdx = 2;
                        depthText = FormatGameStr("JONES_STR_16BBP");
                        break;

                    case 24:
                        colorDepthIdx = 3;
                        depthText = FormatGameStr("JONES_STR_24BBP");
                        break;

                    case 32:
                        colorDepthIdx = 4;
                        depthText = FormatGameStr("JONES_STR_32BBP");
                        break;
                }

                const int colorDepthMask = colorDepthIdx ? 1 << colorDepthIdx : 0;
                aVideoModes[videoModeNum].colorDepthFlags |= colorDepthMask;
                aVideoModes[videoModeNum].aColorDepthVideoModes[colorDepthIdx] = (int)i;

                int comboIdx = pColorDepthCombo->FindString(JonesWx::FromGame(depthText.c_str()));
                if ( comboIdx == wxNOT_FOUND )
                {
                    comboIdx = pColorDepthCombo->Append(JonesWx::FromGame(depthText.c_str()));
                    aColorDepths.push_back({ colorDepthIdx, colorDepthMask });
                }

                if ( pMode->rasterInfo.width == curVideoMode.rasterInfo.width
                    && pMode->rasterInfo.height == curVideoMode.rasterInfo.height
                    && pMode->rasterInfo.colorInfo.bpp == curVideoMode.rasterInfo.colorInfo.bpp )
                {
                    selVideoModeNum   = videoModeNum;
                    selColorDepthMask = aColorDepths[comboIdx].colorDepthMask;
                    bHasCurVideoMode  = true;
                    pColorDepthCombo->SetSelection(comboIdx);
                }
            }

            if ( aVideoModes.empty() )
            {
                aCurResolutions.clear();
                pResText->SetLabel(wxString());
                return;
            }

            if ( !bHasCurVideoMode )
            {
                // No color depth selected, so we select first video mode
                selVideoModeNum   = 0;
                selColorDepthMask = aVideoModes[0].colorDepthFlags;
                pColorDepthCombo->SetSelection(0);
            }

            UpdateResolutions(selColorDepthMask, curResText.empty() ? aVideoModes[selVideoModeNum].resolutionText : curResText);
        }

    protected:
        void OnButton(int resultID) override
        {
            switch ( resultID )
            {
                case 1093: // Advanced settings
                    ShowAdvancedDisplaySettings(this);
                    return;

                case 1099: // Default
                    SetDefaults();
                    return;

                case 1:
                    SaveSettings();
                    break;
            }

            EndModal(resultID);
        }

    private:
        friend int ShowAdvancedDisplaySettings(DisplaySettingsDialog* pDisplayDlg);

        std::string GetResolutionText() const
        {
            return JonesWx::ToGame(pResText->GetLabel());
        }

        ColorDepthItem GetSelectedColorDepth() const
        {
            int sel = pColorDepthCombo->GetSelection();
            if ( sel == wxNOT_FOUND || (size_t)sel >= aColorDepths.size() ) {
                return { 0, 0 };
            }
            return aColorDepths[sel];
        }

        // Updates list of resolutions for color depth and selects the one matching resText
        void UpdateResolutions(int colorDepthMask, const std::string& resText)
        {
            aCurResolutions.clear();
            size_t selIdx = 0;
            for ( const VideoMode& mode : aVideoModes )
            {
                if ( (colorDepthMask & mode.colorDepthFlags) != 0 )
                {
                    if ( mode.resolutionText == resText ) {
                        selIdx = aCurResolutions.size();
                    }
                    aCurResolutions.push_back(&mode);
                }
            }

            pResSlider->SetRange(0, aCurResolutions.empty() ? 0 : (int)aCurResolutions.size() - 1);
            pResSlider->SetTickFreq(1);
            pResSlider->SetValue((int)selIdx);
            UpdateResolutionText();
        }

        void UpdateResolutionText()
        {
            const int pos = pResSlider->GetValue();
            if ( pos >= 0 && (size_t)pos < aCurResolutions.size() ) {
                pResText->SetLabel(JonesWx::FromGame(aCurResolutions[pos]->resolutionText.c_str()));
            }
        }

        void SetDefaults()
        {
            // Select 16 bpp, 640x480
            for ( size_t i = 0; i < aColorDepths.size(); ++i )
            {
                if ( aColorDepths[i].colorDepthMask == colorDepth16bpp )
                {
                    pColorDepthCombo->SetSelection((int)i);
                    break;
                }
            }

            UpdateResolutions(colorDepth16bpp, FormatGameStr("JONES_STR_BYPIXELS", 640, 480));
        }

        void SaveSettings()
        {
            // Save new screen resolution
            pSettings->videoModeNum = 0;
            const int pos = pResSlider->GetValue();
            if ( pos >= 0 && (size_t)pos < aCurResolutions.size() ) {
                pSettings->videoModeNum = aCurResolutions[pos]->aColorDepthVideoModes[GetSelectedColorDepth().colorDepthIdx];
            }

            StdDisplayInfo* pDisplayInfo = &pDisplayEnv->aDisplayInfos[pSettings->displayDeviceNum];
            pSettings->width  = pDisplayInfo->aModes[pSettings->videoModeNum].rasterInfo.width;
            pSettings->height = pDisplayInfo->aModes[pSettings->videoModeNum].rasterInfo.height;

            wuRegistry_SaveStr("Display", pDisplayInfo->displayDevice.aDriverName);
            wuRegistry_SaveStr("3D Device", pDisplayInfo->aDevices[pSettings->device3DNum].deviceDescription);
            wuRegistry_SaveInt("Width", pSettings->width);
            wuRegistry_SaveInt("Height", pSettings->height);
            wuRegistry_SaveInt("BPP", pDisplayInfo->aModes[pSettings->videoModeNum].rasterInfo.colorInfo.bpp);

            // Set new fog config
            wuRegistry_SaveFloat("Fog Density", sithRender_g_fogDensity / 100.0f);
            wuRegistry_SaveIntEx("Fog", pSettings->bFog);
            std3D_EnableFog(pSettings->bFog, sithRender_g_fogDensity);

            if ( sithWorld_g_pCurrentWorld ) {
                sithWorld_g_pCurrentWorld->state = (SithWorldState)(sithWorld_g_pCurrentWorld->state | SITH_WORLD_STATE_UPDATE_FOG);
            }

            // Save buffering and tex filtering mode
            wuRegistry_SaveIntEx("Buffering", pSettings->bBuffering);
            wuRegistry_SaveInt("Filter", pSettings->filter);
        }

        StdDisplayEnvironment* pDisplayEnv;
        JonesDisplaySettings* pSettings;

        std::vector<VideoMode> aVideoModes;
        std::vector<const VideoMode*> aCurResolutions;
        std::vector<ColorDepthItem> aColorDepths; // Parallel to color depth combo items

        wxSlider* pResSlider;
        wxStaticText* pResText;
        wxChoice* pColorDepthCombo;
        wxButton* pOkBtn;
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Advanced display settings

    class AdvancedDisplaySettingsDialog : public JonesWxDialog
    {
    public:
        AdvancedDisplaySettingsDialog(DisplaySettingsDialog* pDisplayDlg, StdDisplayEnvironment* pDisplayEnv, JonesDisplaySettings* pSettings) :
            JonesWxDialog((HWND)pDisplayDlg->GetHandle(), "JONES_STR_DPLY_OPTS"),
            pDisplayDlg(pDisplayDlg),
            pDisplayEnv(pDisplayEnv),
            pSettings(pSettings)
        {
            wxStaticBoxSizer* pSettingsBox = new wxStaticBoxSizer(wxVERTICAL, this, JonesWx::Str("JONES_STR_ADVDSPLY_SET"));
            wxWindow* pBox = pSettingsBox->GetStaticBox();

            // 3D device & rasterizer
            pDeviceCombo = new wxChoice(pBox, wxID_ANY);
            pDeviceCombo->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { OnDeviceChanged(); });

            pRasterCombo = new wxChoice(pBox, wxID_ANY);
            pRasterCombo->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { OnRasterChanged(); });

            pSettingsBox->Add(CreateLabel(pBox, "JONES_STR_3D_DEVICE"));
            pSettingsBox->Add(pDeviceCombo, wxSizerFlags().Expand().Border(wxBOTTOM, Scaled(6)));
            pSettingsBox->Add(CreateLabel(pBox, "JONES_STR_3D_RASTER"));
            pSettingsBox->Add(pRasterCombo, wxSizerFlags().Expand().Border(wxBOTTOM, Scaled(8)));

            // Buffering, performance & mipmap radio groups
            wxStaticBoxSizer* pBufferBox = new wxStaticBoxSizer(wxVERTICAL, pBox, JonesWx::Str("JONES_STR_BUFFER"));
            pDoubleBufferRB = new wxRadioButton(pBufferBox->GetStaticBox(), wxID_ANY, JonesWx::Str("JONES_STR_DB_BUFFER"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
            pTripleBufferRB = new wxRadioButton(pBufferBox->GetStaticBox(), wxID_ANY, JonesWx::Str("JONES_STR_TPL_BUFFER"));
            pBufferBox->Add(pDoubleBufferRB);
            pBufferBox->Add(pTripleBufferRB);

            wxStaticBoxSizer* pSpeedBox = new wxStaticBoxSizer(wxVERTICAL, pBox, JonesWx::Str("JONES_STR_SPEED"));
            pAverageRB  = new wxRadioButton(pSpeedBox->GetStaticBox(), wxID_ANY, JonesWx::Str("JONES_STR_AVERAGE"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
            pFastRB     = new wxRadioButton(pSpeedBox->GetStaticBox(), wxID_ANY, JonesWx::Str("JONES_STR_FAST"));
            pVeryFastRB = new wxRadioButton(pSpeedBox->GetStaticBox(), wxID_ANY, JonesWx::Str("JONES_STR_VFAST"));
            pSpeedBox->Add(pAverageRB);
            pSpeedBox->Add(pFastRB);
            pSpeedBox->Add(pVeryFastRB);

            wxStaticBoxSizer* pMipmapBox = new wxStaticBoxSizer(wxVERTICAL, pBox, JonesWx::Str("JONES_STR_MIPMAP"));
            pNoMipmapRB  = new wxRadioButton(pMipmapBox->GetStaticBox(), wxID_ANY, JonesWx::Str("JONES_STR_NO_MIPMAP"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
            pBilinearRB  = new wxRadioButton(pMipmapBox->GetStaticBox(), wxID_ANY, JonesWx::Str("JONES_STR_BI_MIPMAP"));
            pTrilinearRB = new wxRadioButton(pMipmapBox->GetStaticBox(), wxID_ANY, JonesWx::Str("JONES_STR_TRI_MIPMAP"));
            pMipmapBox->Add(pNoMipmapRB);
            pMipmapBox->Add(pBilinearRB);
            pMipmapBox->Add(pTrilinearRB);

            wxBoxSizer* pGroupsRow = new wxBoxSizer(wxHORIZONTAL);
            pGroupsRow->Add(pBufferBox, wxSizerFlags(1).Expand());
            pGroupsRow->AddSpacer(Scaled(6));
            pGroupsRow->Add(pSpeedBox, wxSizerFlags(1).Expand());
            pGroupsRow->AddSpacer(Scaled(6));
            pGroupsRow->Add(pMipmapBox, wxSizerFlags(1).Expand());
            pSettingsBox->Add(pGroupsRow, wxSizerFlags().Expand().Border(wxBOTTOM, Scaled(8)));

            // Fog
            wxStaticBoxSizer* pFogBox = new wxStaticBoxSizer(wxVERTICAL, pBox, JonesWx::Str("JONES_STR_FOGDENSITY"));
            pFogCB = new wxCheckBox(pFogBox->GetStaticBox(), wxID_ANY, JonesWx::Str("JONES_STR_FOG"));
            pFogSlider = new wxSlider(pFogBox->GetStaticBox(), wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(Scaled(260), -1), wxSL_HORIZONTAL | wxSL_AUTOTICKS);
            pFogSlider->SetTickFreq(5);
            pFogSlider->SetPageSize(5);

            wxBoxSizer* pFogLabels = new wxBoxSizer(wxHORIZONTAL);
            pFogLabels->Add(new wxStaticText(pFogBox->GetStaticBox(), wxID_ANY, "0%"));
            pFogLabels->AddStretchSpacer();
            pFogLabels->Add(new wxStaticText(pFogBox->GetStaticBox(), wxID_ANY, "100%"));

            pFogBox->Add(pFogCB);
            pFogBox->Add(pFogSlider, wxSizerFlags().Expand());
            pFogBox->Add(pFogLabels, wxSizerFlags().Expand());

            wxBoxSizer* pFogRow = new wxBoxSizer(wxHORIZONTAL);
            pFogRow->Add(CreateResourceIcon(pBox, 136, 32), wxSizerFlags().CenterVertical().Border(wxLEFT | wxRIGHT, Scaled(16)));
            pFogRow->Add(pFogBox, wxSizerFlags(1).Expand());
            pSettingsBox->Add(pFogRow, wxSizerFlags().Expand().Border(wxBOTTOM, Scaled(6)));

            wxBoxSizer* pDefaultRow = new wxBoxSizer(wxHORIZONTAL);
            pDefaultRow->AddStretchSpacer();
            pDefaultRow->Add(CreateButton(pBox, 1099, "JONES_STR_DEFAULTSET"));
            pSettingsBox->Add(pDefaultRow, wxSizerFlags().Expand());

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pSettingsBox, wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ CreateButton(this, 1, "JONES_STR_OK", /*bDefault=*/true), CreateButton(this, 2, "JONES_STR_CANCEL") }), wxSizerFlags().Expand());

            InitDevices();
            InitOptions();
            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            if ( resultID == 1099 )
            {
                SetDefaults();
                return;
            }

            if ( resultID == 1 ) {
                SaveSettings();
            }

            EndModal(resultID);
        }

    private:
        void InitDevices()
        {
            // 3D devices, i.e.: display devices with HAL
            pDeviceCombo->Clear();
            aDeviceNums.clear();
            for ( size_t i = 0; i < pDisplayEnv->numInfos; ++i )
            {
                if ( pDisplayEnv->aDisplayInfos[i].displayDevice.bHAL == 1 )
                {
                    int idx = pDeviceCombo->Append(JonesWx::FromGame(pDisplayEnv->aDisplayInfos[i].displayDevice.aDriverName));
                    aDeviceNums.push_back(i);
                    if ( i == (size_t)pSettings->displayDeviceNum ) {
                        pDeviceCombo->SetSelection(idx);
                    }
                }
            }

            // 3D rasterizers of selected display device
            const wxString selRasterDesc = pRasterCombo->GetStringSelection();
            pRasterCombo->Clear();

            StdDisplayInfo* pDisplayInfo = &pDisplayEnv->aDisplayInfos[pSettings->displayDeviceNum];
            pRasterCombo->Enable(pDisplayInfo->displayDevice.bHAL != 0);
            if ( !pDisplayInfo->displayDevice.bHAL ) {
                return;
            }

            bool bCurSel = false;
            for ( size_t i = 0; i < pDisplayInfo->numDevices; ++i )
            {
                const wxString desc = JonesWx::FromGame(pDisplayInfo->aDevices[i].deviceDescription);
                int idx = pRasterCombo->Append(desc);
                if ( desc == selRasterDesc || pSettings->device3DNum == i )
                {
                    pRasterCombo->SetSelection(idx);
                    pSettings->device3DNum = i;
                    bCurSel = true;
                }
            }

            if ( !bCurSel )
            {
                // Set first device as selected
                pRasterCombo->SetSelection(0);
                pSettings->device3DNum = 0;
            }
        }

        void InitOptions()
        {
            (pSettings->bBuffering ? pTripleBufferRB : pDoubleBufferRB)->SetValue(true);

            switch ( pSettings->filter )
            {
                case STD3D_MIPMAPFILTER_NONE:
                    pNoMipmapRB->SetValue(true);
                    break;

                case STD3D_MIPMAPFILTER_BILINEAR:
                    pBilinearRB->SetValue(true);
                    break;

                case STD3D_MIPMAPFILTER_TRILINEAR:
                    pTrilinearRB->SetValue(true);
                    break;
            }

            switch ( wuRegistry_GetInt("Performance Level", 4) )
            {
                case 0:
                    pAverageRB->SetValue(true);
                    break;

                case 2:
                    pFastRB->SetValue(true);
                    break;

                case 4:
                    pVeryFastRB->SetValue(true);
                    break;
            }

            pFogCB->SetValue(pSettings->bFog != 0);
            pFogSlider->SetValue((int)sithRender_g_fogDensity);
        }

        void OnDeviceChanged()
        {
            int sel = pDeviceCombo->GetSelection();
            if ( sel == wxNOT_FOUND ) {
                return;
            }

            pSettings->displayDeviceNum = aDeviceNums[sel];
            InitDevices();
            pDisplayDlg->InitVideoModes();
        }

        void OnRasterChanged()
        {
            int sel = pRasterCombo->GetSelection();
            if ( sel == wxNOT_FOUND ) {
                return;
            }

            pSettings->device3DNum = sel; // Fixed: Original read selection of the 3D device combo box
            InitDevices();
            pDisplayDlg->InitVideoModes();
        }

        void SetDefaults()
        {
            pFogCB->SetValue(true);
            pSettings->bFog = 1;
            sithRender_g_fogDensity = 100.0f;
            pFogSlider->SetValue(100);

            pVeryFastRB->SetValue(true);

            pDoubleBufferRB->SetValue(true);
            pSettings->bBuffering = 0;

            pBilinearRB->SetValue(true);
        }

        void SaveSettings()
        {
            pSettings->bFog = pFogCB->GetValue();
            sithRender_g_fogDensity = (float)pFogSlider->GetValue();
            pSettings->fogDensity   = sithRender_g_fogDensity / 100.0f;

            pSettings->bBuffering = pTripleBufferRB->GetValue();

            if ( pNoMipmapRB->GetValue() ) {
                pSettings->filter = STD3D_MIPMAPFILTER_NONE;
            }
            else if ( pTrilinearRB->GetValue() ) {
                pSettings->filter = STD3D_MIPMAPFILTER_TRILINEAR;
            }
            else {
                pSettings->filter = STD3D_MIPMAPFILTER_BILINEAR;
            }

            const int perfLevel = pAverageRB->GetValue() ? 0 : pFastRB->GetValue() ? 2 : 4;

            // Fixed: Original compared to the performance level after it was already saved, so the message was never shown
            const bool bPerfChanged = perfLevel != wuRegistry_GetInt("Performance Level", 4);
            wuRegistry_SaveInt("Performance Level", perfLevel);

            if ( JonesMain_HasStarted() && bPerfChanged )
            {
                // Inform that the change takes effect after game restart
                const char* pMsgText = jonesString_GetString("JONES_STR_PERFORMANCE");
                if ( pMsgText ) {
                    jonesConfig_ShowMessageDialog((HWND)GetHandle(), "JONES_STR_DPLY_OPTS", pMsgText, 136); // 136 - tv icon
                }
            }
        }

        DisplaySettingsDialog* pDisplayDlg;
        StdDisplayEnvironment* pDisplayEnv;
        JonesDisplaySettings* pSettings;
        std::vector<size_t> aDeviceNums; // Display info index of 3D device combo items

        wxChoice* pDeviceCombo;
        wxChoice* pRasterCombo;
        wxRadioButton* pDoubleBufferRB;
        wxRadioButton* pTripleBufferRB;
        wxRadioButton* pAverageRB;
        wxRadioButton* pFastRB;
        wxRadioButton* pVeryFastRB;
        wxRadioButton* pNoMipmapRB;
        wxRadioButton* pBilinearRB;
        wxRadioButton* pTrilinearRB;
        wxCheckBox* pFogCB;
        wxSlider* pFogSlider;
    };

    int ShowAdvancedDisplaySettings(DisplaySettingsDialog* pDisplayDlg)
    {
        AdvancedDisplaySettingsDialog dlg(pDisplayDlg, pDisplayDlg->pDisplayEnv, pDisplayDlg->pSettings);
        return dlg.ShowModal(); // Game state is already set by the display settings dialog
    }
}

extern "C" {

int J3DAPI jonesConfig_ShowGamePlayOptions(HWND hWnd)
{
    if ( !JonesWx::EnsureInit() ) {
        return -1;
    }

    GamePlayOptionsDialog dlg(hWnd);
    return dlg.ShowGameModal();
}

int J3DAPI jonesConfig_ShowSoundSettingsDialog(HWND hWnd, JonesSoundSettings* pData)
{
    if ( !JonesWx::EnsureInit() ) {
        return -1;
    }

    SoundSettingsDialog dlg(hWnd, pData);
    return dlg.ShowGameModal();
}

int J3DAPI jonesConfig_ShowDisplaySettingsDialog(HWND hWnd, StdDisplayEnvironment* pDisplayEnv, JonesDisplaySettings* pDSettings)
{
    if ( !JonesWx::EnsureInit() ) {
        return -1;
    }

    // Save current settings, to restore in case of canceling
    const float curFogDensity = sithRender_g_fogDensity;
    const int curPerLevel     = wuRegistry_GetInt("Performance Level", 4);
    const JonesDisplaySettings curSettings = *pDSettings;

    int res;
    {
        DisplaySettingsDialog dlg(hWnd, pDisplayEnv, pDSettings);
        res = dlg.ShowGameModal();
    }

    if ( res == 1 ) // OK
    {
        int bMouseEnabled = wuRegistry_GetIntEx("Mouse Control", 0);
        jonesConfig_EnableMouseControl(bMouseEnabled);
    }
    else // Cancel
    {
        // Restore previous settings
        sithRender_g_fogDensity = curFogDensity;
        wuRegistry_SaveInt("Performance Level", curPerLevel);
        *pDSettings = curSettings;
    }

    return res;
}

} // extern "C"
