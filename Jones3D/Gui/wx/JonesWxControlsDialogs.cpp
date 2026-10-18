// Control options dialogs: control options (111), edit control scheme (115), create control scheme (116),
// assign key (117) and reassign key (120)
#include "JonesWx.h"
#include "JonesWxDialogs.h"

#include <Jones3D/Display/jonesConfig.h>
#include <Jones3D/Main/jonesString.h>
#include <Jones3D/Play/JonesControl.h>

#include <sith/Devices/sithControl.h>
#include <std/General/std.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>
#include <std/Win95/stdControl.h>
#include <w32util/wuRegistry.h>

#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/statbox.h>
#include <wx/timer.h>

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace
{
    constexpr int keyListID        = 1005;
    constexpr int controllerListID = 1006;

    constexpr int typewriterIconID = 145;

    // List item data: LOWORD - action ID, HIWORD - control ID (kid, cid or joystick axis kid)
    long MakeItemData(size_t actionId, size_t controlId)
    {
        return (long)MAKELONG(actionId, controlId);
    }

    void ShowMessage(wxWindow* pParent, const char* pText)
    {
        jonesConfig_ShowMessageDialog((HWND)pParent->GetHandle(), "JONES_STR_CTRL_OPTS", pText, typewriterIconID);
    }

    void ShowMessageStr(wxWindow* pParent, const char* pKey)
    {
        const char* pText = jonesString_GetString(pKey);
        if ( pText ) {
            ShowMessage(pParent, pText);
        }
    }

    std::string GetControlString(size_t controlId)
    {
        char aText[JONESCONFIG_CONTROLSTRING_MAXLEN] = { 0 };
        jonesConfig_ControlToString(controlId, aText);
        return aText;
    }

    bool IsControllerControl(size_t controlId)
    {
        return (controlId >= STDCONTROL_JOYSTICK_FIRSTKID && controlId <= STDCONTROL_JOYSTICK_LASTPOVCID)
            || (controlId >= JONESCONFIG_JOYAXISKID_UP && controlId <= JONESCONFIG_JOYAXISKID_GROUND);
    }

    bool IsKeyboardControl(size_t controlId)
    {
        return (controlId < STDCONTROL_MAX_KEYBOARD_BUTTONS || controlId > STDCONTROL_JOYSTICK_LASTPOVCID)
            && (controlId < JONESCONFIG_JOYAXISKID_UP || controlId > JONESCONFIG_JOYAXISKID_GROUND);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Assign key

    struct AssignKeyData
    {
        size_t actionId;
        JonesControlsScheme* pScheme;
        char aControlName[JONESCONFIG_CONTROLSTRING_MAXLEN];
        size_t controlId;
        int listID;
        wxListCtrl* pList; // Edit scheme list of listID
    };

    // Waits for the player to press key, button or move joystick axis
    class AssignKeyDialog : public JonesWxDialog
    {
    public:
        AssignKeyDialog(wxWindow* pParent, AssignKeyData* pData) :
            JonesWxDialog((HWND)pParent->GetHandle(), "JONES_STR_CTRL_OPTS"),
            pData(pData)
        {
            wxStaticText* pCaption = CreateLabel(this, pData->listID == controllerListID ? "JONES_STR_NEXTBUTTON" : "JONES_STR_NEXTKEY", wxALIGN_CENTRE_HORIZONTAL);
            wxStaticText* pAction  = new wxStaticText(this, wxID_ANY, JonesWx::FromGame(jonesConfig_GetControlActionName(pData->actionId, /*bCapitalized=*/1)), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);

            wxBoxSizer* pTextCol = new wxBoxSizer(wxVERTICAL);
            pTextCol->Add(pCaption, wxSizerFlags().Expand());
            pTextCol->AddSpacer(Scaled(8));
            pTextCol->Add(pAction, wxSizerFlags().Expand());
            pTextCol->AddSpacer(Scaled(12));
            pTextCol->Add(CreateButton(this, 2, "JONES_STR_SLOWCANCEL"), wxSizerFlags().Center());

            wxBoxSizer* pContent = new wxBoxSizer(wxHORIZONTAL);
            pContent->Add(CreateResourceIcon(this, typewriterIconID, 32), wxSizerFlags().Top().Border(wxRIGHT, Scaled(16)));
            pContent->Add(pTextCol, wxSizerFlags(1).Expand());
            FinishLayout(pContent);

            // Keys are read via game controls, don't let keyboard activate the cancel button
            Bind(wxEVT_CHAR_HOOK, [](wxKeyEvent& event) {
                if ( event.GetKeyCode() == WXK_ESCAPE ) {
                    event.Skip(); // Handled by base, cancels dialog
                }
            });

            // Periodically check for pressed key
            timer.Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
                if ( !bLocked ) {
                    ReadKey();
                }
            });
            timer.Start(100);
        }

        ~AssignKeyDialog()
        {
            timer.Stop();
        }

    private:
        void EndWithKey()
        {
            stdControl_SetActivation(bControlsActive);
            timer.Stop();
            EndModal(1);
        }

        void ShowLockedMessage(const char* pText)
        {
            bLocked = true;
            ShowMessage(this, pText);
            bLocked = false;
        }

        void ReadKey()
        {
            const int bJoyEnabled = wuRegistry_GetIntEx("Joystick Control", 0);

            // Read controls
            bControlsActive = stdControl_ControlsActive();
            stdControl_SetActivation(1);
            stdControl_ReadControls();

            // Check if any of the axis were moved
            if ( bJoyEnabled && pData->listID == controllerListID && ReadJoyAxisKey() )
            {
                EndWithKey();
                return;
            }

            // Check if button was pressed
            for ( size_t keyNum = 0; keyNum < STDCONTROL_MAX_KEYID; ++keyNum )
            {
                int bJustPressed;
                if ( !stdControl_ReadKey(keyNum, &bJustPressed) ) {
                    continue;
                }

                if ( pData->listID == keyListID && IsControllerControl(keyNum) )
                {
                    const char* pMsgText = jonesString_GetString("JONES_STR_KEYONLY");
                    if ( pMsgText ) {
                        ShowLockedMessage(pMsgText);
                    }
                    return;
                }

                if ( pData->listID == controllerListID && !IsControllerControl(keyNum) )
                {
                    const char* pMsgText = jonesString_GetString("JONES_STR_BUTTONONLY");
                    if ( pMsgText ) {
                        ShowLockedMessage(pMsgText);
                    }
                    return;
                }

                jonesConfig_ControlToString(keyNum, pData->aControlName);
                pData->controlId = keyNum;
                switch ( keyNum )
                {
                    case DIK_ESCAPE:
                    case DIK_MINUS:
                    case DIK_EQUALS:
                    case DIK_F4:
                    {
                        // Non mappable key, skip
                        const char* pMsgText = jonesString_GetString("JONES_STR_NO_MAP");
                        if ( pMsgText )
                        {
                            char aMsgText[256] = { 0 };
                            STD_FORMAT(aMsgText, pMsgText, pData->aControlName);
                            ShowLockedMessage(aMsgText);
                        }

                        memset(pData->aControlName, 0, sizeof(pData->aControlName));
                        return;
                    }

                    case DIK_1:
                    case DIK_2:
                    case DIK_3:
                    case DIK_4:
                    case DIK_5:
                    case DIK_6:
                    case DIK_7:
                    case DIK_8:
                    case DIK_9:
                    case DIK_0:
                    {
                        if ( pData->actionId >= JONESCONTROL_ACTION_BAZOOKA && pData->actionId <= JONESCONTROL_ACTION_GRENADE )
                        {
                            EndWithKey();
                            return; // Fixed: Original also showed the message below after closing the dialog
                        }

                        // Number keys are reserved for weapons
                        const char* pMsgText = jonesString_GetString("JONES_STR_NO_KEYMAP_NUM");
                        if ( pMsgText )
                        {
                            char aMsgText[256] = { 0 };
                            STD_FORMAT(aMsgText, pMsgText, pData->aControlName);
                            ShowLockedMessage(aMsgText);
                        }

                        memset(pData->aControlName, 0, sizeof(pData->aControlName));
                        return;
                    }

                    default:
                        EndWithKey();
                        return;
                }
            }
        }

        bool ReadJoyAxisKey()
        {
            size_t numJoysticks = stdControl_GetNumJoysticks();
            for ( size_t joyNum = 0; joyNum < numJoysticks; ++joyNum )
            {
                for ( size_t axisNum = 0; axisNum < STDCONTROL_JOYSTICK_NUMAXES; ++axisNum )
                {
                    int bJustPressed;
                    if ( stdControl_ReadAxisAsKey(STDCONTROL_GET_JOYSTICK_AXIS(joyNum, axisNum) | STDCONTROL_AID_POSITIVE_AXIS, &bJustPressed) )
                    {
                        if ( axisNum == STDCONTROL_JOYSTICK_AXIS_X ) {
                            pData->controlId = JONESCONFIG_JOYAXISKID_RIGHT;
                        }
                        else if ( axisNum == STDCONTROL_JOYSTICK_AXIS_Y ) {
                            pData->controlId = JONESCONFIG_JOYAXISKID_DOWN;
                        }
                        else if ( axisNum == STDCONTROL_JOYSTICK_AXIS_RZ ) {
                            pData->controlId = JONESCONFIG_JOYAXISKID_TWISTR;
                        }
                    }
                    else if ( stdControl_ReadAxisAsKey(STDCONTROL_GET_JOYSTICK_AXIS(joyNum, axisNum) | STDCONTROL_AID_NEGATIVE_AXIS, &bJustPressed) )
                    {
                        if ( axisNum == STDCONTROL_JOYSTICK_AXIS_X ) {
                            pData->controlId = JONESCONFIG_JOYAXISKID_LEFT;
                        }
                        else if ( axisNum == STDCONTROL_JOYSTICK_AXIS_Y ) {
                            pData->controlId = JONESCONFIG_JOYAXISKID_UP;
                        }
                        else if ( axisNum == STDCONTROL_JOYSTICK_AXIS_RZ ) {
                            pData->controlId = JONESCONFIG_JOYAXISKID_TWISTL;
                        }
                    }
                    else {
                        continue;
                    }

                    if ( pData->controlId )
                    {
                        jonesConfig_ControlToString(pData->controlId, pData->aControlName);
                        return true;
                    }
                }
            }

            return false;
        }

        AssignKeyData* pData;
        wxTimer timer;
        bool bLocked        = false;
        int bControlsActive = 0;
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Reassign key

    struct ReassignKeyData
    {
        AssignKeyData* pAssignKeyData;
        int index;       // List index of the existing binding
        size_t offset;   // Bind index of the existing binding
        size_t actionId; // Action of the existing binding
    };

    // Asks to move key binding from one action to another
    class ReassignKeyDialog : public JonesWxDialog
    {
    public:
        ReassignKeyDialog(wxWindow* pParent, ReassignKeyData* pData) :
            JonesWxDialog((HWND)pParent->GetHandle(), "JONES_STR_CTRL_OPTS"),
            pData(pData)
        {
            const std::string controlName = "\"" + GetControlString(pData->pAssignKeyData->controlId) + "\" ";

            wxBoxSizer* pTextCol = new wxBoxSizer(wxVERTICAL);
            pTextCol->Add(new wxStaticText(this, wxID_ANY, JonesWx::FormatStr("JONES_STR_NEWMAP", controlName.c_str()), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL), wxSizerFlags().Expand());
            pTextCol->AddSpacer(Scaled(8));
            pTextCol->Add(CreateActionLabel("JONES_STR_FROM", pData->actionId), wxSizerFlags().Expand());
            pTextCol->Add(CreateActionLabel("JONES_STR_TO", pData->pAssignKeyData->actionId), wxSizerFlags().Expand());
            pTextCol->AddSpacer(Scaled(8));
            pTextCol->Add(CreateLabel(this, "JONES_STR_REMAP", wxALIGN_CENTRE_HORIZONTAL), wxSizerFlags().Expand());

            wxBoxSizer* pRow = new wxBoxSizer(wxHORIZONTAL);
            pRow->Add(CreateResourceIcon(this, typewriterIconID, 32), wxSizerFlags().CenterVertical().Border(wxRIGHT, Scaled(16)));
            pRow->Add(pTextCol, wxSizerFlags(1).Expand());

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pRow, wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ CreateButton(this, 1, "JONES_STR_YES"), CreateButton(this, 2, "JONES_STR_NO", /*bDefault=*/true) }), wxSizerFlags().Expand());
            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            if ( resultID == 1 ) {
                RemoveExistingBinding();
            }

            EndModal(resultID);
        }

    private:
        wxStaticText* CreateActionLabel(const char* pFormatKey, size_t actionId)
        {
            const char* pFormat     = jonesString_GetString(pFormatKey);
            const char* pActionName = jonesConfig_GetControlActionName(actionId, /*bCapitalized=*/1);

            char aText[256] = { 0 };
            if ( pFormat && pActionName ) {
                STD_FORMAT(aText, pFormat, pActionName);
            }

            return new wxStaticText(this, wxID_ANY, JonesWx::FromGame(aText), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
        }

        void RemoveExistingBinding()
        {
            AssignKeyData* pAssignData = pData->pAssignKeyData;
            JonesControlsScheme* pScheme = pAssignData->pScheme;
            wxListCtrl* pList = pAssignData->pList;

            const int numBinds = pAssignData->listID == keyListID
                ? JONESCONTROL_ACTION_GETNUMKEYBOARDBINDS(pScheme->aActions[pData->actionId])
                : JONESCONTROL_ACTION_GETNUMCONTROLLERBINDS(pScheme->aActions[pData->actionId]);

            if ( numBinds == 1 )
            {
                // Keep the action row with empty binding
                pList->SetItemData(pData->index, MakeItemData(pData->actionId, 0));
                pList->SetItem(pData->index, 1, wxString());
            }
            else if ( numBinds > 1 )
            {
                pList->DeleteItem(pData->index);
            }

            for ( size_t i = pData->offset + 1; i < JONESCONTROL_ACTION_GETNUMBINDS(pScheme->aActions[pData->actionId]); ++i )
            {
                if ( i < STD_ARRAYLEN(pScheme->aActions[pData->actionId]) - 1 ) {
                    pScheme->aActions[pData->actionId][i] = pScheme->aActions[pData->actionId][i + 1];
                }
            }

            if ( pAssignData->listID == keyListID ) {
                JONESCONTROL_ACTION_UPDATENUMBINDINGS_KEYBOARD(pScheme->aActions[pData->actionId], -1);
            }
            else {
                JONESCONTROL_ACTION_UPDATENUMBINDINGS_CONTROLLER(pScheme->aActions[pData->actionId], -1);
            }
        }

        ReassignKeyData* pData;
    };

    // Shows assign key dialog until the player chooses unbound key or confirms reassignment.
    // Returns 1 when key was assigned and 2 when canceled.
    int ShowAssignKeyDialog(wxWindow* pParent, AssignKeyData* pData)
    {
        int remapDlgResult = 1;
        int mapKeyDlgResult, index;
        do
        {
            memset(pData->aControlName, 0, sizeof(pData->aControlName));
            pData->controlId = 0;

            {
                AssignKeyDialog dlg(pParent, pData);
                mapKeyDlgResult = dlg.ShowGameModal();
            }

            if ( mapKeyDlgResult != 1 ) {
                break;
            }

            size_t actionID = (size_t)-1, offset = 0;
            index = jonesConfig_AssignKey_CheckBindForKey(pData->pScheme, pData->controlId, &actionID, pData->listID, &offset);
            if ( index > -1 && actionID == pData->actionId )
            {
                // Key is already assigned to this action
                const char* pErrorText  = jonesString_GetString("JONES_STR_SAMEMAP");
                const char* pActionName = jonesConfig_GetControlActionName(pData->actionId, /*bCapitalized=*/1);
                if ( !pErrorText || !pActionName ) {
                    return 2;
                }

                char aMsgText[256] = { 0 };
                STD_FORMAT(aMsgText, "\"%s\" %s %s.", GetControlString(pData->controlId).c_str(), pErrorText, pActionName);
                ShowMessage(pParent, aMsgText);
                return 2;
            }

            // Key is assigned to another action, ask to reassign
            if ( index > -1 )
            {
                ReassignKeyData redata;
                redata.pAssignKeyData = pData;
                redata.index          = index;
                redata.actionId       = actionID;
                redata.offset         = offset;

                ReassignKeyDialog dlg(pParent, &redata);
                remapDlgResult = dlg.ShowGameModal();
            }
        } while ( index > -1 && remapDlgResult != 1 );

        return mapKeyDlgResult;
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Edit control scheme

    class EditControlSchemeDialog : public JonesWxDialog
    {
    public:
        EditControlSchemeDialog(wxWindow* pParent, JonesControlsScheme* pScheme, JonesControlsScheme* pDefaultScheme, bool bReadOnly) :
            JonesWxDialog((HWND)pParent->GetHandle(), "JONES_STR_CTRL_OPTS"),
            pScheme(pScheme),
            pDefaultScheme(pDefaultScheme),
            bReadOnly(bReadOnly)
        {
            wxStaticBoxSizer* pSettingsBox = new wxStaticBoxSizer(wxVERTICAL, this, JonesWx::FormatStr("JONES_STR_SETTINGS", pScheme->aName));
            wxWindow* pBox = pSettingsBox->GetStaticBox();

            pKeyList        = CreateList(pBox, keyListID, "JONES_STR_MAPKEY");
            pControllerList = CreateList(pBox, controllerListID, "JONES_STR_MAPBUTTON");

            wxFlexGridSizer* pLists = new wxFlexGridSizer(2, Scaled(4), Scaled(8));
            pLists->AddGrowableCol(0);
            pLists->AddGrowableCol(1);
            pLists->AddGrowableRow(1);
            pLists->Add(CreateLabel(pBox, "JONES_STR_KEYBOARD"));
            pLists->Add(CreateLabel(pBox, "JONES_STR_CONTROLLER"));
            pLists->Add(pKeyList, wxSizerFlags(1).Expand());
            pLists->Add(pControllerList, wxSizerFlags(1).Expand());

            wxButton* pDefaultBtn = CreateButton(pBox, 1099, "JONES_STR_DEFAULTSET");
            wxButton* pEditBtn    = CreateButton(pBox, 1056, "JONES_STR_EDIT_ASGNMNT");
            wxButton* pAddBtn     = CreateButton(pBox, 1046, "JONES_STR_ADD_ASGNMNT");
            wxButton* pDeleteBtn  = CreateButton(pBox, 1038, "JONES_STR_DEL_ASGNMNT");

            wxBoxSizer* pAssignBtns = new wxBoxSizer(wxVERTICAL);
            pAssignBtns->Add(pEditBtn, wxSizerFlags().Expand());
            pAssignBtns->AddSpacer(Scaled(4));
            pAssignBtns->Add(pAddBtn, wxSizerFlags().Expand());
            pAssignBtns->AddSpacer(Scaled(4));
            pAssignBtns->Add(pDeleteBtn, wxSizerFlags().Expand());

            wxBoxSizer* pBottomRow = new wxBoxSizer(wxHORIZONTAL);
            pBottomRow->Add(CreateResourceIcon(pBox, typewriterIconID, 32), wxSizerFlags().Top());
            pBottomRow->AddStretchSpacer();
            pBottomRow->Add(pDefaultBtn, wxSizerFlags().Top());
            pBottomRow->AddStretchSpacer();
            pBottomRow->Add(pAssignBtns);

            pSettingsBox->Add(pLists, wxSizerFlags(1).Expand());
            pSettingsBox->AddSpacer(Scaled(8));
            pSettingsBox->Add(pBottomRow, wxSizerFlags().Expand());

            wxButton* pSaveBtn = CreateButton(this, 1, "JONES_STR_SAVE_SCHEME");

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pSettingsBox, wxSizerFlags(1).Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ pSaveBtn, CreateButton(this, 2, "JONES_STR_CANCEL") }), wxSizerFlags().Expand());

            if ( bReadOnly )
            {
                // Default schemes can only be viewed
                for ( wxButton* pBtn : { pEditBtn, pAddBtn, pDeleteBtn, pSaveBtn, pDefaultBtn } ) {
                    pBtn->Disable();
                }
            }

            FillList(pKeyList, keyListID);
            FillList(pControllerList, controllerListID);
            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            switch ( resultID )
            {
                case 1046: // Add
                    if ( !bReadOnly ) {
                        AddAssignment();
                    }
                    return;

                case 1056: // Edit
                    if ( !bReadOnly ) {
                        EditAssignment();
                    }
                    return;

                case 1038: // Delete
                    if ( !bReadOnly ) {
                        DeleteAssignment();
                    }
                    return;

                case 1099: // Default scheme
                    SetDefaultScheme();
                    return;

                case 1: // Save
                    if ( !CheckRequiredBindings() ) {
                        return;
                    }

                    pScheme->bModified = 1;
                    break;
            }

            EndModal(resultID);
        }

    private:
        wxListCtrl* CreateList(wxWindow* pParent, int listID, const char* pColumnKey)
        {
            wxListCtrl* pList = new wxListCtrl(pParent, wxID_ANY, wxDefaultPosition, wxSize(Scaled(270), Scaled(220)), wxLC_REPORT | wxLC_SINGLE_SEL);
            pList->AppendColumn(JonesWx::Str("JONES_STR_FCTN"), wxLIST_FORMAT_LEFT, Scaled(160));
            pList->AppendColumn(JonesWx::Str(pColumnKey), wxLIST_FORMAT_LEFT, Scaled(85));

            // Last clicked list is the target of the edit buttons
            auto setCurList = [this, pList, listID]() {
                pCurList  = pList;
                curListID = listID;
            };
            pList->Bind(wxEVT_LIST_ITEM_SELECTED, [setCurList](wxListEvent& event) { setCurList(); event.Skip(); });
            pList->Bind(wxEVT_LEFT_DOWN, [setCurList](wxMouseEvent& event) { setCurList(); event.Skip(); });
            pList->Bind(wxEVT_SET_FOCUS, [setCurList](wxFocusEvent& event) { setCurList(); event.Skip(); });
            pList->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) {
                if ( !bReadOnly ) {
                    EditAssignment();
                }
            });
            return pList;
        }

        void FillList(wxListCtrl* pList, int listID)
        {
            pList->DeleteAllItems();

            long itemNum = 0;
            for ( size_t i = 0; i < JONESCONTROL_ACTION_NUMACTIONS; ++i )
            {
                const wxString actionName = JonesWx::FromGame(jonesConfig_GetControlActionName(i, /*bCapitalized=*/0));
                size_t numControllers = 0;

                for ( size_t bindNum = 1; bindNum <= (uint8_t)pScheme->aActions[i][0]; ++bindNum ) // Note, index 0 is num bindings state
                {
                    const size_t controlId = pScheme->aActions[i][bindNum];
                    if ( (listID == keyListID && IsKeyboardControl(controlId)) || (listID == controllerListID && IsControllerControl(controlId)) )
                    {
                        if ( listID == controllerListID ) {
                            ++numControllers;
                        }

                        pList->InsertItem(itemNum, actionName);
                        pList->SetItemData(itemNum, MakeItemData(i, controlId));
                        pList->SetItem(itemNum++, 1, JonesWx::FromGame(GetControlString(controlId).c_str()));
                    }
                }

                // Controller list has row for every action
                if ( !numControllers && listID == controllerListID )
                {
                    pList->InsertItem(itemNum, actionName);
                    pList->SetItemData(itemNum++, MakeItemData(i, 0));
                }
            }
        }

        long GetSelectedItem() const
        {
            return pCurList ? pCurList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) : -1;
        }

        // Returns action ID of selected list item and its bind index, or -1 if no item is selected
        int GetSelectedListItemBinding(long* pItemIdx, size_t* pBindIdx) const
        {
            long itemIdx = GetSelectedItem();
            if ( itemIdx < 0 ) {
                return -1;
            }

            const long data        = (long)pCurList->GetItemData(itemIdx);
            const size_t actionId  = LOWORD(data);
            const size_t controlId = HIWORD(data);
            STD_ASSERT(actionId < JONESCONTROL_ACTION_NUMACTIONS);

            size_t bindIdx = 0;
            size_t i;
            for ( i = 1; i <= JONESCONTROL_ACTION_GETNUMBINDS(pScheme->aActions[actionId]) && pScheme->aActions[actionId][i] != controlId; ++i ) {
                ++bindIdx;
            }

            if ( controlId && JONESCONTROL_ACTION_GETNUMBINDS(pScheme->aActions[actionId]) && i > JONESCONTROL_ACTION_GETNUMBINDS(pScheme->aActions[actionId]) ) {
                return -1;
            }

            if ( pItemIdx ) {
                *pItemIdx = itemIdx;
            }

            if ( pBindIdx ) {
                *pBindIdx = bindIdx;
            }

            return (int)actionId;
        }

        void EditAssignment()
        {
            long itemIdx;
            size_t bindIdx;
            int actionId = GetSelectedListItemBinding(&itemIdx, &bindIdx);
            if ( actionId < 0 )
            {
                ShowMessageStr(this, "JONES_STR_NOKEYACTION");
                return;
            }

            AssignKeyData data = {};
            data.pScheme  = pScheme;
            data.actionId = actionId;
            data.listID   = curListID;
            data.pList    = pCurList;
            if ( ShowAssignKeyDialog(this, &data) != 1 || !JONESCONTROL_ACTION_GETNUMBINDS(pScheme->aActions[data.actionId]) ) {
                return;
            }

            itemIdx = GetSelectedItem();
            pCurList->SetItem(itemIdx, 1, JonesWx::FromGame(data.aControlName));
            pCurList->SetItemData(itemIdx, MakeItemData(data.actionId, data.controlId));

            // Assign new key binding, note bind index 0 is num binds state
            pScheme->aActions[data.actionId][bindIdx + 1] = data.controlId;

            // Update num binds only if 0
            if ( data.listID == keyListID && !JONESCONTROL_ACTION_GETNUMKEYBOARDBINDS(pScheme->aActions[data.actionId]) ) {
                JONESCONTROL_ACTION_UPDATENUMBINDINGS_KEYBOARD(pScheme->aActions[data.actionId], +1);
            }
            else if ( data.listID == controllerListID && !JONESCONTROL_ACTION_GETNUMCONTROLLERBINDS(pScheme->aActions[data.actionId]) ) {
                JONESCONTROL_ACTION_UPDATENUMBINDINGS_CONTROLLER(pScheme->aActions[data.actionId], +1);
            }
        }

        void AddAssignment()
        {
            long itemIdx = GetSelectedItem();
            if ( itemIdx < 0 )
            {
                ShowMessageStr(this, "JONES_STR_NOKEYACTION");
                return;
            }

            AssignKeyData data = {};
            data.actionId = LOWORD(pCurList->GetItemData(itemIdx));
            data.pScheme  = pScheme;
            data.listID   = curListID;
            data.pList    = pCurList;
            if ( ShowAssignKeyDialog(this, &data) != 1 ) {
                return;
            }

            STD_ASSERT(data.actionId < JONESCONTROL_ACTION_NUMACTIONS);
            if ( JONESCONTROL_ACTION_HASMAXBINDS(pScheme->aActions[data.actionId]) )
            {
                // Max number of action key bindings reached
                const char* pErrorText  = jonesString_GetString("JONES_STR_EXCEED");
                const char* pActionName = jonesConfig_GetControlActionName(data.actionId, /*bCapitalized=*/1);
                if ( pErrorText && pActionName )
                {
                    char aMsgText[256] = { 0 };
                    STD_FORMAT(aMsgText, pErrorText, pActionName);
                    ShowMessage(this, aMsgText);
                }
                return;
            }

            const int numBinds = curListID == keyListID
                ? JONESCONTROL_ACTION_GETNUMKEYBOARDBINDS(pScheme->aActions[data.actionId])
                : JONESCONTROL_ACTION_GETNUMCONTROLLERBINDS(pScheme->aActions[data.actionId]);

            if ( numBinds <= 0 )
            {
                // Use the action's empty row
                pCurList->SetItemData(itemIdx, MakeItemData(data.actionId, data.controlId)); // Fixed: Original retrieved item data here instead of setting it
            }
            else
            {
                // Insert new row after the last row of the action
                const long numItems = pCurList->GetItemCount();
                while ( itemIdx < numItems && LOWORD(pCurList->GetItemData(itemIdx)) == data.actionId ) {
                    ++itemIdx;
                }

                pCurList->InsertItem(itemIdx, JonesWx::FromGame(jonesConfig_GetControlActionName(data.actionId, /*bCapitalized=*/0)));
                pCurList->SetItemData(itemIdx, MakeItemData(data.actionId, data.controlId));
            }

            pCurList->SetItemState(itemIdx, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            pCurList->SetItem(itemIdx, 1, JonesWx::FromGame(data.aControlName));

            // Assign next key binding
            JONES_CONTROLACTION_ADDNEXTKEYBIND(pScheme->aActions[data.actionId], data.controlId);
            if ( curListID == keyListID ) {
                JONESCONTROL_ACTION_UPDATENUMBINDINGS_KEYBOARD(pScheme->aActions[data.actionId], +1);
            }
            else {
                JONESCONTROL_ACTION_UPDATENUMBINDINGS_CONTROLLER(pScheme->aActions[data.actionId], +1);
            }
        }

        void DeleteAssignment()
        {
            long itemIdx;
            size_t bindIdx;
            int actionId = GetSelectedListItemBinding(&itemIdx, &bindIdx);
            if ( actionId < 0 ) {
                return;
            }

            const size_t numBinds = curListID == keyListID
                ? JONESCONTROL_ACTION_GETNUMKEYBOARDBINDS(pScheme->aActions[actionId])
                : JONESCONTROL_ACTION_GETNUMCONTROLLERBINDS(pScheme->aActions[actionId]);
            if ( !numBinds ) {
                return;
            }

            if ( numBinds <= 1 )
            {
                // Keep the action row with empty binding
                pCurList->SetItem(itemIdx, 1, wxString());
                pCurList->SetItemData(itemIdx, MakeItemData(actionId, 0));
            }
            else
            {
                pCurList->DeleteItem(itemIdx);
            }

            for ( size_t i = bindIdx + 1; i < JONESCONTROL_ACTION_GETNUMBINDS(pScheme->aActions[actionId]); ++i ) {
                pScheme->aActions[actionId][i] = pScheme->aActions[actionId][i + 1];
            }

            if ( curListID == keyListID ) {
                JONESCONTROL_ACTION_UPDATENUMBINDINGS_KEYBOARD(pScheme->aActions[actionId], -1);
            }
            else {
                JONESCONTROL_ACTION_UPDATENUMBINDINGS_CONTROLLER(pScheme->aActions[actionId], -1);
            }
        }

        void SetDefaultScheme()
        {
            if ( !pDefaultScheme ) {
                return;
            }

            for ( size_t i = 0; i < STD_ARRAYLEN(pScheme->aActions); ++i )
            {
                memset(pScheme->aActions[i], 0, sizeof(pScheme->aActions[i])); // Fixed: Original cleared only first bytes of the bindings
                for ( size_t j = 0; j <= JONESCONTROL_ACTION_GETNUMBINDS(pDefaultScheme->aActions[i]); ++j ) {
                    pScheme->aActions[i][j] = pDefaultScheme->aActions[i][j];
                }
            }

            FillList(pKeyList, keyListID);
            FillList(pControllerList, controllerListID);
        }

        // Checks that all required actions have key bindings
        bool CheckRequiredBindings()
        {
            std::string missing;
            for ( size_t i = 0; i < STD_ARRAYLEN(pScheme->aActions); ++i )
            {
                // Weapon actions are optional
                if ( JONESCONTROL_ACTION_GETNUMBINDS(pScheme->aActions[i]) == 0 && (i < JONESCONTROL_ACTION_FISTS || i > JONESCONTROL_ACTION_BAZOOKA) )
                {
                    const char* pActionName = jonesConfig_GetControlActionName(i, /*bCapitalized=*/1);
                    if ( pActionName ) {
                        missing += pActionName;
                    }
                    missing += "\n";
                }
            }

            if ( missing.empty() ) {
                return true;
            }

            const char* pErrorText = jonesString_GetString("JONES_STR_NO_MAPNG");
            std::string msg = std::string(pErrorText ? pErrorText : "") + "\n" + missing;
            ShowMessage(this, msg.c_str());
            return false;
        }

        JonesControlsScheme* pScheme;
        JonesControlsScheme* pDefaultScheme;
        bool bReadOnly;

        wxListCtrl* pKeyList;
        wxListCtrl* pControllerList;
        wxListCtrl* pCurList = nullptr;
        int curListID        = 0;
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Create control scheme

    class CreateControlSchemeDialog : public JonesWxDialog
    {
    public:
        CreateControlSchemeDialog(wxWindow* pParent, const wxListBox* pSchemeList) :
            JonesWxDialog((HWND)pParent->GetHandle(), "JONES_STR_CTRL_OPTS"),
            pSchemeList(pSchemeList)
        {
            pNameEdit = new wxTextCtrl(this, wxID_ANY, wxString(), wxDefaultPosition, wxSize(Scaled(200), -1));
            pNameEdit->SetMaxLength(60);

            wxBoxSizer* pNameCol = new wxBoxSizer(wxVERTICAL);
            pNameCol->Add(CreateLabel(this, "JONES_STR_NAME"));
            pNameCol->Add(pNameEdit, wxSizerFlags().Expand().Border(wxTOP, Scaled(2)));

            wxBoxSizer* pRow = new wxBoxSizer(wxHORIZONTAL);
            pRow->Add(CreateResourceIcon(this, typewriterIconID, 32), wxSizerFlags().Top().Border(wxRIGHT, Scaled(16)));
            pRow->Add(pNameCol, wxSizerFlags(1).Expand());

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pRow, wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ CreateButton(this, 1, "JONES_STR_OK", /*bDefault=*/true), CreateButton(this, 2, "JONES_STR_CANCEL") }), wxSizerFlags().Expand());
            FinishLayout(pContent);

            pNameEdit->SetFocus();
        }

        std::string GetSchemeName() const { return name; }

    protected:
        void OnButton(int resultID) override
        {
            if ( resultID == 1 && !ValidateName() ) {
                return;
            }

            EndModal(resultID);
        }

    private:
        bool ValidateName()
        {
            std::string newName = JonesWx::ToGame(pNameEdit->GetValue());
            if ( newName.empty() )
            {
                ShowMessageStr(this, "JONES_STR_NO_SCHEME");
                return false;
            }

            if ( strcspn(newName.c_str(), "/:\\\"?<>*|+") != newName.size() )
            {
                // Bad file name format
                const char* pErrorText = jonesString_GetString("JONES_STR_INVALIDFILE");
                if ( pErrorText )
                {
                    char aErrorText[256] = { 0 };
                    STD_FORMAT(aErrorText, pErrorText, newName.c_str());
                    ShowMessage(this, aErrorText);
                }

                pNameEdit->Clear();
                return false;
            }

            // Check if scheme with same name already exists
            for ( unsigned int i = 0; i < pSchemeList->GetCount(); ++i )
            {
                if ( JonesWx::ToGame(pSchemeList->GetString(i)) == newName )
                {
                    ShowMessageStr(this, "JONES_STR_DUP_SCHEME");
                    pNameEdit->Clear();
                    return false;
                }
            }

            name = newName;
            return true;
        }

        const wxListBox* pSchemeList;
        wxTextCtrl* pNameEdit;
        std::string name;
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Control options

    class ControlOptionsDialog : public JonesWxDialog
    {
    public:
        ControlOptionsDialog(HWND hwndParent, JonesControlsConfig* pConfig) :
            JonesWxDialog(hwndParent, "JONES_STR_CTRL_OPTS"),
            pConfig(pConfig)
        {
            wxStaticBoxSizer* pSchemesBox = new wxStaticBoxSizer(wxHORIZONTAL, this, JonesWx::Str("JONES_STR_AVAIL_KEYCONFIG"));
            wxWindow* pBox = pSchemesBox->GetStaticBox();

            pSchemeList = new wxListBox(pBox, wxID_ANY, wxDefaultPosition, wxSize(Scaled(180), Scaled(110)), 0, nullptr, wxLB_SINGLE | wxLB_HSCROLL);
            pSchemeList->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { OnSchemeSelected(); });
            pSchemeList->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) { OnSchemeSelected(); EditSelectedScheme(); });

            pMouseCB    = new wxCheckBox(pBox, wxID_ANY, JonesWx::Str("JONES_STR_CTRLMOUSE"));
            pJoystickCB = new wxCheckBox(pBox, wxID_ANY, JonesWx::Str("JONES_STR_CTRLJOYSTICK"));

            pDeleteBtn = CreateButton(pBox, 1022, "JONES_STR_DEL_SCHEME");
            pEditBtn   = CreateButton(pBox, 1028, "JONES_STR_EDIT_SCHEME");

            wxBoxSizer* pListCol = new wxBoxSizer(wxVERTICAL);
            pListCol->Add(pSchemeList, wxSizerFlags(1).Expand());
            pListCol->AddSpacer(Scaled(6));
            pListCol->Add(pMouseCB);
            pListCol->AddSpacer(Scaled(2));
            pListCol->Add(pJoystickCB);

            wxBoxSizer* pButtonsCol = new wxBoxSizer(wxVERTICAL);
            pButtonsCol->Add(pDeleteBtn, wxSizerFlags().Expand());
            pButtonsCol->AddSpacer(Scaled(4));
            pButtonsCol->Add(pEditBtn, wxSizerFlags().Expand());
            pButtonsCol->AddSpacer(Scaled(4));
            pButtonsCol->Add(CreateButton(pBox, 1029, "JONES_STR_NEW_SCHEME"), wxSizerFlags().Expand());

            pSchemesBox->Add(CreateResourceIcon(pBox, typewriterIconID, 32), wxSizerFlags().Top().Border(wxALL, Scaled(6)));
            pSchemesBox->Add(pListCol, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, Scaled(8)));
            pSchemesBox->Add(pButtonsCol);

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pSchemesBox, wxSizerFlags(1).Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ CreateButton(this, 1, "JONES_STR_OK", /*bDefault=*/true), CreateButton(this, 2, "JONES_STR_CANCEL") }), wxSizerFlags().Expand());

            InitSchemeList();
            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            switch ( resultID )
            {
                case 1022:
                    DeleteScheme();
                    return;

                case 1028:
                    EditSelectedScheme();
                    return;

                case 1029:
                    CreateNewScheme();
                    return;

                case 1:
                    if ( !Save() ) {
                        resultID = 2; // Cancel due to error
                    }
                    break;
            }

            EndModal(resultID);
        }

    private:
        void InitSchemeList()
        {
            std::string selectedName;
            if ( pConfig->selectedShemeIdx == -1 )
            {
                char aSelectedCfgName[128] = { 0 };
                wuRegistry_GetStr("Configuration", aSelectedCfgName, STD_ARRAYLEN(aSelectedCfgName), "");
                selectedName = aSelectedCfgName;
                if ( selectedName.empty() )
                {
                    const char* pDfltKeyset = jonesConfig_GetDefaultControlSchemeName();
                    if ( pDfltKeyset ) {
                        selectedName = pDfltKeyset;
                    }
                }
            }

            pConfig->selectedShemeIdx = 0;
            for ( size_t i = 0; i < pConfig->numSchemes; ++i )
            {
                pSchemeList->Append(JonesWx::FromGame(pConfig->aSchemes[i].aName));
                pConfig->aSchemes[i].bModified = 0;
                if ( selectedName == pConfig->aSchemes[i].aName ) {
                    pConfig->selectedShemeIdx = (int)i;
                }
            }

            pSchemeList->SetSelection(pConfig->selectedShemeIdx);
            UpdateButtons();

            pMouseCB->SetValue(wuRegistry_GetIntEx("Mouse Control", 0) != 0);

            if ( stdControl_GetNumJoysticks() ) {
                pJoystickCB->SetValue(wuRegistry_GetIntEx("Joystick Control", 0) != 0);
            }
            else {
                pJoystickCB->Disable();
            }
        }

        void OnSchemeSelected()
        {
            pConfig->selectedShemeIdx = pSchemeList->GetSelection();
            UpdateButtons();
        }

        void UpdateButtons()
        {
            // Default schemes can be only viewed and can't be deleted
            const bool bDefault = pSchemeList->GetSelection() < (int)JONESCONFIG_NUMDEFAULTCONTROLSCHEMES;
            const char* pText   = jonesString_GetString(bDefault ? "JONES_STR_VIEW_SCHEME" : "JONES_STR_EDIT_SCHEME");
            if ( pText ) {
                pEditBtn->SetLabel(JonesWx::FromGame(pText));
            }

            pDeleteBtn->Enable(!bDefault);
        }

        void DeleteScheme()
        {
            const int selIdx = pSchemeList->GetSelection();
            if ( selIdx == wxNOT_FOUND ) {
                return;
            }

            const std::string schemeName = JonesWx::ToGame(pSchemeList->GetString(selIdx));
            pSchemeList->Delete(selIdx);

            for ( size_t schemeNum = 0; schemeNum < pConfig->numSchemes; ++schemeNum )
            {
                if ( schemeName != pConfig->aSchemes[schemeNum].aName ) {
                    continue;
                }

                if ( (pConfig->numDeleteSchemes % 10) == 0 ) {
                    pConfig->paDeleteSchemes = (char(*)[128])STDREALLOC(pConfig->paDeleteSchemes, (pConfig->numDeleteSchemes + 10) * 128);
                }

                // Scheme file is deleted when dialog is closed with OK
                if ( pConfig->aSchemes[schemeNum].bModified != 1 && pConfig->paDeleteSchemes )
                {
                    char aPath[128] = { 0 };
                    STD_FORMAT(aPath, "%s\\%s%s", jonesConfig_GetKeySetsDirPath(), schemeName.c_str(), ".kfg");
                    STD_STRCPY(pConfig->paDeleteSchemes[pConfig->numDeleteSchemes], aPath);
                    ++pConfig->numDeleteSchemes;
                }

                // Remove scheme from list
                memmove(&pConfig->aSchemes[schemeNum], &pConfig->aSchemes[schemeNum + 1], sizeof(JonesControlsScheme) * (pConfig->numSchemes - schemeNum - 1));
                memset(&pConfig->aSchemes[pConfig->numSchemes - 1], 0, sizeof(JonesControlsScheme));

                if ( (int)schemeNum == pConfig->selectedShemeIdx )
                {
                    pConfig->selectedShemeIdx = pConfig->selectedShemeIdx > 0 ? pConfig->selectedShemeIdx - 1 : 0;
                    pSchemeList->SetSelection(pConfig->selectedShemeIdx);
                    UpdateButtons();
                }

                --pConfig->numSchemes;
                return;
            }
        }

        void EditSelectedScheme()
        {
            const int selIdx = pSchemeList->GetSelection();
            if ( selIdx == wxNOT_FOUND ) {
                return;
            }

            const std::string schemeName = JonesWx::ToGame(pSchemeList->GetString(selIdx));

            size_t schemeNum = 0;
            while ( schemeNum < pConfig->numSchemes && schemeName != pConfig->aSchemes[schemeNum].aName ) {
                ++schemeNum;
            }

            if ( schemeNum == pConfig->numSchemes ) {
                return;
            }

            JonesControlsScheme* pEditScheme = jonesConfig_CloneControlSchemesList(pConfig->aSchemes, schemeNum, 1);
            if ( !pEditScheme ) {
                return;
            }

            int result;
            {
                EditControlSchemeDialog dlg(this, pEditScheme, jonesConfig_GetDefaultControlScheme(pConfig), schemeNum < JONESCONFIG_NUMDEFAULTCONTROLSCHEMES);
                result = dlg.ShowGameModal();
            }

            if ( result == 1 )
            {
                pConfig->aSchemes[schemeNum].bModified = pEditScheme->bModified;
                if ( pConfig->aSchemes[schemeNum].bModified ) {
                    memcpy(pConfig->aSchemes[schemeNum].aActions, pEditScheme->aActions, sizeof(pEditScheme->aActions));
                }
            }

            stdMemory_Free(pEditScheme);
        }

        void CreateNewScheme()
        {
            // Ask for new scheme name
            std::string newName;
            {
                CreateControlSchemeDialog dlg(this, pSchemeList);
                if ( dlg.ShowGameModal() != 1 ) {
                    return;
                }
                newName = dlg.GetSchemeName();
            }

            // New scheme starts as a copy of the first default scheme
            JonesControlsScheme* pNewScheme = jonesConfig_CloneControlSchemesList(pConfig->aSchemes, 0, 1);
            if ( !pNewScheme )
            {
                ShowMessageStr(this, "JONES_STR_NO_MEM_SCHEME");
                return;
            }

            STD_STRCPY(pNewScheme->aName, newName.c_str());

            int result;
            {
                EditControlSchemeDialog dlg(this, pNewScheme, jonesConfig_GetDefaultControlScheme(pConfig), /*bReadOnly=*/false);
                result = dlg.ShowGameModal();
            }

            if ( result != 1 )
            {
                stdMemory_Free(pNewScheme);
                return;
            }

            // Insert new scheme in alphabetical order after default schemes
            size_t schemeIdx = JONESCONFIG_NUMDEFAULTCONTROLSCHEMES;
            while ( schemeIdx < pConfig->numSchemes && strcmp(newName.c_str(), pConfig->aSchemes[schemeIdx].aName) >= 0 ) {
                ++schemeIdx;
            }

            if ( (pConfig->numSchemes % 10) == 0 ) {
                pConfig->aSchemes = (JonesControlsScheme*)STDREALLOC(pConfig->aSchemes, sizeof(JonesControlsScheme) * (pConfig->numSchemes + 10));
            }

            if ( !pConfig->aSchemes )
            {
                stdMemory_Free(pNewScheme);
                ShowMessageStr(this, "JONES_STR_NO_MEM_SCHEME");
                return;
            }

            memmove(&pConfig->aSchemes[schemeIdx + 1], &pConfig->aSchemes[schemeIdx], sizeof(JonesControlsScheme) * (pConfig->numSchemes - schemeIdx));
            memcpy(&pConfig->aSchemes[schemeIdx], pNewScheme, sizeof(JonesControlsScheme));
            ++pConfig->numSchemes;
            stdMemory_Free(pNewScheme);

            pSchemeList->Insert(JonesWx::FromGame(newName.c_str()), (unsigned int)schemeIdx);
            pSchemeList->SetSelection((int)schemeIdx);
            pConfig->selectedShemeIdx = (int)schemeIdx;
            UpdateButtons();
        }

        bool Save()
        {
            const int bMouseEnabled = pMouseCB->GetValue();
            const int bJoyEnabled   = pJoystickCB->GetValue();

            // Delete removed scheme files
            for ( size_t i = 0; i < pConfig->numDeleteSchemes; ++i ) {
                remove(pConfig->paDeleteSchemes[i]);
            }

            pConfig->numDeleteSchemes = 0;
            if ( pConfig->paDeleteSchemes )
            {
                stdMemory_Free(pConfig->paDeleteSchemes);
                pConfig->paDeleteSchemes = NULL;
            }

            bool bSuccess = true;
            if ( !jonesConfig_CopyControlConfig(pConfig, jonesConfig_GetControlOptionsConfig()) )
            {
                ShowMessageStr(this, "JONES_STR_NO_MEM_SCHEME");
                bSuccess = false;
            }
            else
            {
                // Write modified schemes to file
                for ( size_t i = JONESCONFIG_NUMDEFAULTCONTROLSCHEMES; i < pConfig->numSchemes; ++i )
                {
                    if ( pConfig->aSchemes[i].bModified == 1 && !jonesConfig_ControlOptions_WriteScheme(&pConfig->aSchemes[i]) ) {
                        ShowMessageStr(this, "JONES_STR_NO_MEM_SCHEME");
                    }
                }
            }

            // Save mouse/joystick settings
            wuRegistry_SaveIntEx("Mouse Control", bMouseEnabled);
            wuRegistry_SaveIntEx("Joystick Control", bJoyEnabled);

            if ( bJoyEnabled )
            {
                JonesControlsScheme* pActiveScheme = jonesConfig_LoadActiveControlScheme();
                if ( pActiveScheme )
                {
                    JonesControl_EnableJoystickAxes();
                    jonesConfig_BindControls(pActiveScheme);
                    jonesConfig_FreeControlScheme(pActiveScheme);
                }
            }
            else
            {
                sithControl_UnbindJoystickAxes();
            }

            return bSuccess;
        }

        JonesControlsConfig* pConfig;

        wxListBox* pSchemeList;
        wxCheckBox* pMouseCB;
        wxCheckBox* pJoystickCB;
        wxButton* pDeleteBtn;
        wxButton* pEditBtn;
    };
}

