// Game progress dialogs: statistics (164), chapter completed (233), store (190) and purchase message box (212)
#include "JonesWx.h"
#include "JonesWxDialogs.h"

#include <Jones3D/Display/jonesConfig.h>
#include <Jones3D/Display/JonesHudConstants.h>
#include <Jones3D/Main/JonesLevel.h>
#include <Jones3D/Main/JonesMain.h>
#include <Jones3D/Main/jonesString.h>
#include <Jones3D/Play/jonesInventory.h>

#include <sith/Dss/sithGamesave.h>
#include <std/General/std.h>
#include <std/General/stdUtil.h>

#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/scrolbar.h>
#include <wx/statbox.h>

#include <algorithm>
#include <vector>

namespace
{
    const char* aLevelNames[JONESLEVEL_NUMLEVELS - 1] = // not including the general load level
    {
        "JONES_STR_CYN",
        "JONES_STR_BAB",
        "JONES_STR_RIV",
        "JONES_STR_SHS",
        "JONES_STR_LAG",
        "JONES_STR_VOL",
        "JONES_STR_TEM",
        "JONES_STR_JEP",
        "JONES_STR_TEO",
        "JONES_STR_OLV",
        "JONES_STR_SEA",
        "JONES_STR_PYR",
        "JONES_STR_SOL",
        "JONES_STR_NUB",
        "JONES_STR_INF",
        "JONES_STR_AET",
        "JONES_STR_PRU"
    };

    // Glyph rects of digits 0-9 in misc/numbers.bmp
    const wxRect aNumberGlyphRects[10] =
    {
        wxRect(wxPoint(11, 2),   wxPoint(44, 45)),
        wxRect(wxPoint(49, 2),   wxPoint(69, 45)),
        wxRect(wxPoint(78, 2),   wxPoint(106, 45)),
        wxRect(wxPoint(110, 2),  wxPoint(138, 45)),
        wxRect(wxPoint(11, 76),  wxPoint(44, 120)),
        wxRect(wxPoint(45, 76),  wxPoint(75, 120)),
        wxRect(wxPoint(77, 76),  wxPoint(107, 120)),
        wxRect(wxPoint(11, 150), wxPoint(45, 194)),
        wxRect(wxPoint(45, 150), wxPoint(75, 194)),
        wxRect(wxPoint(76, 150), wxPoint(107, 194))
    };

    // Area of IQ points in misc/iq.bmp
    const wxRect iqPointsRect(39, 207, 127, 46);

