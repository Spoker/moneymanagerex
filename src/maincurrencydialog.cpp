/*******************************************************
 Copyright (C) 2006 Madhan Kanagavel
 Copyright (C) 2015 Gabriele-V
 Copyright (C) 2016 Nikolay

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ********************************************************/

#include "maincurrencydialog.h"
#include "currencydialog.h"
#include "constants.h"
#include "defs.h"
#include "images_list.h"
#include "mmCalculator.h"
#include "mmSimpleDialogs.h"
#include "mmtextctrl.h"
#include "paths.h"
#include "util.h"
#include "validators.h"
#include "model/allmodel.h"

#include <vector>
#include <wx/sstream.h>
#include <wx/xml/xml.h>

wxIMPLEMENT_DYNAMIC_CLASS(mmMainCurrencyDialog, wxDialog);

wxBEGIN_EVENT_TABLE(mmMainCurrencyDialog, wxDialog)
    EVT_BUTTON(wxID_ADD, mmMainCurrencyDialog::OnBtnAdd)
    EVT_BUTTON(wxID_EDIT, mmMainCurrencyDialog::OnBtnEdit)
    EVT_BUTTON(wxID_SELECTALL, mmMainCurrencyDialog::OnBtnSelect)
    EVT_BUTTON(wxID_REMOVE, mmMainCurrencyDialog::OnBtnDelete)
    EVT_BUTTON(wxID_CANCEL, mmMainCurrencyDialog::OnCancel)

    EVT_MENU_RANGE(MENU_ITEM1, MENU_ITEM3, mmMainCurrencyDialog::OnMenuSelected)

    EVT_DATAVIEW_ITEM_ACTIVATED(wxID_ANY, mmMainCurrencyDialog::OnListItemActivated)
    EVT_DATAVIEW_SELECTION_CHANGED(wxID_ANY, mmMainCurrencyDialog::OnListItemSelected)
    EVT_DATAVIEW_ITEM_CONTEXT_MENU(wxID_ANY, mmMainCurrencyDialog::OnItemRightClick)

    EVT_BUTTON(HISTORY_ADD, mmMainCurrencyDialog::OnHistoryAdd)
    EVT_BUTTON(HISTORY_DELETE, mmMainCurrencyDialog::OnHistoryDelete)
    EVT_BUTTON(HISTORY_UPDATE, mmMainCurrencyDialog::OnHistoryUpdate)
    EVT_BUTTON(HISTORY_DELUNUSED, mmMainCurrencyDialog::OnHistoryDeleteUnused)
    EVT_LIST_ITEM_SELECTED(wxID_ANY, mmMainCurrencyDialog::OnHistorySelected)
    EVT_LIST_ITEM_DESELECTED(wxID_ANY, mmMainCurrencyDialog::OnHistoryDeselected)
wxEND_EVENT_TABLE()

mmMainCurrencyDialog::mmMainCurrencyDialog(
    wxWindow* parent
    , int currencyID
    , bool bEnableSelect
) : currencyListBox_(nullptr),
    bEnableSelect_(bEnableSelect),
    m_static_dialog(false)
{
    ColName_[CURR_BASE]   = " ";
    ColName_[CURR_SYMBOL] = _("Symbol");
    ColName_[CURR_NAME]   = _("Name");
    ColName_[BASE_RATE]   = _("Last Rate");

    m_currency_id = currencyID == -1 ? Option::instance().BaseCurrency() : currencyID;
    long style = wxCAPTION | wxRESIZE_BORDER | wxSYSTEM_MENU | wxCLOSE_BOX;
    Create(parent, wxID_ANY, _("Currency Dialog"), wxDefaultPosition, wxDefaultSize, style);
}

bool mmMainCurrencyDialog::Create(wxWindow* parent
    , wxWindowID id
    , const wxString& caption
    , const wxPoint& pos
    , const wxSize& size
    , long style)
{
    SetExtraStyle(GetExtraStyle()|wxWS_EX_BLOCK_EVENTS);
    wxDialog::Create(parent, id, caption, pos, size, style);

    CreateControls();
    SetIcon(mmex::getProgramIcon());
    fillControls();
    Centre();

    return TRUE;
}

