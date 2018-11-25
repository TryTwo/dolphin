// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HLE/HLE_Misc.h"

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/GeckoCode.h"
#include "Core/HW/CPU.h"
#include "Core/Host.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

namespace HLE_Misc
{
// If you just want to kill a function, one of the three following are usually appropriate.
// According to the PPC ABI, the return value is always in r3.
void UnimplementedFunction()
{
  NPC = LR;
}

void HBReload()
{
  // There isn't much we can do. Just stop cleanly.
  CPU::Break();
  Host_Message(HostMessageID::WMUserStop);
}

// Because Dolphin messes around with the CPU state instead of patching the game binary, we
// need a way to branch into the GCH from an arbitrary PC address. Branching is easy, returning
// back is the hard part. This HLE function acts as a trampoline that restores the original LR, SP,
// and PC before the magic, invisible BL instruction happened.
void GeckoReturnTrampoline()
{
  // Stack frame is built in GeckoCode.cpp, Gecko::RunCodeHandler.
  u32 SP = GPR(1);
  GPR(1) = PowerPC::HostRead_U32(SP + 8);
  NPC = PowerPC::HostRead_U32(SP + 12);
  LR = PowerPC::HostRead_U32(SP + 16);
  PowerPC::ExpandCR(PowerPC::HostRead_U32(SP + 20));
  for (int i = 0; i < 14; ++i)
  {
    riPS0(i) = PowerPC::HostRead_U64(SP + 24 + 2 * i * sizeof(u64));
    riPS1(i) = PowerPC::HostRead_U64(SP + 24 + (2 * i + 1) * sizeof(u64));
  }
}
}