extern "C" {

int J3DAPI jonesConfig_ShowControlOptions(HWND hWnd)
{
    if ( !JonesWx::EnsureInit() || !jonesConfig_ControlOptions_InitControlsConfig() ) {
        return 2;
    }

    JonesControlsConfig* pCurConfig = jonesConfig_GetControlOptionsConfig();

    JonesControlsConfig config = {};
    if ( !jonesConfig_CopyControlConfig(pCurConfig, &config) ) {
        return 2;
    }

    int result;
    {
        ControlOptionsDialog dlg(hWnd, &config);
        result = dlg.ShowGameModal();
    }

    if ( result == 1 )
    {
        // Added: Make sure we have valid index
        if ( config.numSchemes <= (size_t)config.selectedShemeIdx )
        {
            jonesConfig_FreeControlConfigEntry(&config);
            return 2;
        }

        // Set and bind selected scheme as active game control set
        wuRegistry_SaveStr("Configuration", config.aSchemes[config.selectedShemeIdx].aName);
        jonesConfig_BindControls(&config.aSchemes[config.selectedShemeIdx]);

        jonesConfig_FreeControlConfigEntry(&config);
        jonesConfig_FreeControlConfigEntry(pCurConfig);
        pCurConfig->numSchemes = 0;

        int bMouseEnabled = wuRegistry_GetIntEx("Mouse Control", 0);
        jonesConfig_EnableMouseControl(bMouseEnabled);
    }
    else
    {
        jonesConfig_FreeControlConfigEntry(&config);
    }

    return result;
}

} // extern "C"