void mmMainCurrencyDialog::fillControls()
{
    currencyListBox_->DeleteAllItems();
    int baseCurrencyID = Option::instance().BaseCurrency();

    cbShowAll_->SetValue(Model_Infotable::instance().GetBoolInfo("SHOW_HIDDEN_CURRENCIES", true));

    bool skip_unused = !cbShowAll_->IsChecked();
    for (const auto& currency : Model_Currency::instance().all(Model_Currency::COL_CURRENCYNAME))
    {
        int currencyID = currency.CURRENCYID;
        bool currency_is_base = false;
        if (Model_Currency::GetBaseCurrency()) currency_is_base = Model_Currency::GetBaseCurrency()->CURRENCYID == currencyID;

        if (skip_unused && !(Model_Account::is_used(currency) || currency_is_base)) continue;

        wxVector<wxVariant> data;
        data.push_back(wxVariant(baseCurrencyID == currencyID));
        data.push_back(wxVariant(currency.CURRENCY_SYMBOL));
        data.push_back(wxVariant(currency.CURRENCYNAME));
        data.push_back(wxVariant(wxString()<<Model_CurrencyHistory::getLastRate(currencyID)));
        currencyListBox_->AppendItem(data, (wxUIntPtr)currencyID);
        if (selectedIndex_ == currencyListBox_->GetItemCount() - 1)
        {
            currencyListBox_->SelectRow(selectedIndex_);
            itemButtonEdit_->Enable();
            m_currency_id = currencyID;
        }
        if (m_currency_id == currencyID)
        {
            selectedIndex_ = currencyListBox_->GetItemCount() - 1;
            currencyListBox_->SelectRow(selectedIndex_);
            itemButtonEdit_->Enable();
        }
    }

    m_button_download_history->Enable(false);
    //Ensure that the selected item is visible.
    wxDataViewItem item(currencyListBox_->GetCurrentItem());
    currencyListBox_->EnsureVisible(item);
}

