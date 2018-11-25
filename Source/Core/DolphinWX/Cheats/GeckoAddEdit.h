// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>

#include "Core/GeckoCode.h"

class wxTextCtrl;
using namespace Gecko;

class GeckoAddEdit final : public wxDialog
{
public:
  GeckoAddEdit(GeckoCode* code, wxWindow* parent, wxWindowID id = wxID_ANY,
               const wxString& title = _("Edit Gecko Code"), const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE);

 void SetGeckoCode(Gecko::GeckoCode* code);

 private:
  struct GEntry
  {
    GEntry() {}
    GEntry(u32 _addr, u32 _value) : cmd_addr(_addr), value(_value) {}
    u32 cmd_addr;
    u32 value;
  };
  void CreateGUI();
  void SaveCheatData(wxCommandEvent& event);
  std::string namet;
  GeckoCode* m_gcode;
  GeckoCode* m_code;
  wxTextCtrl* m_txt_cheat_name;
  wxTextCtrl* m_cheat_codes;
};