    // Creates IQ image with IQ points drawn on it
    wxBitmap CreateIQBitmap(int iqPoints, const wxSize& size)
    {
        wxImage iq      = JonesWx::LoadImage("misc\\iq.bmp");
        wxImage mask    = JonesWx::LoadImage("misc\\iqMask.bmp");
        wxImage numbers = JonesWx::LoadImage("misc\\numbers.bmp");
        if ( !iq.IsOk() || !mask.IsOk() || mask.GetSize() != iq.GetSize() ) {
            return wxBitmap();
        }

        // Clear points area, i.e.: make it transparent
        iq.SetRGB(iqPointsRect, 0, 0, 0);
        mask.SetRGB(iqPointsRect, 255, 255, 255);

        if ( numbers.IsOk() )
        {
            // Collect digits, last digit first
            std::vector<int> aDigits;
            int points = (std::max)(iqPoints, 0);
            do
            {
                aDigits.push_back(points % 10);
                points /= 10;
            } while ( points );

            int width = 0, height = 0;
            for ( int digit : aDigits )
            {
                width += aNumberGlyphRects[digit].width;
                height = (std::max)(height, aNumberGlyphRects[digit].height);
            }

            // Draw digits from right to left
            int x = iqPointsRect.x + (iqPointsRect.width - width) / 2 + width;
            const int y = iqPointsRect.y + (iqPointsRect.height - height) / 2;
            for ( int digit : aDigits )
            {
                const wxRect& glyph = aNumberGlyphRects[digit];
                x -= glyph.width;
                for ( int gy = 0; gy < glyph.height; ++gy )
                {
                    for ( int gx = 0; gx < glyph.width; ++gx )
                    {
                        const int px = x + gx, py = y + gy;
                        if ( px < 0 || py < 0 || px >= iq.GetWidth() || py >= iq.GetHeight() ) {
                            continue;
                        }

                        // Original draws digits with SRCAND onto the image and SRCCOPY onto the mask
                        const unsigned char r = numbers.GetRed(glyph.x + gx, glyph.y + gy);
                        const unsigned char g = numbers.GetGreen(glyph.x + gx, glyph.y + gy);
                        const unsigned char b = numbers.GetBlue(glyph.x + gx, glyph.y + gy);
                        iq.SetRGB(px, py, iq.GetRed(px, py) & r, iq.GetGreen(px, py) & g, iq.GetBlue(px, py) & b);
                        mask.SetRGB(px, py, r, g, b);
                    }
                }
            }
        }

        // Black mask pixel is opaque
        iq.InitAlpha();
        const unsigned char* pMask = mask.GetData();
        unsigned char* pAlpha      = iq.GetAlpha();
        const size_t numPixels     = (size_t)iq.GetWidth() * iq.GetHeight();
        for ( size_t i = 0; i < numPixels; ++i ) {
            pAlpha[i] = (unsigned char)(255 - (pMask[i * 3] + pMask[i * 3 + 1] + pMask[i * 3 + 2]) / 3);
        }

        iq.Rescale(size.x, size.y, wxIMAGE_QUALITY_HIGH);
        return wxBitmap(iq);
    }

    wxString FormatElapsedTime(uint32_t elapsedTime)
    {
        // Packed as hours << 8 | minutes
        const int hours   = (int)(elapsedTime >> 8);
        const int minutes = (int)(elapsedTime & 0xFF);
        return wxString::Format("%i : %02i ", hours, minutes);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Statistics

    class StatisticsDialog : public JonesWxDialog
    {
    public:
        StatisticsDialog(HWND hwndParent, SithGameStatistics* pStatistics) :
            JonesWxDialog(hwndParent, "JONES_STR_IQ"),
            pStatistics(pStatistics)
        {
            iqSize = wxSize(Scaled(130), Scaled(162));
            pIQImage = new wxStaticBitmap(this, wxID_ANY, CreateIQBitmap(jonesInventory_GetTotalIQPoints(), iqSize));

            pTitleText = new wxStaticText(this, wxID_ANY, JonesWx::Str("JONES_STR_SUMMARY"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);

            // Last position is game summary
            pLevelScroll = new wxScrollBar(this, wxID_ANY, wxDefaultPosition, wxSize(iqSize.x, -1), wxSB_HORIZONTAL);
            curPos = pStatistics->curLevelNum + 1;
            pLevelScroll->SetScrollbar(curPos, 1, curPos + 1, 1);
            pLevelScroll->Bind(wxEVT_SCROLL_CHANGED, [this](wxScrollEvent&) { OnScroll(); });
            pLevelScroll->Bind(wxEVT_SCROLL_THUMBTRACK, [this](wxScrollEvent&) { OnScroll(); });
            pLevelScroll->Bind(wxEVT_SCROLL_LINEUP, [this](wxScrollEvent&) { OnScroll(); });
            pLevelScroll->Bind(wxEVT_SCROLL_LINEDOWN, [this](wxScrollEvent&) { OnScroll(); });
            pLevelScroll->Bind(wxEVT_SCROLL_PAGEUP, [this](wxScrollEvent&) { OnScroll(); });
            pLevelScroll->Bind(wxEVT_SCROLL_PAGEDOWN, [this](wxScrollEvent&) { OnScroll(); });

            wxBoxSizer* pLeftCol = new wxBoxSizer(wxVERTICAL);
            pLeftCol->Add(pIQImage, wxSizerFlags().Center());
            pLeftCol->AddSpacer(Scaled(6));
            pLeftCol->Add(pTitleText, wxSizerFlags().Expand());
            pLeftCol->Add(pLevelScroll, wxSizerFlags().Expand());

            wxFlexGridSizer* pStats = new wxFlexGridSizer(2, Scaled(10), Scaled(12));
            pStats->AddGrowableCol(0);
            const char* aLabels[] = { "JONES_STR_CHS_DONE", "JONES_STR_TIME_PASSED", "JONES_STR_TREASURES", "JONES_STR_HINTS_SEEN", "JONES_STR_DFCLTYDROP" };
            for ( size_t i = 0; i < STD_ARRAYLEN(aLabels); ++i )
            {
                pStats->Add(CreateLabel(this, aLabels[i], wxALIGN_RIGHT), wxSizerFlags().Expand());
                apValues[i] = new wxStaticText(this, wxID_ANY, wxString(), wxDefaultPosition, wxSize(Scaled(50), -1), wxALIGN_RIGHT | wxST_NO_AUTORESIZE);
                pStats->Add(apValues[i], wxSizerFlags().Expand());
            }

            wxBoxSizer* pRow = new wxBoxSizer(wxHORIZONTAL);
            pRow->Add(pLeftCol);
            pRow->AddSpacer(Scaled(16));
            pRow->Add(pStats, wxSizerFlags(1).Top());

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pRow, wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ CreateButton(this, 1, "JONES_STR_CLOSE", /*bDefault=*/true) }), wxSizerFlags().Expand());

            SetLevel(JONESLEVEL_BONUSLEVELNUM); // Summary
            FinishLayout(pContent);
        }