void mmMainCurrencyDialog::CreateControls()
{
    wxBoxSizer* mainBoxSizer = new wxBoxSizer(wxHORIZONTAL);
    this->SetSizer(mainBoxSizer);

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
    mainBoxSizer->Add(itemBoxSizer2, g_flagsExpand);

    wxBoxSizer* itemBoxSizer22 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer2->Add(itemBoxSizer22, wxSizerFlags(g_flagsExpand).Proportion(0));

    wxBitmapButton* update_button = new wxBitmapButton(this
        , wxID_STATIC, mmBitmap(png::CURRATES));
    itemBoxSizer22->Add(update_button, g_flagsH);
    update_button->Connect(wxID_STATIC, wxEVT_COMMAND_BUTTON_CLICKED
        , wxCommandEventHandler(mmMainCurrencyDialog::OnOnlineUpdateCurRate), nullptr, this);
    update_button->SetToolTip(_("Online update currency rate"));
    itemBoxSizer22->AddSpacer(4);

    itemBoxSizer22->Add(new wxStaticText(this, wxID_STATIC
        , _("Online Update")), g_flagsH);

    itemBoxSizer22->AddSpacer(15);
    cbShowAll_ = new wxCheckBox(this, wxID_SELECTALL, _("Show All"), wxDefaultPosition
        , wxDefaultSize, wxCHK_2STATE);
    cbShowAll_->SetToolTip(_("Show all even the unused currencies"));
    cbShowAll_->Connect(wxID_SELECTALL, wxEVT_COMMAND_CHECKBOX_CLICKED,
        wxCommandEventHandler(mmMainCurrencyDialog::OnShowHiddenChbClick), nullptr, this);

    itemBoxSizer22->Add(cbShowAll_, g_flagsH);

    wxBoxSizer* itemBoxSizer3 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer2->Add(itemBoxSizer3, g_flagsExpand);

    //TODO:provide proper style and fix validator (does not working)
    currencyListBox_ = new wxDataViewListCtrl( this
        , wxID_ANY, wxDefaultPosition, wxSize(-1, 200)/*, wxDV_HORIZ_RULES, mmDoubleValidator(4)*/);

    currencyListBox_->AppendToggleColumn(ColName_[CURR_BASE], wxDATAVIEW_CELL_INERT, 30);
    currencyListBox_->AppendTextColumn(ColName_[CURR_SYMBOL], wxDATAVIEW_CELL_INERT, 60
        , wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE);
    currencyListBox_->AppendTextColumn(ColName_[CURR_NAME], wxDATAVIEW_CELL_INERT, 170
        , wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE);
    currencyListBox_->AppendTextColumn(ColName_[BASE_RATE], wxDATAVIEW_CELL_EDITABLE, 80);

    itemBoxSizer3->Add(currencyListBox_, g_flagsExpand);

    wxPanel* buttonsPanel = new wxPanel(this, wxID_ANY);
    itemBoxSizer2->Add(buttonsPanel, wxSizerFlags(g_flagsV).Center());

    wxBoxSizer* buttonsSizer = new wxBoxSizer(wxVERTICAL);
    buttonsPanel->SetSizer(buttonsSizer);
    wxStdDialogButtonSizer* itemBoxSizer66 = new wxStdDialogButtonSizer;
    buttonsSizer->Add(itemBoxSizer66);

    wxButton* itemButton7 = new wxButton(buttonsPanel, wxID_ADD, _("&Add "));
    itemBoxSizer66->Add(itemButton7, g_flagsH);

    itemButtonEdit_ = new wxButton(buttonsPanel, wxID_EDIT, _("&Edit "));
    itemBoxSizer66->Add(itemButtonEdit_, g_flagsH);
    itemButtonEdit_->Disable();

    itemButtonDelete_ = new wxButton(buttonsPanel, wxID_REMOVE, _("&Remove "));
    itemBoxSizer66->Add(itemButtonDelete_, g_flagsH);
    itemButtonDelete_->Disable();

    wxStdDialogButtonSizer* itemBoxSizer9 = new wxStdDialogButtonSizer;
    buttonsSizer->Add(itemBoxSizer9, wxSizerFlags(g_flagsExpand).Border(wxALL, 0));

    wxButton* itemButtonSelect = new wxButton(buttonsPanel, wxID_SELECTALL, _("&Select"));
    itemBoxSizer9->Add(itemButtonSelect, wxSizerFlags(g_flagsExpand).Proportion(4));
    //itemButtonSelect->SetToolTip(_("Select the currently selected currency as the selected currency for the account"));

    if (bEnableSelect_ == false)
        itemButtonSelect->Disable();

    //Some interfaces has no any close buttons, it may confuse user. Cancel button added
    wxButton* itemCancelButton = new wxButton(buttonsPanel, wxID_CANCEL, wxGetTranslation(g_CloseLabel));
    itemBoxSizer9->Add(itemCancelButton, g_flagsH);
    itemCancelButton->SetFocus();

    //History Panel
    wxBoxSizer* rightBoxSizer = new wxBoxSizer(wxVERTICAL);
    mainBoxSizer->Add(rightBoxSizer, g_flagsExpand);

    historyStaticBox_ = new wxStaticBox(this, wxID_ANY, _("Currency History Options"));

    wxStaticBoxSizer* historyStaticBox_Sizer = new wxStaticBoxSizer(historyStaticBox_, wxVERTICAL);
    rightBoxSizer->Add(historyStaticBox_Sizer, g_flagsExpand);

    valueListBox_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 150)
        , wxLC_REPORT);
    historyStaticBox_Sizer->Add(valueListBox_, g_flagsExpand);

    wxListItem col0, col1, col2;
    // Add first column
    col0.SetId(0);
    col0.SetText(_("Date"));
    col0.SetWidth(90);
    valueListBox_->InsertColumn(0, col0);

    // Add second column
    col1.SetId(1);
    col1.SetText(_("Value"));
    col1.SetWidth(100);
    valueListBox_->InsertColumn(1, col1);

    // Add third column
    col2.SetId(2);
    col2.SetText(_("Diff."));
    col2.SetWidth(90);
    valueListBox_->InsertColumn(2, col2);

    //History Buttons
    wxPanel* values_panel = new wxPanel(this, wxID_ANY);
    historyStaticBox_Sizer->Add(values_panel, wxSizerFlags(g_flagsV).Centre());
    wxStdDialogButtonSizer*  values_sizer = new wxStdDialogButtonSizer;
    values_panel->SetSizer(values_sizer);


    values_sizer->Add(new wxStaticText(values_panel, wxID_STATIC, _("Date")), g_flagsH);

    valueDatePicker_ = new wxDatePickerCtrl(values_panel, wxID_ANY, wxDefaultDateTime,
        wxDefaultPosition, wxSize(120, -1), wxDP_DROPDOWN | wxDP_SHOWCENTURY);
    values_sizer->Add(valueDatePicker_, g_flagsH);
    valueDatePicker_->SetToolTip(_("Specify the date of currency value"));

    values_sizer->Add(new wxStaticText(values_panel, wxID_STATIC, _("Value")), g_flagsH);

    valueTextBox_ = new mmTextCtrl(values_panel, wxID_ANY, wxGetEmptyString()
        , wxDefaultPosition, wxSize(120, -1), wxALIGN_RIGHT | wxTE_PROCESS_ENTER
        , mmCalcValidator());
    valueTextBox_->SetToolTip(_("Enter the currency value"));
    values_sizer->Add(valueTextBox_, g_flagsH);

    wxPanel* buttons_panel = new wxPanel(this, wxID_ANY);
    historyStaticBox_Sizer->Add(buttons_panel, wxSizerFlags(g_flagsV).Centre());
    wxStdDialogButtonSizer*  buttons_sizer = new wxStdDialogButtonSizer;
    buttons_panel->SetSizer(buttons_sizer);

    m_button_download_history = new wxBitmapButton(buttons_panel, HISTORY_UPDATE, mmBitmap(png::CURRATES));
    m_button_download_history->SetToolTip(_("Download Currency Values history"));
    historyButtonAdd_ = new wxButton(buttons_panel, HISTORY_ADD, _("&Add / Update ")
        , wxDefaultPosition, wxSize(-1, m_button_download_history->GetSize().GetY()));
    historyButtonAdd_->SetToolTip(_("Add Currency Values to history"));
    historyButtonDelete_ = new wxButton(buttons_panel, HISTORY_DELETE, _("&Delete ")
        , wxDefaultPosition, wxSize(-1, m_button_download_history->GetSize().GetY()));
    historyButtonDelete_->SetToolTip(_("Delete selected Currency Values"));

    wxBitmapButton* buttonDelUnusedHistory = new wxBitmapButton(buttons_panel
        , HISTORY_DELUNUSED, mmBitmap(png::VOID_STAT));
    buttonDelUnusedHistory->SetToolTip(_("Delete Currency Values history for unused currencies"));
    buttons_sizer->Add(m_button_download_history, g_flagsH);
    buttons_sizer->Add(historyButtonAdd_, g_flagsH);
    buttons_sizer->Add(historyButtonDelete_, g_flagsH);
    buttons_sizer->Add(buttonDelUnusedHistory, g_flagsH);

    this->SetMinSize(wxSize(800,550));
    this->Fit();
}

