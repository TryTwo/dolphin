// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;
class IniFile;

namespace Gecko
{
class GeckoCode
{
public:
  GeckoCode() : enabled(false) {}

  struct Code
  {
    u32 address = 0;
    u32 data = 0;
    std::string original_line;
  };

  std::vector<Code> codes;
  std::string name, creator;
  std::vector<std::string> notes;

  bool enabled;
  bool user_defined;

  bool Exist(u32 address, u32 data) const;
};

bool operator==(const GeckoCode& lhs, const GeckoCode& rhs);
bool operator!=(const GeckoCode& lhs, const GeckoCode& rhs);

bool operator==(const GeckoCode::Code& lhs, const GeckoCode::Code& rhs);
bool operator!=(const GeckoCode::Code& lhs, const GeckoCode::Code& rhs);

// Installation address for codehandler.bin in the Game's RAM
constexpr u32 INSTALLER_BASE_ADDRESS = 0x80001800;
constexpr u32 INSTALLER_END_ADDRESS = 0x80003000;
constexpr u32 ENTRY_POINT = INSTALLER_BASE_ADDRESS + 0xA8;
// If the GCT is max-length then this is the second word of the End code (0xF0000000 0x00000000)
// If the table is shorter than the max-length then this address is unused / contains trash.
constexpr u32 HLE_TRAMPOLINE_ADDRESS = INSTALLER_END_ADDRESS - 4;

void SetActiveCodes(const std::vector<GeckoCode>& gcodes);
void RunCodeHandler();
void Shutdown();
void DoState(PointerWrap&);

}  // namespace Gecko