    private:
        void SetLevel(int levelNum)
        {
            // Note, level num JONESLEVEL_BONUSLEVELNUM points to the total level stats
            const SithLevelStatistic* pLevelStats = &pStatistics->aLevelStatistic[levelNum];
            apValues[0]->SetLabel(wxString::Format("%i ", pStatistics->curLevelNum));
            apValues[1]->SetLabel(FormatElapsedTime(pLevelStats->elapsedTime));
            apValues[2]->SetLabel(wxString::Format("%i ", (int)pLevelStats->numFoundTreasures));
            apValues[3]->SetLabel(wxString::Format("%i ", (int)pLevelStats->numSeenHints));
            apValues[4]->SetLabel(wxString::Format("%i ", pLevelStats->difficultyPenalty));
        }

        void OnScroll()
        {
            const int scrollPos = pLevelScroll->GetThumbPosition();
            if ( scrollPos == curPos ) {
                return;
            }

            int levelNum, iqPoints;
            const char* pTitleKey;
            if ( scrollPos == pLevelScroll->GetRange() - 1 )
            {
                pTitleKey = "JONES_STR_SUMMARY";
                levelNum  = JONESLEVEL_BONUSLEVELNUM;
                iqPoints  = jonesInventory_GetTotalIQPoints();
            }
            else
            {
                const int curLevelNum = jonesConfig_GetCurrentLevelNum();
                pTitleKey = scrollPos == curLevelNum && curLevelNum > 0 ? aLevelNames[JONESLEVEL_LASTLEVELNUM] : aLevelNames[scrollPos];
                levelNum  = scrollPos;
                iqPoints  = pStatistics->aLevelStatistic[scrollPos].iqPoints;
            }

            pTitleText->SetLabel(JonesWx::Str(pTitleKey));
            SetLevel(levelNum);
            pIQImage->SetBitmap(CreateIQBitmap(iqPoints, iqSize));
            curPos = scrollPos;
        }

        SithGameStatistics* pStatistics;
        wxSize iqSize;
        wxStaticBitmap* pIQImage;
        wxStaticText* pTitleText;
        wxScrollBar* pLevelScroll;
        wxStaticText* apValues[5];
        int curPos;
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Purchase message box