void mmMainCurrencyDialog::OnBtnAdd(wxCommandEvent& /*event*/)
{
    mmCurrencyDialog dlg(this, 0);
    if (dlg.ShowModal() == wxID_OK)
    {
        m_currency_id = dlg.getCurrencyID();
        selectedIndex_ = -1;
    }

    fillControls();
}

void mmMainCurrencyDialog::OnBtnEdit(wxCommandEvent& /*event*/)
{
    Model_Currency::Data *currency = Model_Currency::instance().get(m_currency_id);
    if (currency)
        mmCurrencyDialog(this, currency).ShowModal();
    fillControls();
}

void mmMainCurrencyDialog::OnBtnSelect(wxCommandEvent& /*event*/)
{
    if (selectedIndex_ > -1) EndModal(wxID_OK);
}

void mmMainCurrencyDialog::OnBtnDelete(wxCommandEvent& /*event*/)
{
    if (selectedIndex_ < 0) return;

    Model_Currency::Data* currency = Model_Currency::instance().get(m_currency_id);
    if (!currency) return;
    if (wxMessageBox(_("Do you really want to delete the selected Currency?")
        , _("Currency Dialog")
        , wxYES_NO | wxNO_DEFAULT | wxICON_ERROR) == wxYES)
    {
        Model_Currency::instance().remove(m_currency_id);
        selectedIndex_ = -1;
        m_currency_id = -1;
        fillControls();
    }
}

bool mmMainCurrencyDialog::Execute(wxWindow* parent, int& currencyID)
{
    bool result = false;

    mmMainCurrencyDialog dlg(parent, currencyID);
    if (dlg.ShowModal() == wxID_OK)
    {
        currencyID = dlg.m_currency_id;
        result = true;
    }
    dlg.Destroy();

    return result;
}

bool mmMainCurrencyDialog::Execute(int& currencyID)
{
    bool result = false;

    mmMainCurrencyDialog dlg(NULL, currencyID);
    dlg.m_static_dialog = true;
    dlg.SetTitle(_("Base Currency Selection"));
    dlg.valueListBox_->Enable(false);
    dlg.valueTextBox_->Enable(false);
    dlg.valueDatePicker_->Enable(false);
    dlg.historyButtonAdd_->Enable(false);
    dlg.historyButtonDelete_->Enable(false);

    if (dlg.ShowModal() == wxID_OK)
    {
        currencyID = dlg.m_currency_id;
        result = true;
    }
    dlg.Destroy();

    return result;
}

void mmMainCurrencyDialog::OnListItemSelected(wxDataViewEvent& event)
{
    wxDataViewItem item = event.GetItem();
    selectedIndex_ = currencyListBox_->ItemToRow(item);

    wxString currName = "";
    if (selectedIndex_ >= 0)
    {
        m_currency_id = (int) currencyListBox_->GetItemData(item);
        Model_Currency::Data* currency = Model_Currency::instance().get(m_currency_id);
        if (currency)
        {
            currName = currency->CURRENCYNAME;
            itemButtonEdit_->Enable();
            wxString base_currency_symbol; 
            Model_Currency::GetBaseCurrencySymbol(base_currency_symbol);
            m_button_download_history->Enable(currency->CURRENCY_SYMBOL != base_currency_symbol);
        }
    }

    if (!bEnableSelect_)    // prevent user deleting currencies when editing accounts.
    {
        int baseCurrencyID = Option::instance().BaseCurrency();
        Model_Currency::Data* currency = Model_Currency::instance().get(m_currency_id);
        if (currency)
        {
            itemButtonDelete_->Enable(!Model_Account::is_used(currency) && m_currency_id != baseCurrencyID);
            currName = currency->CURRENCYNAME;
        }
    }

    historyStaticBox_->SetLabel(wxString::Format(_("Currency History Options: %s "), currName));

    valueDatePicker_->SetValue(wxDateTime::Today());
    valueTextBox_->SetValue(wxEmptyString);
    ShowCurrencyHistory();
}

