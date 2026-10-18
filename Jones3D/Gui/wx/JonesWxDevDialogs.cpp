// Developer dialogs: dev launcher "Welcome to Indy 3D" (101) and display options (106)
#include "JonesWx.h"
#include "JonesWxDialogs.h"

#include <Jones3D/Main/JonesFile.h>
#include <Jones3D/Main/JonesMain.h>

#include <rdroid/types.h>
#include <sith/World/sithModel.h>
#include <std/General/std.h>
#include <std/General/stdFileUtil.h>
#include <std/General/stdFnames.h>
#include <std/General/stdUtil.h>

#include <wx/listbox.h>
#include <wx/statbox.h>

#include <string>
#include <vector>

namespace
{
    // Combo box with integer item data
    class DataChoice : public wxChoice
    {
    public:
        DataChoice(wxWindow* pParent) : wxChoice(pParent, wxID_ANY) {}

        void AddItem(const wxString& text, int data)
        {
            Append(text);
            aData.push_back(data);
        }

        void ClearItems()
        {
            Clear();
            aData.clear();
        }

        int GetSelectedData() const
        {
            int sel = GetSelection();
            return sel == wxNOT_FOUND || (size_t)sel >= aData.size() ? 0 : aData[sel];
        }

    private:
        std::vector<int> aData;
    };

