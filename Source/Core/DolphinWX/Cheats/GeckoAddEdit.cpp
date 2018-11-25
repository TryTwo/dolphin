// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include <utility>
#include <vector>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/gbsizer.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/stockitem.h>
#include <wx/textctrl.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "DolphinWX/Cheats/GeckoAddEdit.h"
#include "DolphinWX/WxUtils.h"

using namespace Gecko;

GeckoAddEdit::GeckoAddEdit(GeckoCode* code, wxWindow* parent, wxWindowID id, const wxString& title,
                           const wxPoint& position, const wxSize& size, long style)
    : wxDialog(parent, id, title, position, size, style), m_code(std::move(code))
{
  CreateGUI();
}

void GeckoAddEdit::CreateGUI()
{
  const int space10 = FromDIP(10);
  const int space5 = FromDIP(5);

  wxBoxSizer* sEditCheat = new wxBoxSizer(wxVERTICAL);
  wxStaticBoxSizer* sbEntry = new wxStaticBoxSizer(wxVERTICAL, this, _("Cheat Code"));
  wxGridBagSizer* sgEntry = new wxGridBagSizer(space10, space10);

  wxStaticText* lbl_cheat_name = new wxStaticText(sbEntry->GetStaticBox(), wxID_ANY, _("Name:"));
  wxStaticText* lbl_cheat_codes = new wxStaticText(sbEntry->GetStaticBox(), wxID_ANY, _("Code:"));

  m_txt_cheat_name = new wxTextCtrl(sbEntry->GetStaticBox(), wxID_ANY, wxEmptyString);

  m_cheat_codes =
      new wxTextCtrl(sbEntry->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
                     wxDLG_UNIT(this, wxSize(240, 128)), wxTE_MULTILINE);

  {
    wxFont font{m_cheat_codes->GetFont()};
    font.SetFamily(wxFONTFAMILY_TELETYPE);
#ifdef _WIN32
    // Windows uses Courier New for monospace even though there are better fonts.
    font.SetFaceName("Consolas");
#endif
    m_cheat_codes->SetFont(font);
  }

  sgEntry->Add(lbl_cheat_name, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER);
  sgEntry->Add(lbl_cheat_codes, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER);
  sgEntry->Add(m_txt_cheat_name, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND);
  sgEntry->Add(m_cheat_codes, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND);
  sgEntry->AddGrowableCol(1);
  sgEntry->AddGrowableRow(1);
  sbEntry->Add(sgEntry, 1, wxEXPAND | wxALL, space5);

  // OS X UX: ID_NO becomes "Don't Save" when paired with wxID_SAVE
  wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
  buttons->AddButton(new wxButton(this, wxID_SAVE));
  buttons->AddButton(new wxButton(this, wxID_NO, wxGetStockLabel(wxID_CANCEL)));
  buttons->Realize();

  sEditCheat->AddSpacer(space5);
  sEditCheat->Add(sbEntry, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sEditCheat->AddSpacer(space10);
  sEditCheat->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sEditCheat->AddSpacer(space5);

  Bind(wxEVT_BUTTON, &GeckoAddEdit::SaveCheatData, this, wxID_SAVE);

  SetEscapeId(wxID_NO);
  SetAffirmativeId(wxID_SAVE);
  SetSizerAndFit(sEditCheat);
}

void GeckoAddEdit::SetGeckoCode(Gecko::GeckoCode* code)
{
  m_txt_cheat_name->SetValue(StrToWxStr(code->name));

  for (const auto& c : code->codes)
  m_cheat_codes->AppendText(wxString::Format("%08X %08X\n", c.address, c.data));

  m_gcode = code;
}

void GeckoAddEdit::SaveCheatData(wxCommandEvent& WXUNUSED(event))
{
  std::vector<GeckoCode::Code> entries;

  // Split the entered cheat into lines.
  const std::vector<std::string> input_lines =
      SplitString(WxStrToStr(m_cheat_codes->GetValue()), '\n');

  for (size_t i = 0; i < input_lines.size(); i++)
  {
    // Make sure to ignore unneeded whitespace characters.
    std::string line_str = StripSpaces(input_lines[i]);

    if (line_str.empty())
      continue;

    // Let's parse the current line.
    std::vector<std::string> pieces = SplitString(line_str, ' ');
    u32 address = 0;
    u32 data = 0;

    bool good = pieces.size() == 2;

    if (good)
    address = std::stoul(pieces[0], nullptr, 16);

    if (good)
    data = std::stoul(pieces[1], nullptr, 16);

    if (!good)
    {
      WxUtils::ShowErrorDialog(_("Incorrect code size"));
      return;
    }

    Gecko::GeckoCode::Code c;
    c.address = address;
    c.data = data;
    c.original_line = line_str;

    entries.push_back(c);
  }

  // There's no point creating a code with no content.
  if (entries.empty())
  {
    WxUtils::ShowErrorDialog(_("No code"));
    return;
  }

  namet = WxStrToStr(m_txt_cheat_name->GetValue());
  m_gcode->name = namet;
  m_gcode->codes = std::move(entries);
  m_gcode->user_defined = true;

    AcceptAndClose();
}