void mmMainCurrencyDialog::OnListItemActivated(wxDataViewEvent& /* event */)
{
    wxCommandEvent evt(wxEVT_COMMAND_BUTTON_CLICKED, wxID_ANY);

    if (bEnableSelect_)
        OnBtnSelect(evt);
    else
        OnBtnEdit(evt);
}

bool mmMainCurrencyDialog::OnlineUpdateCurRate(int curr_id, bool hide)
{
    wxString msg = wxEmptyString;
	bool ok = GetOnlineRates(msg, curr_id);
	if (ok)
	{
		if (!hide)
		{
			wxMessageDialog msgDlg(this, msg, _("Currency rates have been updated"));
			msgDlg.ShowModal();
		}
		fillControls();
		ShowCurrencyHistory();
	}
	else
	{
		wxMessageDialog msgDlg(this, msg, _("Error"), wxOK | wxICON_ERROR);
		msgDlg.ShowModal();
	}
	return ok;
}

void mmMainCurrencyDialog::OnOnlineUpdateCurRate(wxCommandEvent& /*event*/)
{
    if (!m_static_dialog)    //Abort when trying to set base currency
    {
        OnlineUpdateCurRate(-1, false);
    }
}

void mmMainCurrencyDialog::OnMenuSelected(wxCommandEvent& event)
{
    switch (event.GetId())
    {
        case MENU_ITEM1:
        {
            if (!SetBaseCurrency(m_currency_id))
                mmErrorDialogs::MessageError(this
                    , _("Unable to update history currency rates. Please update them manually!")
                    , _("Currency history error"));
            itemButtonDelete_->Disable();
            fillControls();
            ShowCurrencyHistory();
        } break;
        case MENU_ITEM2: OnlineUpdateCurRate(m_currency_id, false); break;
        case MENU_ITEM3:
        {
            wxCommandEvent e;
            this->OnBtnEdit(e);
        } break;
    } 
}

void mmMainCurrencyDialog::OnItemRightClick(wxDataViewEvent& event)
{
    // disable this function when setting base currency
    if (m_static_dialog) return;

    wxCommandEvent ev(wxEVT_COMMAND_MENU_SELECTED, wxID_ANY);
    ev.SetEventObject( this );

    wxMenu* mainMenu = new wxMenu;
    mainMenu->Append(new wxMenuItem(mainMenu, MENU_ITEM1, _("Set as Base Currency")));
    mainMenu->Append(new wxMenuItem(mainMenu, MENU_ITEM2, _("Online Update Currency Rate")));
    mainMenu->Append(new wxMenuItem(mainMenu, MENU_ITEM3, _("&Edit ")));

    int baseCurrencyID = Option::instance().BaseCurrency();
    if (baseCurrencyID == m_currency_id)
        mainMenu->Enable(MENU_ITEM1, false);

    PopupMenu(mainMenu);
    delete mainMenu;
    event.Skip();
}

void mmMainCurrencyDialog::OnShowHiddenChbClick(wxCommandEvent& event)
{
    Model_Infotable::instance().Set("SHOW_HIDDEN_CURRENCIES", cbShowAll_->IsChecked());
    fillControls();
}

void mmMainCurrencyDialog::OnCancel(wxCommandEvent& /*event*/)
{
    EndModal(wxID_CANCEL);
}

void mmMainCurrencyDialog::ShowCurrencyHistory()
{
    if (m_static_dialog) return;    //Abort when trying to set base currency
    valueListBox_->DeleteAllItems();

    int baseCurrencyID = Option::instance().BaseCurrency();
    if (m_currency_id <= 0 || m_currency_id == baseCurrencyID)
    {
        historyButtonAdd_->Disable();
        historyButtonDelete_->Disable();
        return;
    }
    else
    {
        historyButtonAdd_->Enable();
        historyButtonDelete_->Enable();
    }      

    Model_CurrencyHistory::Data_Set histData = Model_CurrencyHistory::instance().find(Model_CurrencyHistory::CURRENCYID(m_currency_id));
    std::stable_sort(histData.begin(), histData.end(), SorterByCURRDATE());
    std::reverse(histData.begin(), histData.end());
    if (!histData.empty())
    {
        int idx = 0;
        for (const auto &d : histData)
        {
            wxListItem item;
            item.SetId(idx);
            item.SetData(d.CURRHISTID);
            valueListBox_->InsertItem(item);
            const wxString dispAmount = wxString::Format(wxT("%f"), d.CURRVALUE);
            valueListBox_->SetItem(idx, 0, mmGetDateForDisplay(d.CURRDATE));
            valueListBox_->SetItem(idx, 1, dispAmount);
            const wxString& priceAmount = wxEmptyString;
            valueListBox_->SetItem(idx, 2, priceAmount);
            idx++;
        }
        valueListBox_->RefreshItems(0, --idx);
    }
}