    class DevDialog : public JonesWxDialog
    {
    public:
        DevDialog(HWND hwndParent, JonesState* pState, bool bLauncher) :
            JonesWxDialog(hwndParent, nullptr),
            pState(pState),
            bLauncher(bLauncher)
        {
            SetTitle(bLauncher ? "Welcome to Indy 3D" : "Options");
            pDisplayEnv = JonesMain_GetStartupDisplayEnvironment();

            // Display settings
            wxStaticBoxSizer* pDisplayBox = new wxStaticBoxSizer(wxVERTICAL, this, "Display Settings");
            wxWindow* pBox = pDisplayBox->GetStaticBox();

            pDisplayCombo = new DataChoice(pBox);
            p3DDeviceCombo = new DataChoice(pBox);
            pModeCombo = new DataChoice(pBox);
            pWindowCB = new wxCheckBox(pBox, wxID_ANY, "Open in a Window");

            pDisplayCombo->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
                this->pState->displaySettings.displayDeviceNum = pDisplayCombo->GetSelectedData();
                InitDisplayDevices();
            });
            p3DDeviceCombo->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
                this->pState->displaySettings.device3DNum = p3DDeviceCombo->GetSelectedData();
                InitDisplayDevices();
            });
            pModeCombo->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
                this->pState->displaySettings.videoModeNum = pModeCombo->GetSelectedData();
            });

            pDisplayBox->Add(pDisplayCombo, wxSizerFlags().Expand());
            pDisplayBox->Add(new wxStaticText(pBox, wxID_ANY, "3D Device"), wxSizerFlags().Border(wxTOP, Scaled(4)));
            pDisplayBox->Add(p3DDeviceCombo, wxSizerFlags().Expand());
            pDisplayBox->Add(new wxStaticText(pBox, wxID_ANY, "Display Mode"), wxSizerFlags().Border(wxTOP, Scaled(4)));
            pDisplayBox->Add(pModeCombo, wxSizerFlags().Expand());

            wxBoxSizer* pDisplayOptions = new wxBoxSizer(wxHORIZONTAL);
            pDisplayOptions->Add(pWindowCB, wxSizerFlags().CenterVertical());

            // Render modes
            pGeometryCombo = new DataChoice(this);
            pGeometryCombo->AddItem("Vertex Only", RD_GEOMETRY_VERTEX);
            pGeometryCombo->AddItem("Wire Frame", RD_GEOMETRY_WIREFRAME);
            pGeometryCombo->AddItem("Solid", RD_GEOMETRY_SOLID);
            pGeometryCombo->AddItem("Texture", RD_GEOMETRY_FULL);
            SelectData(pGeometryCombo, pState->displaySettings.geoMode, 0);

            pLightingCombo = new DataChoice(this);
            // Note, original dialog has data of "None" and "Lit" swapped
            pLightingCombo->AddItem("None", RD_LIGHTING_LIT);
            pLightingCombo->AddItem("Lit", RD_LIGHTING_NONE);
            pLightingCombo->AddItem("Diffuse", RD_LIGHTING_DIFFUSE);
            pLightingCombo->AddItem("Gouraud", RD_LIGHTING_GOURAUD);
            SelectData(pLightingCombo, pState->displaySettings.lightMode, 0);

            pFilterCombo = new DataChoice(bLauncher ? (wxWindow*)this : pBox);
            pFilterCombo->AddItem("None", STD3D_MIPMAPFILTER_NONE);
            pFilterCombo->AddItem("Bilinear", STD3D_MIPMAPFILTER_BILINEAR);
            pFilterCombo->AddItem("Trilinear", STD3D_MIPMAPFILTER_TRILINEAR);
            pFilterCombo->SetSelection(1); // Bilinear is default

            wxBoxSizer* pLeftCol = new wxBoxSizer(wxVERTICAL);
            pLeftCol->Add(pDisplayBox, wxSizerFlags().Expand());

            if ( !bLauncher )
            {
                pDisplayOptions->AddStretchSpacer();
                pDisplayOptions->Add(new wxStaticText(pBox, wxID_ANY, "Mip Map:"), wxSizerFlags().CenterVertical().Border(wxRIGHT, Scaled(4)));
                pDisplayOptions->Add(pFilterCombo, wxSizerFlags().CenterVertical());
                pDisplayBox->Add(pDisplayOptions, wxSizerFlags().Expand().Border(wxTOP, Scaled(6)));
                pGeometryCombo->Hide();
                pLightingCombo->Hide();
            }
            else
            {
                pDevModeCB = new wxCheckBox(pBox, wxID_ANY, "Dev Mode");
                pDevModeCB->SetValue(pState->bDevMode != 0);
                pDisplayOptions->AddStretchSpacer();
                pDisplayOptions->Add(pDevModeCB, wxSizerFlags().CenterVertical());
                pDisplayBox->Add(pDisplayOptions, wxSizerFlags().Expand().Border(wxTOP, Scaled(6)));

                wxBoxSizer* pModesRow = new wxBoxSizer(wxHORIZONTAL);
                pModesRow->Add(new wxStaticText(this, wxID_ANY, "Geometry:"), wxSizerFlags().CenterVertical().Border(wxRIGHT, Scaled(4)));
                pModesRow->Add(pGeometryCombo, wxSizerFlags(1).CenterVertical());
                pModesRow->AddSpacer(Scaled(8));
                pModesRow->Add(new wxStaticText(this, wxID_ANY, "Lighting:"), wxSizerFlags().CenterVertical().Border(wxRIGHT, Scaled(4)));
                pModesRow->Add(pLightingCombo, wxSizerFlags(1).CenterVertical());
                pModesRow->AddSpacer(Scaled(8));
                pModesRow->Add(new wxStaticText(this, wxID_ANY, "Mip Map:"), wxSizerFlags().CenterVertical().Border(wxRIGHT, Scaled(4)));
                pModesRow->Add(pFilterCombo, wxSizerFlags(1).CenterVertical());
                pLeftCol->Add(pModesRow, wxSizerFlags().Expand().Border(wxTOP, Scaled(8)));

                // Error output, debug output and performance level
                wxStaticBoxSizer* pErrorBox = new wxStaticBoxSizer(wxHORIZONTAL, this, "Error Output");
                apOutputRB[JONES_OUTPUTMODE_NONE]    = new wxRadioButton(pErrorBox->GetStaticBox(), wxID_ANY, "Normal", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
                apOutputRB[JONES_OUTPUTMODE_CONSOLE] = new wxRadioButton(pErrorBox->GetStaticBox(), wxID_ANY, "Console");
                apOutputRB[JONES_OUTPUTMODE_LOGFILE] = new wxRadioButton(pErrorBox->GetStaticBox(), wxID_ANY, "Log File");
                for ( wxRadioButton* pRB : apOutputRB ) {
                    pErrorBox->Add(pRB, wxSizerFlags().Border(wxRIGHT, Scaled(6)));
                }

                wxStaticBoxSizer* pDebugBox = new wxStaticBoxSizer(wxHORIZONTAL, this, "Debug Output");
                apLogLevelRB[JONES_LOGLEVEL_NORMAL]  = new wxRadioButton(pDebugBox->GetStaticBox(), wxID_ANY, "Normal", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
                apLogLevelRB[JONES_LOGLEVEL_VERBOSE] = new wxRadioButton(pDebugBox->GetStaticBox(), wxID_ANY, "Verbose");
                apLogLevelRB[JONES_LOGLEVEL_ERROR]   = new wxRadioButton(pDebugBox->GetStaticBox(), wxID_ANY, "Quiet");
                pDebugBox->Add(apLogLevelRB[JONES_LOGLEVEL_NORMAL], wxSizerFlags().Border(wxRIGHT, Scaled(6)));
                pDebugBox->Add(apLogLevelRB[JONES_LOGLEVEL_VERBOSE], wxSizerFlags().Border(wxRIGHT, Scaled(6)));
                pDebugBox->Add(apLogLevelRB[JONES_LOGLEVEL_ERROR], wxSizerFlags().Border(wxRIGHT, Scaled(6)));

                wxStaticBoxSizer* pPerfBox = new wxStaticBoxSizer(wxVERTICAL, this, "Performance Level");
                pHiPolyCB = new wxCheckBox(pPerfBox->GetStaticBox(), wxID_ANY, "High Poly Objects"); // Added
                pHiPolyCB->SetValue(sithModel_IsHiPolyEnabled() != 0);
                pPerfBox->Add(pHiPolyCB, wxSizerFlags().Border(wxBOTTOM, Scaled(6)));

                wxBoxSizer* pPerfRow = new wxBoxSizer(wxHORIZONTAL);
                for ( int i = 0; i < 5; ++i )
                {
                    apPerfRB[i] = new wxRadioButton(pPerfBox->GetStaticBox(), wxID_ANY, wxString::Format("%d", i), wxDefaultPosition, wxDefaultSize, i == 0 ? wxRB_GROUP : 0);
                    pPerfRow->Add(apPerfRB[i], wxSizerFlags().Border(wxRIGHT, Scaled(4)));
                }
                pPerfBox->Add(pPerfRow);

                wxBoxSizer* pOutputCol = new wxBoxSizer(wxVERTICAL);
                pOutputCol->Add(pErrorBox, wxSizerFlags().Expand());
                pOutputCol->AddSpacer(Scaled(6));
                pOutputCol->Add(pDebugBox, wxSizerFlags().Expand());

                wxBoxSizer* pDebugRow = new wxBoxSizer(wxHORIZONTAL);
                pDebugRow->Add(pOutputCol, wxSizerFlags(1).Expand());
                pDebugRow->AddSpacer(Scaled(6));
                pDebugRow->Add(pPerfBox, wxSizerFlags().Expand());
                pLeftCol->Add(pDebugRow, wxSizerFlags().Expand().Border(wxTOP, Scaled(8)));

                UpdateRadioButtons();
            }

            pStartBtn = CreateButton(this, 1, bLauncher ? "&Start Game" : "OK", /*bDefault=*/true);
            wxButton* pExitBtn = CreateButton(this, 2, bLauncher ? "&Exit" : "Cancel");

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            if ( bLauncher )
            {
                // Level list
                wxStaticBoxSizer* pLevelBox = new wxStaticBoxSizer(wxVERTICAL, this, "Select Level");
                pLevelList = new wxListBox(pLevelBox->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxSize(Scaled(190), Scaled(300)), 0, nullptr, wxLB_SINGLE | wxLB_SORT);
                pLevelList->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) { OnButton(1); });
                pLevelBox->Add(pLevelList, wxSizerFlags(1).Expand());
                FillLevelList();

                wxBoxSizer* pRow = new wxBoxSizer(wxHORIZONTAL);
                pRow->Add(pLeftCol, wxSizerFlags(1).Expand());
                pRow->AddSpacer(Scaled(8));
                pRow->Add(pLevelBox, wxSizerFlags().Expand());
                pContent->Add(pRow, wxSizerFlags(1).Expand());
            }
            else
            {
                pContent->Add(pLeftCol, wxSizerFlags(1).Expand());
            }

            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ pStartBtn, pExitBtn }), wxSizerFlags().Expand());

            InitDisplayDrivers();
            InitDisplayDevices();
            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            if ( resultID == 1 && !StartGame() ) {
                return;
            }

            EndModal(resultID);
        }

    private:
        static void SelectData(DataChoice* pCombo, int data, int defaultIdx)
        {
            for ( unsigned int i = 0; i < pCombo->GetCount(); ++i )
            {
                pCombo->SetSelection((int)i);
                if ( pCombo->GetSelectedData() == data ) {
                    return;
                }
            }
            pCombo->SetSelection(defaultIdx);
        }

        void InitDisplayDrivers()
        {
            for ( size_t i = 0; i < pDisplayEnv->numInfos; ++i )
            {
                if ( pDisplayEnv->aDisplayInfos[i].displayDevice.bHAL == 1 )
                {
                    pDisplayCombo->AddItem(JonesWx::FromGame(pDisplayEnv->aDisplayInfos[i].displayDevice.aDriverName), (int)i);
                    if ( i == (size_t)pState->displaySettings.displayDeviceNum ) {
                        pDisplayCombo->SetSelection((int)pDisplayCombo->GetCount() - 1);
                    }
                }
            }
        }

        void InitDisplayDevices()
        {
            StdDisplayInfo* pDisplay = &pDisplayEnv->aDisplayInfos[pState->displaySettings.displayDeviceNum];

            pModeCombo->ClearItems();
            if ( !pDisplay->displayDevice.bHAL )
            {
                for ( wxWindow* pWnd : { (wxWindow*)pStartBtn, (wxWindow*)pWindowCB, (wxWindow*)pModeCombo, (wxWindow*)p3DDeviceCombo } ) {
                    pWnd->Disable();
                }
                return;
            }

            pStartBtn->Enable();
            pWindowCB->Enable(pDisplay->displayDevice.bWindowRenderNotSupported == 0);
            if ( pDisplay->displayDevice.bWindowRenderNotSupported ) {
                pState->displaySettings.bWindowMode = 0;
            }

            pModeCombo->Enable();
            p3DDeviceCombo->Enable();
            pWindowCB->SetValue(pState->displaySettings.bWindowMode != 0);

            // 3D drivers
            const wxString selDriverName = p3DDeviceCombo->GetStringSelection();
            p3DDeviceCombo->ClearItems();

            bool bDriverSet = false;
            for ( size_t deviceNum = 0; deviceNum < pDisplay->numDevices; ++deviceNum )
            {
                const wxString name = JonesWx::FromGame(pDisplay->aDevices[deviceNum].deviceDescription);
                p3DDeviceCombo->AddItem(name, (int)deviceNum);
                if ( name == selDriverName || (size_t)pState->displaySettings.device3DNum == deviceNum )
                {
                    p3DDeviceCombo->SetSelection((int)deviceNum);
                    pState->displaySettings.device3DNum = deviceNum;
                    bDriverSet = true;
                }
            }

            if ( !bDriverSet )
            {
                pState->displaySettings.device3DNum = 0;
                p3DDeviceCombo->SetSelection(0);
            }

            // Display modes (resolutions)
            const StdVideoMode* pCurMode = JonesMain_GetCurVideoMode();
            bool bModeSet = false;
            for ( size_t modeNum = 0; modeNum < pDisplay->numModes; ++modeNum )
            {
                const StdVideoMode* pMode = &pDisplay->aModes[modeNum];
                if ( pMode->aspectRatio != 1.0f || pMode->rasterInfo.width < 512 || pMode->rasterInfo.height < 384 ) {
                    continue;
                }

                if ( !JonesMain_CurDisplaySupportsBPP(&pState->displaySettings, pMode->rasterInfo.colorInfo.bpp) ) {
                    continue;
                }

                pModeCombo->AddItem(wxString::Format("%dx%d %dbpp", (int)pMode->rasterInfo.width, (int)pMode->rasterInfo.height, (int)pMode->rasterInfo.colorInfo.bpp), (int)modeNum);
                if ( pMode->rasterInfo.width == pCurMode->rasterInfo.width
                    && pMode->rasterInfo.height == pCurMode->rasterInfo.height
                    && pMode->rasterInfo.colorInfo.bpp == pCurMode->rasterInfo.colorInfo.bpp )
                {
                    pModeCombo->SetSelection((int)pModeCombo->GetCount() - 1);
                    bModeSet = true;
                }
            }

            if ( !bModeSet ) {
                pModeCombo->SetSelection(0);
            }
        }

        void FillLevelList()
        {
            char aNdyDir[128];
            STD_MAKEPATH(aNdyDir, JonesFile_GetWorkingDirPath(), "ndy");
            if ( !stdUtil_DirExists(aNdyDir) ) {
                STD_MAKEPATH(aNdyDir, JonesFile_GetResourcePath(), "ndy"); // Use absolute resource path to search for level files
            }

            for ( const char* pExt : { "ndy", "cnd" } )
            {
                tFoundFileInfo fileInfo;
                FindFileData* pFileData = stdFileUtil_NewFind(aNdyDir, 3, pExt);
                while ( stdFileUtil_FindNext(pFileData, &fileInfo) ) {
                    pLevelList->Append(JonesWx::FromGame(fileInfo.aName));
                }
                stdFileUtil_DisposeFind(pFileData);
            }

            // Select cur level from config, or level in the middle of the list
            int sel = pLevelList->FindString(JonesWx::FromGame(pState->aCurLevelFilename), /*bCase=*/true);
            if ( sel == wxNOT_FOUND ) {
                sel = (int)pLevelList->GetCount() / 2;
            }

            if ( pLevelList->GetCount() ) {
                pLevelList->SetSelection(sel);
            }
        }

        void UpdateRadioButtons()
        {
            if ( pState->outputMode >= 0 && pState->outputMode < 3 ) {
                apOutputRB[pState->outputMode]->SetValue(true);
            }

            if ( pState->logLevel >= 0 && pState->logLevel < 3 ) {
                apLogLevelRB[pState->logLevel]->SetValue(true);
            }

            if ( pState->performanceLevel >= 0 && pState->performanceLevel < 5 ) {
                apPerfRB[pState->performanceLevel]->SetValue(true);
            }
        }

        bool StartGame()
        {
            JonesDisplaySettings* pSettings = &pState->displaySettings;
            pSettings->device3DNum  = p3DDeviceCombo->GetSelectedData();
            pSettings->videoModeNum = pModeCombo->GetSelectedData();
            pSettings->filter       = (Std3DMipmapFilterType)pFilterCombo->GetSelectedData();
            pSettings->bWindowMode  = pWindowCB->GetValue();

            if ( bLauncher )
            {
                pSettings->geoMode   = (rdGeometryMode)pGeometryCombo->GetSelectedData();
                pSettings->lightMode = (rdLightMode)pLightingCombo->GetSelectedData();
                if ( pSettings->geoMode == RD_GEOMETRY_WIREFRAME || pSettings->geoMode == RD_GEOMETRY_VERTEX ) {
                    pSettings->bClearBackBuffer = 1;
                }

                sithModel_EnableHiPoly(pHiPolyCB->GetValue()); // Added
                pState->bDevMode = pDevModeCB->GetValue();

                for ( int i = 0; i < 3; ++i )
                {
                    if ( apOutputRB[i]->GetValue() ) {
                        pState->outputMode = (JonesOuputMode)i;
                    }

                    if ( apLogLevelRB[i]->GetValue() ) {
                        pState->logLevel = (JonesLogLevel)i;
                    }
                }

                for ( int i = 0; i < 5; ++i )
                {
                    if ( apPerfRB[i]->GetValue() ) {
                        pState->performanceLevel = i;
                    }
                }
            }

            const StdVideoMode* pMode = &pDisplayEnv->aDisplayInfos[pSettings->displayDeviceNum].aModes[pSettings->videoModeNum];
            pSettings->width  = pMode->rasterInfo.width;
            pSettings->height = pMode->rasterInfo.height;

            if ( !bLauncher ) {
                return true; // Note, original display options didn't save settings because it has no level list
            }

            // Get selected level & save settings
            const int sel = pLevelList->GetSelection();
            if ( sel == wxNOT_FOUND ) {
                return false;
            }

            STD_STRCPY(pState->aCurLevelFilename, JonesWx::ToGame(pLevelList->GetString(sel)).c_str());
            JonesMain_SaveDevSettings(pState);
            return true;
        }

        JonesState* pState;
        bool bLauncher;
        StdDisplayEnvironment* pDisplayEnv;

        DataChoice* pDisplayCombo;
        DataChoice* p3DDeviceCombo;
        DataChoice* pModeCombo;
        DataChoice* pGeometryCombo;
        DataChoice* pLightingCombo;
        DataChoice* pFilterCombo;
        wxCheckBox* pWindowCB;
        wxCheckBox* pDevModeCB = nullptr;
        wxCheckBox* pHiPolyCB  = nullptr;
        wxRadioButton* apOutputRB[3]   = { nullptr };
        wxRadioButton* apLogLevelRB[3] = { nullptr };
        wxRadioButton* apPerfRB[5]     = { nullptr };
        wxListBox* pLevelList = nullptr;
        wxButton* pStartBtn;
    };
}

extern "C" {

int J3DAPI JonesMain_ShowDevDialog(HWND hWnd, JonesState* pConfig)
{
    if ( !JonesWx::EnsureInit() ) {
        return 2;
    }

    DevDialog dlg(hWnd, pConfig, /*bLauncher=*/true);
    return dlg.ShowGameModal();
}

int J3DAPI JonesMain_ShowDisplayOptions(HWND hWnd, JonesState* pState)
{
    if ( !JonesWx::EnsureInit() ) {
        return 2;
    }

    DevDialog dlg(hWnd, pState, /*bLauncher=*/false);
    return dlg.ShowGameModal();
}

} // extern "C"