    class PurchaseMessageBox : public JonesWxDialog
    {
    public:
        PurchaseMessageBox(wxWindow* pParent, const tStoreCartState* pCart) :
            JonesWxDialog((HWND)pParent->GetHandle(), "JONES_STR_STORE")
        {
            wxString text;
            if ( pCart->total )
            {
                text += JonesWx::StrOr("JONES_STR_ADDINV", "") + " ";
                text += JonesWx::FormatStr("JONES_STR_SUBCHEST", pCart->total) + "  ";
                text += JonesWx::StrOr("JONES_STR_YESNOBUY", "");
            }
            else
            {
                text += JonesWx::StrOr("JONES_STR_NOCHANGE", "") + "  ";
                text += JonesWx::StrOr("JONES_STR_ABANDON", "");
            }

            wxStaticText* pText = new wxStaticText(this, wxID_ANY, text);
            pText->Wrap(Scaled(320));

            wxBoxSizer* pRow = new wxBoxSizer(wxHORIZONTAL);
            pRow->Add(CreateResourceIcon(this, 191, 32), wxSizerFlags().Top().Border(wxRIGHT, Scaled(12)));
            pRow->Add(pText, wxSizerFlags(1).CenterVertical());

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pRow, wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ CreateButton(this, 1, "JONES_STR_YES", /*bDefault=*/true), CreateButton(this, 2, "JONES_STR_NO") }), wxSizerFlags().Expand());
            FinishLayout(pContent);
        }
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Store

    // List item data: LOWORD - price, HIWORD - store item index
    long MakeStoreItemData(int price, size_t itemIdx)
    {
        return (long)MAKELONG(price, itemIdx);
    }

    class StoreDialog : public JonesWxDialog
    {
    public:
        StoreDialog(HWND hwndParent, tStoreCartState* pCart) :
            JonesWxDialog(hwndParent, "JONES_STR_STORE"),
            pCart(pCart)
        {
            const int iconSize = Scaled(48);
            imageList.Create(iconSize, iconSize, /*mask=*/true, (int)STD_ARRAYLEN(JonesHud_aStoreItems));
            for ( size_t i = 0; i < STD_ARRAYLEN(JonesHud_aStoreItems); ++i )
            {
                wxBitmap icon = JonesWx::LoadResourceIcon(JonesHud_aStoreItems[i].iconID, iconSize);
                imageList.Add(icon.IsOk() ? icon : wxBitmap(iconSize, iconSize));
            }

            wxStaticBoxSizer* pStoreBox = new wxStaticBoxSizer(wxHORIZONTAL, this, JonesWx::Str("JONES_STR_STORE"));
            wxWindow* pBox = pStoreBox->GetStaticBox();

            pCurrentList   = CreateList(pBox, "JONES_STR_COUNT", 0);
            pAvailableList = CreateList(pBox, "JONES_STR_COST", wxLC_SINGLE_SEL);
            pCartList      = CreateList(pBox, "JONES_STR_COST", wxLC_SINGLE_SEL);

            // Double click adds or removes item
            pAvailableList->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { AddCartItem(); });
            pCartList->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { RemoveCartItem(); });

            // Drag & drop between available and cart lists
            pAvailableList->Bind(wxEVT_LIST_BEGIN_DRAG, [this](wxListEvent&) { BeginDrag(pAvailableList); });
            pCartList->Bind(wxEVT_LIST_BEGIN_DRAG, [this](wxListEvent&) { BeginDrag(pCartList); });
            Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) { EndDrag(); });
            Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) { pDragList = nullptr; SetCursor(wxNullCursor); });

            pTotalText   = new wxStaticText(pBox, wxID_ANY, wxString(), wxDefaultPosition, wxSize(Scaled(50), -1), wxALIGN_RIGHT | wxST_NO_AUTORESIZE);
            pBalanceText = new wxStaticText(pBox, wxID_ANY, wxString(), wxDefaultPosition, wxSize(Scaled(50), -1), wxALIGN_RIGHT | wxST_NO_AUTORESIZE);

            wxFlexGridSizer* pBalances = new wxFlexGridSizer(2, Scaled(6), Scaled(8));
            pBalances->Add(CreateLabel(pBox, "JONES_STR_BALANCE"), wxSizerFlags().CenterVertical());
            pBalances->Add(pTotalText, wxSizerFlags().CenterVertical());
            pBalances->Add(CreateLabel(pBox, "JONES_STR_CREDIT"), wxSizerFlags().CenterVertical());
            pBalances->Add(pBalanceText, wxSizerFlags().CenterVertical());

            wxBoxSizer* pBalanceRow = new wxBoxSizer(wxHORIZONTAL);
            pBalanceRow->Add(CreateResourceIcon(pBox, 191, 32), wxSizerFlags().CenterVertical());
            pBalanceRow->AddStretchSpacer();
            pBalanceRow->Add(pBalances, wxSizerFlags().CenterVertical());

            wxBoxSizer* pCartCol = new wxBoxSizer(wxVERTICAL);
            pCartCol->Add(CreateLabel(pBox, "JONES_STR_CARTITEMS"));
            pCartCol->Add(pCartList, wxSizerFlags(1).Expand());
            pCartCol->AddSpacer(Scaled(8));
            pCartCol->Add(pBalanceRow, wxSizerFlags().Expand());

            pStoreBox->Add(CreateListColumn(pBox, "JONES_STR_CURRENT", pCurrentList), wxSizerFlags(1).Expand());
            pStoreBox->AddSpacer(Scaled(6));
            pStoreBox->Add(CreateListColumn(pBox, "JONES_STR_AVAILABLE", pAvailableList), wxSizerFlags(1).Expand());
            pStoreBox->AddSpacer(Scaled(6));
            pStoreBox->Add(pCartCol, wxSizerFlags(1).Expand());

            wxBoxSizer* pButtonRow = new wxBoxSizer(wxHORIZONTAL);
            pButtonRow->Add(CreateButton(this, 1168, "JONES_STR_ADD_ITEM"));
            pButtonRow->AddSpacer(Scaled(6));
            pButtonRow->Add(CreateButton(this, 1169, "JONES_STR_RETURN_ITEM"));
            pButtonRow->AddSpacer(Scaled(6));
            pButtonRow->Add(CreateButton(this, 1175, "JONES_STR_CLEAR"));
            pButtonRow->AddStretchSpacer();
            pButtonRow->Add(CreateButton(this, 1, "JONES_STR_DONE", /*bDefault=*/true));

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pStoreBox, wxSizerFlags(1).Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(pButtonRow, wxSizerFlags().Expand());

            // Note, current inventory list has to be filled first as filling available list resets the item counts
            FillCurrentList();
            FillAvailableList();
            pCart->total = 0;
            UpdateBalances();

            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            switch ( resultID )
            {
                case 1:
                case 2:
                {
                    if ( pCart->balance > 0 )
                    {
                        PurchaseMessageBox dlg(this, pCart);
                        if ( dlg.ShowGameModal() != 1 ) {
                            return;
                        }
                    }

                    EndModal(resultID);
                } return;

                case 1168:
                    AddCartItem();
                    return;

                case 1169:
                    RemoveCartItem();
                    return;

                case 1175:
                    ClearCart();
                    return;
            }
        }

    private:
        wxListCtrl* CreateList(wxWindow* pParent, const char* pCountColumnKey, long style)
        {
            wxListCtrl* pList = new wxListCtrl(pParent, wxID_ANY, wxDefaultPosition, wxSize(Scaled(220), Scaled(250)), wxLC_REPORT | style);
            pList->SetImageList(&imageList, wxIMAGE_LIST_SMALL);
            pList->AppendColumn(JonesWx::Str("JONES_STR_ITEMNAME"), wxLIST_FORMAT_LEFT, Scaled(150));
            pList->AppendColumn(JonesWx::Str(pCountColumnKey), wxLIST_FORMAT_RIGHT, Scaled(50));
            return pList;
        }

        wxSizer* CreateListColumn(wxWindow* pParent, const char* pTitleKey, wxListCtrl* pList)
        {
            wxBoxSizer* pCol = new wxBoxSizer(wxVERTICAL);
            pCol->Add(CreateLabel(pParent, pTitleKey));
            pCol->Add(pList, wxSizerFlags(1).Expand());
            return pCol;
        }

        void FillCurrentList()
        {
            long itemNum = 0;
            for ( size_t i = 0; i < STD_ARRAYLEN(JonesHud_aStoreItems); ++i )
            {
                const int amount = (pCart->apItemsState[i] & 0xFFF0) >> 4;
                if ( amount )
                {
                    pCurrentList->InsertItem(itemNum, JonesWx::Str(JonesHud_aStoreItems[i].aName), (int)i);
                    pCurrentList->SetItem(itemNum++, 1, wxString::Format("%d", amount));
                }
            }
        }

        void FillAvailableList()
        {
            long itemNum = 0;
            for ( size_t i = 0; i < STD_ARRAYLEN(JonesHud_aStoreItems); ++i )
            {
                if ( (pCart->apItemsState[i] & 0xF) != 0 ) // Item is available
                {
                    pAvailableList->InsertItem(itemNum, JonesWx::Str(JonesHud_aStoreItems[i].aClipName), (int)i);
                    pAvailableList->SetItemData(itemNum, MakeStoreItemData(JonesHud_aStoreItems[i].price, i));
                    pAvailableList->SetItem(itemNum++, 1, wxString::Format("%d", JonesHud_aStoreItems[i].price));
                }

                // Low word is now num items in cart
                pCart->apItemsState[i] &= ~0xFFFF;
            }
        }

        void UpdateBalances()
        {
            pTotalText->SetLabel(wxString::Format("%i ", pCart->total));
            pBalanceText->SetLabel(wxString::Format("%i ", pCart->balance));
        }

        std::vector<long> GetSelectedItems(wxListCtrl* pList) const
        {
            std::vector<long> aItems;
            for ( long idx = pList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED); idx != -1; idx = pList->GetNextItem(idx, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) ) {
                aItems.push_back(idx);
            }
            return aItems;
        }

        void ShowStoreMessage(const char* pKey)
        {
            const char* pText = jonesString_GetString(pKey);
            if ( pText ) {
                jonesConfig_ShowMessageDialog((HWND)GetHandle(), "JONES_STR_STORE", pText, 191);
            }
        }

        void AddCartItem()
        {
            const std::vector<long> aSelected = GetSelectedItems(pAvailableList);
            if ( aSelected.empty() ) {
                return;
            }

            int total = 0;
            bool bBonusMap = false;
            for ( long idx : aSelected )
            {
                const size_t storeItemIdx = HIWORD(pAvailableList->GetItemData(idx));
                if ( JonesHud_aStoreItems[storeItemIdx].menuID == JONESHUD_MENU_INVITEM_BONUSMAP ) {
                    bBonusMap = true;
                }

                total += JonesHud_aStoreItems[storeItemIdx].price;
            }

            if ( pCart->balance < total )
            {
                ShowStoreMessage(bBonusMap ? "JONES_STR_NOPERU" : aSelected.size() == 1 ? "JONES_STR_CANTBUY1" : "JONES_STR_CANTBUY");
                return;
            }

            if ( bBonusMap ) {
                ShowStoreMessage("JONES_STR_PERU");
            }

            for ( long idx : aSelected )
            {
                const long data           = (long)pAvailableList->GetItemData(idx);
                const size_t storeItemIdx = HIWORD(data);

                const long cartIdx = pCartList->GetItemCount();
                pCartList->InsertItem(cartIdx, pAvailableList->GetItemText(idx), (int)storeItemIdx);
                pCartList->SetItemData(cartIdx, data);
                pCartList->SetItem(cartIdx, 1, pAvailableList->GetItemText(idx, 1));

                int numItems = pCart->apItemsState[storeItemIdx];
                pCart->apItemsState[storeItemIdx] = (((uint16_t)numItems + 1) & 0xFFFF) | (numItems & 0xFFFF0000);
            }

            pCart->total   += total;
            pCart->balance -= total;
            UpdateBalances();
        }

        void RemoveCartItem()
        {
            std::vector<long> aSelected = GetSelectedItems(pCartList);
            if ( aSelected.empty() ) {
                return;
            }

            // Delete from the last item to keep indices valid
            int total = 0;
            for ( auto it = aSelected.rbegin(); it != aSelected.rend(); ++it )
            {
                const long data = (long)pCartList->GetItemData(*it);
                total += LOWORD(data);
                pCartList->DeleteItem(*it);

                int numItems = pCart->apItemsState[HIWORD(data)];
                pCart->apItemsState[HIWORD(data)] = (((uint16_t)numItems - 1) & 0xFFFF) | (numItems & 0xFFFF0000);
            }

            pCart->total   -= total;
            pCart->balance += total;
            UpdateBalances();
        }

        void ClearCart()
        {
            for ( long i = 0; i < pCartList->GetItemCount(); ++i )
            {
                const size_t idx = HIWORD(pCartList->GetItemData(i));
                pCart->apItemsState[idx] = (((uint16_t)pCart->apItemsState[idx] - 1) & 0xFFFF) | (pCart->apItemsState[idx] & 0xFFFF0000);
            }

            pCart->balance += pCart->total;
            pCart->total = 0;
            pCartList->DeleteAllItems();
            UpdateBalances();
        }

        void BeginDrag(wxListCtrl* pList)
        {
            pDragList = pList;
            SetCursor(wxCursor(wxCURSOR_HAND));
            CaptureMouse();
        }

        void EndDrag()
        {
            if ( !pDragList ) {
                return;
            }

            wxListCtrl* pSrcList = pDragList;
            pDragList = nullptr;
            if ( HasCapture() ) {
                ReleaseMouse();
            }
            SetCursor(wxNullCursor);

            const wxPoint pos = wxGetMousePosition();
            if ( pSrcList == pAvailableList && pCartList->GetScreenRect().Contains(pos) ) {
                AddCartItem();
            }
            else if ( pSrcList == pCartList && pAvailableList->GetScreenRect().Contains(pos) ) {
                RemoveCartItem();
            }
        }

        tStoreCartState* pCart;
        wxImageList imageList;
        wxListCtrl* pCurrentList;
        wxListCtrl* pAvailableList;
        wxListCtrl* pCartList;
        wxStaticText* pTotalText;
        wxStaticText* pBalanceText;
        wxListCtrl* pDragList = nullptr;
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Chapter completed

    class LevelCompletedDialog : public JonesWxDialog
    {
    public:
        LevelCompletedDialog(HWND hwndParent, tLevelCompletedDialogState* pState) :
            JonesWxDialog(hwndParent, "JONES_STR_CHPDONE"),
            pState(pState)
        {
            const int prevLevelNum = pState->pStatistics->curLevelNum - 1;
            wxStaticBitmap* pIQImage = new wxStaticBitmap(this, wxID_ANY, CreateIQBitmap(pState->pStatistics->aLevelStatistic[prevLevelNum].iqPoints, wxSize(Scaled(130), Scaled(162))));

            wxFlexGridSizer* pStats = new wxFlexGridSizer(2, Scaled(10), Scaled(12));
            pStats->AddGrowableCol(0);
            const std::pair<const char*, wxString> aRows[] = {
                { "JONES_STR_TIME_PASSED", FormatElapsedTime((uint32_t)pState->elapsedTime) },
                { "JONES_STR_TREASURES",   wxString::Format("%i ", pState->numFoundTrasures) },
                { "JONES_STR_WORTH",       wxString::Format("%i ", pState->foundTreasureValue) },
                { "JONES_STR_ALLWORTH",    wxString::Format("%i ", pState->totalTreasureValue) },
            };

            for ( const auto& row : aRows )
            {
                pStats->Add(CreateLabel(this, row.first, wxALIGN_RIGHT), wxSizerFlags().Expand());
                pStats->Add(new wxStaticText(this, wxID_ANY, row.second, wxDefaultPosition, wxSize(Scaled(50), -1), wxALIGN_RIGHT | wxST_NO_AUTORESIZE), wxSizerFlags().Expand());
            }

            wxBoxSizer* pRow = new wxBoxSizer(wxHORIZONTAL);
            pRow->Add(pIQImage);
            pRow->AddSpacer(Scaled(16));
            pRow->Add(pStats, wxSizerFlags(1).Top());

            wxBoxSizer* pContent = new wxBoxSizer(wxVERTICAL);
            pContent->Add(pRow, wxSizerFlags().Expand());
            pContent->AddSpacer(Scaled(12));
            pContent->Add(CreateButtonRow({ CreateButton(this, 1, "JONES_STR_CLOSE", /*bDefault=*/true) }), wxSizerFlags().Expand());
            FinishLayout(pContent);
        }

    protected:
        void OnButton(int resultID) override
        {
            // Show store if not bonus level
            if ( pState->pStatistics->curLevelNum + 1 < JONESLEVEL_BONUSLEVELNUM ) {
                ShowStore((HWND)GetHandle(), pState->balance, pState->apItemsState, pState->unknown2);
            }

            EndModal(resultID);
        }

    public:
        static int ShowStore(HWND hWnd, int* pBalance, int* pItemsState, int a4)
        {
            tStoreCartState cart = {};
            cart.balance      = *pBalance;
            cart.apItemsState = pItemsState;
            cart.unknown20    = a4;

            int result;
            {
                StoreDialog dlg(hWnd, &cart);
                result = dlg.ShowGameModal();
            }

            *pBalance = cart.balance;
            return result;
        }

    private:
        tLevelCompletedDialogState* pState;
    };
}