void mmMainCurrencyDialog::OnHistoryAdd(wxCommandEvent& /*event*/)
{
    if (m_currency_id <= 0) return;

    wxString listStr;
    wxDateTime dt;
    double dPrice = 0.0;
    Model_Currency::Data* currency = Model_Currency::instance().get(m_currency_id);
    wxString currentPriceStr = valueTextBox_->GetValue().Trim();
    if (!Model_Currency::fromString(currentPriceStr, dPrice, currency) || (dPrice < 0.0))
        return;
    Model_CurrencyHistory::instance().addUpdate(m_currency_id, valueDatePicker_->GetValue(), dPrice, Model_CurrencyHistory::MANUAL);

    fillControls();
    ShowCurrencyHistory();
}

void mmMainCurrencyDialog::OnHistoryDelete(wxCommandEvent& /*event*/)
{
    if (valueListBox_->GetSelectedItemCount() <= 0) return;

    long item = -1;
    Model_CurrencyHistory::instance().Savepoint();
    for (;;)
    {
        item = valueListBox_->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (item == -1) break;
        Model_CurrencyHistory::instance().remove((int)valueListBox_->GetItemData(item));
    }
    Model_CurrencyHistory::instance().ReleaseSavepoint();

    fillControls();
    ShowCurrencyHistory();
}

void mmMainCurrencyDialog::OnHistoryUpdate(wxCommandEvent& /*event*/)
{
    if (m_static_dialog) return;    //Abort when trying to set base currency
	Model_Currency::Data* CurrentCurrency = Model_Currency::instance().get(m_currency_id);
	if (!CurrentCurrency) return; //No currency selected

    wxString base_currency_symbol;
	if (!Model_Currency::GetBaseCurrencySymbol(base_currency_symbol))
	{
		return mmErrorDialogs::MessageError(this
			, _("Could not find base currency symbol!")
			, _("Currency history error"));
	}
	
	wxString msg;
	std::map<wxDateTime, double> historical_rates;
	bool UpdStatus = GetOnlineHistory(historical_rates
		, CurrentCurrency->CURRENCY_SYMBOL.Upper(), msg);

    if (!UpdStatus)
    {
        return mmErrorDialogs::MessageError(this
			, wxString::Format(_("Unable to download %s currency rates")
				, CurrentCurrency->CURRENCY_SYMBOL)
			, _("Currency history error"));
    }

    const int msgResult = wxMessageBox(_("Do you want to add also dates without any transaction?")
        , _("Currency Dialog"), wxYES_NO | wxNO_DEFAULT | wxICON_ERROR);
    bool CheckDate;
    if (msgResult == wxYES || msgResult == wxNO)
        CheckDate = msgResult == wxNO;
    else
        return;


	bool Found = !historical_rates.empty();

	if (Found)
	{
		Model_CurrencyHistory::instance().Savepoint();
		if (CheckDate)
		{
			const std::map<wxDateTime, int> DatesList = Model_Currency::DateUsed(m_currency_id);
			for (const auto entry : DatesList)
			{
				wxLogDebug("%s %s",  entry.first.FormatISODate(), entry.first.FormatISOTime());
				if (historical_rates.find(entry.first) != historical_rates.end())
				{
					const wxDate date = entry.first;
					wxLogDebug("%s", date.FormatISODate());
					Model_CurrencyHistory::instance().addUpdate(m_currency_id
						, date, historical_rates[date], Model_CurrencyHistory::ONLINE);
				}

			}
		}
		else
		{
			for (auto &entry : historical_rates)
			{
				Model_CurrencyHistory::instance().addUpdate(m_currency_id
					, entry.first, entry.second, Model_CurrencyHistory::ONLINE);
			}
		}
		Model_CurrencyHistory::instance().ReleaseSavepoint();

		fillControls();
		ShowCurrencyHistory();
    }
    else
        mmErrorDialogs::MessageError(this
            , wxString::Format(_("Unable to download history for symbol %s. History rates not available!")
                , CurrentCurrency->CURRENCY_SYMBOL)
            , _("Currency history error"));
}