extern "C" {

int J3DAPI jonesConfig_ShowStatisticsDialog(HWND hWnd, SithGameStatistics* pStatistics)
{
    if ( !pStatistics || !JonesWx::EnsureInit() ) {
        return 2;
    }

    StatisticsDialog dlg(hWnd, pStatistics);
    return dlg.ShowGameModal();
}

int J3DAPI jonesConfig_ShowStoreDialog(HWND hWnd, int* pBalance, int* pItemsState, int a4)
{
    if ( !JonesWx::EnsureInit() ) {
        return 2;
    }

    return LevelCompletedDialog::ShowStore(hWnd, pBalance, pItemsState, a4);
}

int J3DAPI jonesConfig_ShowLevelCompletedDialog(HWND hWnd, int* pBalance, int* apItemsState, int a4, int elapsedTime, int qiPoints, int numFoundTrasures, int foundTrasureValue, int totalTreasureValue)
{
    if ( !JonesWx::EnsureInit() ) {
        return 2;
    }

    tLevelCompletedDialogState state = {};
    state.balance            = pBalance;
    state.apItemsState       = apItemsState;
    state.unknown2           = a4;
    state.elapsedTime        = elapsedTime;
    state.iqPoints           = qiPoints;
    state.numFoundTrasures   = numFoundTrasures;
    state.foundTreasureValue = foundTrasureValue;
    state.totalTreasureValue = totalTreasureValue;
    state.pStatistics        = sithGamesave_GetGameStatistics();

    if ( !state.pStatistics || !sithGamesave_LockGameStatistics() ) {
        return jonesConfig_ShowStoreDialog(hWnd, pBalance, apItemsState, a4);
    }

    int result;
    {
        LevelCompletedDialog dlg(hWnd, &state);
        result = dlg.ShowGameModal();
    }

    sithGamesave_UnlockGameStatistics();
    return result;
}

} // extern "C"