void mmMainCurrencyDialog::OnHistoryDeleteUnused(wxCommandEvent& /*event*/)
{
    if (m_static_dialog) return;    //Abort when trying to set base currency
    Model_CurrencyHistory::instance().Savepoint();
    auto currencies = Model_Currency::instance().all();
    for (auto &currency : currencies)
    {
        if (!Model_Account::is_used(currency))
        {
            for (const auto& r : Model_CurrencyHistory::instance().find(Model_CurrencyHistory::CURRENCYID(currency.CURRENCYID)))
                Model_CurrencyHistory::instance().remove(r.id());
        }
        else
        {
            std::map<wxDateTime, int> DatesList = Model_Currency::DateUsed(currency.CURRENCYID);
            wxDateTime CurrDate;
            for (const auto& r : Model_CurrencyHistory::instance().find(Model_CurrencyHistory::CURRENCYID(currency.CURRENCYID)))
            {
                CurrDate.ParseDate(r.CURRDATE);
                if (DatesList.find(CurrDate) == DatesList.end())
                    Model_CurrencyHistory::instance().remove(r.id());
            }
        }
    }
    Model_CurrencyHistory::instance().ReleaseSavepoint();

    fillControls();
    ShowCurrencyHistory();
}

void mmMainCurrencyDialog::OnHistorySelected(wxListEvent& event)
{
    long selectedIndex = event.GetIndex();
    long histId = valueListBox_->GetItemData(selectedIndex);
    Model_CurrencyHistory::Data *histData = Model_CurrencyHistory::instance().get(histId);

    if (histData->CURRHISTID > 0)
    {
        valueDatePicker_->SetValue(Model_CurrencyHistory::CURRDATE(*histData));
        valueTextBox_->SetValue(wxString::Format("%f", histData->CURRVALUE));
    }
}

void mmMainCurrencyDialog::OnHistoryDeselected(wxListEvent& event)
{
    valueDatePicker_->SetValue(wxDateTime::Today());
    valueTextBox_->SetValue(wxEmptyString);
}

bool mmMainCurrencyDialog::SetBaseCurrency(int& baseCurrencyID)
{
    int baseCurrencyOLD = Option::instance().BaseCurrency();
    if (baseCurrencyID == baseCurrencyOLD)
        return true;

    Option::instance().BaseCurrency(baseCurrencyID);

    Model_Currency::Data* BaseCurrency = Model_Currency::instance().get(baseCurrencyID);
    BaseCurrency->BASECONVRATE = 1;
    Model_Currency::instance().save(BaseCurrency);

    Model_Currency::Data* BaseCurrencyOLD = Model_Currency::instance().get(baseCurrencyOLD);
    BaseCurrencyOLD->BASECONVRATE = 1;
    Model_Currency::instance().save(BaseCurrencyOLD);

    Model_CurrencyHistory::instance().Savepoint();
    for (const auto& r : Model_CurrencyHistory::instance().find(Model_CurrencyHistory::CURRENCYID(baseCurrencyID)))
        Model_CurrencyHistory::instance().remove(r.id());
    Model_CurrencyHistory::instance().ReleaseSavepoint();

    fillControls();
    ShowCurrencyHistory();
    return true;
}


bool mmMainCurrencyDialog::GetOnlineRates(wxString &msg, int curr_id)
{
    wxString base_currency_symbol;
    
	if (!Model_Currency::GetBaseCurrencySymbol(base_currency_symbol))
	{
		msg = _("Could not find base currency symbol!");
		return false;
	}

	wxString sOutput;
	wxString buffer = wxEmptyString;

	auto currencies = Model_Currency::instance()
		.find(Model_Currency::CURRENCY_SYMBOL(base_currency_symbol, NOT_EQUAL));
	for (const auto &currency : currencies)
	{
		if (curr_id > 0 && currency.CURRENCYID != curr_id) continue;
		const auto symbol = currency.CURRENCY_SYMBOL.Upper();
		if (!symbol.IsEmpty()) 
            buffer += wxString::Format("%s%s=X,", symbol ,base_currency_symbol);
	}
	if (buffer.Right(1).Contains(",")) buffer.RemoveLast(1);

	const auto URL = wxString::Format(mmex::weblink::YahooQuotes
		, buffer);

	auto err_code = site_content(URL, sOutput);
	if (err_code != CURLE_OK)
	{
		msg = sOutput;
		return false;
	}

	std::wstringstream ss;
	ss << sOutput.ToStdWstring();
	json::Object o;
	json::Reader::Read(o, ss);

	json::Object r = o[L"quoteResponse"];
	//TODO:     "error": null
	//bool error = json::Boolean(r[L"error"]);
	//if (!error) return false;

	json::Array e = r[L"result"];

    const auto crypto_marker = wxString::Format("%s/%s"
        , base_currency_symbol, base_currency_symbol);
    std::map<wxString, double> currency_data;
    std::vector<wxString> crypto_currencies;

    for (int i = 0; i < e.Size(); i++) 
    {
		const json::Object symbol = e[i];
		auto currency_symbol = wxString(json::String(symbol[L"symbol"]).Value());
        const auto short_name = wxString(json::String(symbol[L"shortName"]).Value());

        wxRegEx pattern("^(...)...=X$");
        if (pattern.Matches(currency_symbol))
        {
            currency_symbol = pattern.GetMatch(currency_symbol, 1);
            if (short_name != crypto_marker)
            {
                const auto price = symbol[L"regularMarketPrice"];
                const auto rate = json::Number(price).Value();
                wxLogDebug("item: %d %s %.2f", i, currency_symbol, rate);
                currency_data[currency_symbol] = (rate <= 0 ? 1 : rate);
            }
            else
            {
                wxLogDebug("Crypto: %s", currency_symbol);
                crypto_currencies.push_back(currency_symbol);
            }
        }
	}

	msg = _("Currency rates have been updated");
	msg << "\n\n";

	const wxDateTime today = wxDateTime::Today();
	Model_CurrencyHistory::instance().Savepoint();
	for (auto &currency : currencies)
	{
		bool unused_currency = !cbShowAll_->IsChecked() && !Model_Account::is_used(currency);
		if (unused_currency) continue;

		const wxString currency_symbol = currency.CURRENCY_SYMBOL.Upper();
		if (!currency_symbol.IsEmpty())
		{
			if (currency_data.find(currency_symbol) != currency_data.end())
			{
				if (Model_Account::is_used(currency)) //ignore unused currencies to have compact preview
				{
					msg << wxString::Format("%s\t: %0.6f -> %0.6f\n"
						, currency_symbol, currency.BASECONVRATE, currency_data[currency_symbol]);
				}
				currency.BASECONVRATE = currency_data[currency_symbol];

				Model_CurrencyHistory::instance().addUpdate(currency.CURRENCYID,
					today, currency.BASECONVRATE, Model_CurrencyHistory::ONLINE);
				Model_Currency::instance().save(&currency);
			}
		}
	}

	//Model_Currency::instance().save(currencies);  BASECONVRATE IS FIXED AND USED IF HISTORY NOT FOUND 
	Model_CurrencyHistory::instance().ReleaseSavepoint();

	return true;
}

bool mmMainCurrencyDialog::GetOnlineHistory(std::map<wxDateTime, double> &historical_rates
	, const wxString &symbol, wxString &msg)
{
    wxString base_currency_symbol;

	if (!Model_Currency::GetBaseCurrencySymbol(base_currency_symbol))
	{
		msg = _("Could not find base currency symbol!");
		return false;
	}

	const wxString URL = wxString::Format(mmex::weblink::YahooQuotesHistory
		, wxString::Format("%s%s=X", symbol, base_currency_symbol)
		, "1y", "1d"); //TODO: aks range and interval
	
	wxString sOutput;
	auto err_code = site_content(URL, sOutput);
	if (err_code != CURLE_OK)
	{
		msg = sOutput;
		return false;
	}

	std::wstringstream ss;
	ss << sOutput.ToStdWstring();
	json::Object o;
	json::Reader::Read(o, ss);

	json::Object r = o[L"chart"]; 
	//TODO:
	//bool error = (json::Null(r[L"error"]) == NULL);
	//if (!error) return false;

	json::Object result = r[L"result"][0]; //meta timestamp indicators
	json::Array timestamp = result[L"timestamp"];
	json::Object indicators = result[L"indicators"];
	json::Array quote = indicators[L"quote"][0][L"close"];

	wxASSERT(timestamp.Size() == quote.Size());

	std::map<wxDate, double> history;
	const wxDateTime today = wxDateTime::Today().GetDateOnly();
	wxDateTime first_date = today;
	double first_price = 0;
	for (int i = 0; i < timestamp.Size(); ++i)
	{
		const wxDateTime time = wxDateTime((time_t)json::Number(timestamp[i]).Value()).GetDateOnly();
		double rate = json::Number(quote[i]).Value();
		history[time] = rate;
		if (first_date > time)
		{
			first_date = time;
			first_price = rate;
		}
	}

	double closed_price = first_price;
	for (wxDateTime i = first_date; i < today; i.Add(wxDateSpan::Days(1)))
	{
		wxLogDebug("Date: %s %s",i.FormatISODate(), i.FormatISOTime());
		double rate = closed_price;
		if (history.find(i) != history.end()) {
			rate = history[i];
			closed_price = rate;
		}
		historical_rates[i] = rate;
	}

	wxLogDebug("Date: %s %s Today: %s %s" 
		, first_date.FormatISODate(), first_date.FormatISOTime()
		, today.FormatISODate(), today.FormatISOTime());
	return true;
}