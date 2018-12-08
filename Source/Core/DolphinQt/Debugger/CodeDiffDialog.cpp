// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/CodeDiffDialog.h"

#include <chrono>
#include <cinttypes>
#include <regex>
#include <vector>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>

#include "Common/StringUtil.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/Profiler.h"
#include "DolphinQt/Host.h"

#include "DolphinQt/Debugger/CodeWidget.h"

CodeDiffDialog::CodeDiffDialog(CodeWidget* parent) : QDialog(parent), m_parent(parent)
{
  setWindowTitle(tr("Diff"));
  CreateWidgets();
  ConnectWidgets();
  // UpdateBreakpoints();
  JitInterface::ProfilingState state = JitInterface::ProfilingState::Enabled;
  JitInterface::SetProfilingState(state);
}

void CodeDiffDialog::CreateWidgets()
{
  this->resize(882, 619);
  auto* btns_layout = new QHBoxLayout;
  m_exclude_btn = new QPushButton(tr("Code hasn't run"));
  m_include_btn = new QPushButton(tr("Code has run"));
  m_record_btn = new QPushButton(tr("Record functions"));

  btns_layout->addWidget(m_exclude_btn);
  btns_layout->addWidget(m_include_btn);
  btns_layout->addWidget(m_record_btn);

  auto* labels_layout = new QHBoxLayout;
  m_exclude_amt = new QLabel(tr("Excluded"));
  m_current_amt = new QLabel(tr("Current"));
  m_include_amt = new QLabel(tr("Included"));

  labels_layout->addWidget(m_exclude_amt);
  labels_layout->addWidget(m_current_amt);
  labels_layout->addWidget(m_include_amt);

  m_diff_output = new QListWidget();
  m_diff_output->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto* layout = new QVBoxLayout();
  layout->addLayout(btns_layout);
  layout->addLayout(labels_layout);
  layout->addWidget(m_diff_output);

  setLayout(layout);
}

void CodeDiffDialog::ConnectWidgets()
{
  connect(m_record_btn, &QPushButton::pressed, this, &CodeDiffDialog::TTest);
}

void CodeDiffDialog::TTest()
{
  Profiler::ProfileStats prof_stats;
  JitInterface::ProfilingState state = JitInterface::ProfilingState::Enabled;
  JitInterface::SetProfilingState(state);
  JitInterface::GetProfileResults(&prof_stats);
  for (auto& stat : prof_stats.block_stats)
  {
    std::string name = g_symbolDB.GetDescription(stat.addr);
    new QListWidgetItem(QString::fromStdString(StringFromFormat("%08x\t%s\t%" PRIu64, stat.addr,
                                                                name.c_str(), stat.run_count)),
                        m_diff_output);
  }
}
//
// void CodeDiffDialog::ConnectWidgets()
//{
//  connect(m_parent, &CodeWidget::BreakpointsChanged, this, &CodeDiffDialog::UpdateBreakpoints);
//  connect(m_run_Diff, &QPushButton::pressed, this, &CodeDiffDialog::RunDiff);
//  connect(m_reprocess, &QPushButton::pressed, this, &CodeDiffDialog::DisplayDiff);
//}
//
// void CodeDiffDialog::RunDiff()
//{
//  CodeDiff.clear();
//
//  u32 start_bp;
//  u32 end_bp;
//
//  if (!CPU::IsStepping())
//    return;
//
//  CPU::PauseAndLock(true, false);
//  PowerPC::breakpoints.ClearAllTemporary();
//
//  // Keep stepping until the next return instruction or timeout after five seconds
//  using clock = std::chrono::steady_clock;
//  clock::time_point timeout = clock::now() + std::chrono::seconds(5);
//  PowerPC::CoreMode old_mode = PowerPC::GetMode();
//  PowerPC::SetMode(PowerPC::CoreMode::Interpreter);
//
//  //// test bad values
//  // if (m_bp1->currentIndex() == -1)
//  //  start_bp = m_bp1->currentText().toUInt();
//  // else
//  //  start_bp = m_bp1->currentData().toUInt();
//
//  // if (m_bp2->currentIndex() == -1)
//  //  end_bp = m_bp2->currentText().toUInt();
//  // else
//  //  end_bp = m_bp2->currentData().toUInt();
//  start_bp = m_bp1->currentData().toUInt();
//  end_bp = m_bp2->currentData().toUInt();
//  // Loop until either the current instruction is a return instruction with no Link flag
//  // or a breakpoint is detected so it can step at the breakpoint. If the PC is currently
//  // on a breakpoint, skip it.
//  UGeckoInstruction inst = PowerPC::HostRead_Instruction(PC);
//  do
//  {
//    DiffCode();
//    PowerPC::SingleStep();
//
//    if (PC == start_bp && m_clear_on_loop->isChecked())
//      CodeDiff.clear();
//
//  } while (clock::now() < timeout && !PowerPC::breakpoints.IsAddressBreakPoint(PC) &&
//           CodeDiff.size() <= 20000);
//
//  // const Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(m_code_view->GetAddress());
//  PowerPC::SetMode(old_mode);
//  CPU::PauseAndLock(false, false);
//
//  // Check if needed
//  emit Host::GetInstance()->UpdateDisasmDialog();
//
//  CodeDiffDialog::DisplayDiff();
//  // if (PowerPC::breakpoints.IsAddressBreakPoint(PC))
//  //  Core::DisplayMessage(tr("Breakpoint encountered! Step out aborted.").toStdString(), 2000);
//  // else if (clock::now() >= timeout)
//  //  Core::DisplayMessage(tr("Step out timed out!").toStdString(), 2000);
//  // else
//  //  Core::DisplayMessage(tr("Step out successful!").toStdString(), 2000);
//  // auto* item = new QListWidgetItem(tr("Diff timed out, stopped before end breakpoint"));
//  // auto* item = new QListWidgetItem(tr("Max Diff size reached, stopped before end breakpoint"));
//  // m_Diff_output->addltem(item);
//}
//
// void CodeDiffDialog::DiffCode()
//{
//  if (CodeDiff.size() >= 10000)
//    return;
//  TCodeDiff tmp_Diff;
//  std::string tmp = PowerPC::debug_interface.Disassemble(PC);
//  std::regex replace_sp("\\W(sp)");
//  std::regex replace_rtoc("(rtoc)");
//  tmp = std::regex_replace(tmp, replace_sp, "r1");
//  tmp = std::regex_replace(tmp, replace_sp, "r2");
//  tmp_Diff.instruction = tmp;
//  tmp_Diff.address = PC;
//
//  // Pull all register numbers out and store them.
//  std::regex reg("([rfp]\\d+)[^r^f]*(?:([rf]\\d+))?[^r^f\\D]*(?:([rf]\\d+))?");
//  std::smatch match;
//
//  if (std::regex_search(tmp, match, reg))
//  {
//    tmp_Diff.reg0 = match.str(1);
//    if (match[2].matched)
//      tmp_Diff.reg1 = match.str(2);
//    if (match[3].matched)
//      tmp_Diff.reg2 = match.str(3);
//
//    if (match.str(1) != "r")
//      tmp_Diff.is_fpr = true;
//
//    // Get Memory Destination if load/store.
//    if (tmp.compare(0, 2, "st") == 0 || tmp.compare(0, 5, "psq_s") == 0)
//    {
//      tmp_Diff.memory_dest = PowerPC::debug_interface.GetMemoryAddressFromInstruction(tmp);
//      tmp_Diff.is_store = true;
//    }
//    else if ((tmp.compare(0, 1, "l") == 0 && tmp.compare(1, 1, "i") != 0) ||
//             tmp.compare(0, 5, "psq_l") == 0)
//    {
//      tmp_Diff.memory_dest = PowerPC::debug_interface.GetMemoryAddressFromInstruction(tmp);
//      tmp_Diff.is_load = true;
//    }
//  }
//
//  CodeDiff.push_back(tmp_Diff);
//}
//
// void CodeDiffDialog::IterateForwards()
//{
//  std::vector<std::string> exclude = {"dc", "ic", "mt", "c"};
//  TDiffOutput tempout;
//  tempout.instruction;
//  for (auto instr = CodeDiff.begin(); instr != CodeDiff.end(); instr--)
//  {
//    // Not an instruction we care about.
//    if (instr->reg0.empty())
//      continue;
//
//    // test instr reg2 empty
//    auto itR = std::find(RegTrack.begin(), RegTrack.end(), instr->reg0);
//    auto itM = std::find(MemTrack.begin(), MemTrack.end(), instr->memory_dest);
//    const bool match_reg12 =
//        (std::find(RegTrack.begin(), RegTrack.end(), instr->reg1) != RegTrack.end() ||
//         std::find(RegTrack.begin(), RegTrack.end(), instr->reg2) != RegTrack.end());
//    const bool match_reg0 = (itR != RegTrack.end());
//
//    int test;
//    if (*itM == 0)
//      test = 1;
//    if (m_verbose->isChecked() && (match_reg12 || match_reg0 || itM != MemTrack.end()))
//    {
//      tempout.instruction = instr->instruction;
//      tempout.address = instr->address;
//      DiffOutput.push_back(tempout);
//    }
//    // or goto
//    bool cont = false;
//
//    for (auto& s : exclude)
//    {
//      if (instr->instruction.compare(0, s.length(), s) == 0)
//        cont = true;
//    }
//
//    if (cont)
//      continue;
//
//    // Save/Load
//    if (instr->memory_dest)
//    {
//      // If using tracked memory. Add register to tracked if Load. Remove tracked memory if
//      // pverwrotten.
//      if (itM != MemTrack.end())
//      {
//        if (instr->is_load && !match_reg0)
//          RegTrack.push_back(instr->reg0);
//        else if (instr->is_store && !match_reg0)
//          MemTrack.erase(itM);
//      }
//      else if (instr->is_store && match_reg0)
//      {
//        // If load/store but not using tracked memory & if storing tracked register, track memory
//        // location too.
//        MemTrack.push_back(instr->memory_dest);
//      }
//    }
//    else
//    {
//      // Other instructions
//      // Skip if no matches. Happens most often.
//      if (!match_reg0 && !match_reg12)
//        continue;
//      // If tracked register data is being stored in a new register, save new register.
//      else if (match_reg12 && !match_reg0)
//        RegTrack.push_back(instr->reg0);
//      // If tracked register is overwritten, stop tracking.
//      else if (match_reg0 && !match_reg12)
//        RegTrack.erase(itR);
//    }
//
//    if ((RegTrack.empty() && MemTrack.empty()) || DiffOutput.size() > 200)
//      break;
//  }
//}
//
// void CodeDiffDialog::IterateBackwards()
//{
//  std::vector<std::string> exclude = {"dc", "ic", "mt", "c"};  // mf
//  pass = 1;
//  TDiffOutput tempout;
//
//  for (auto instr = CodeDiff.end(); instr != CodeDiff.begin(); instr--)
//  {
//    // Not an instruction we care about
//    testtest = instr->instruction;
//    pass++;
//
//    if (instr->reg0.empty())
//      continue;
//
//    auto itR = std::find(RegTrack.begin(), RegTrack.end(), instr->reg0);
//    auto itM = std::find(MemTrack.begin(), MemTrack.end(), instr->memory_dest);
//    const bool match_reg1 =
//        std::find(RegTrack.begin(), RegTrack.end(), instr->reg1) != RegTrack.end();
//    const bool match_reg2 =
//        std::find(RegTrack.begin(), RegTrack.end(), instr->reg2) != RegTrack.end();
//    const bool match_reg0 = (itR != RegTrack.end());
//
//    // Output stuff like compares if they contain a tracked register
//    if (m_verbose && (match_reg1 || match_reg2 || match_reg0 || itM != MemTrack.end()))
//    {
//      tempout.instruction = instr->instruction;
//      tempout.address = instr->address;
//      DiffOutput.push_back(tempout);
//    }
//
//    // or goto
//
//    // Exclude a few instruction types, such as compare
//    bool cont = false;
//    for (auto& s : exclude)
//    {
//      if (instr->instruction.compare(0, s.length(), s) == 0)
//        cont = true;
//    }
//
//    if (cont)
//      continue;
//
//    // Save/Load
//    if (instr->memory_dest)
//    {
//      // BackDiff: what wrote to tracked Memory & remove memory track. Load doesn't point to where
//      // it came from. Else if: what loaded to tracked register & remove register from track.
//      if (itM != MemTrack.end())
//      {
//        // if (instr->is_load && !match_reg0)
//        //  sidenote;
//        if (instr->is_store && !match_reg0)
//          RegTrack.push_back(instr->reg0);
//      }
//      else if (instr->is_load && match_reg0)
//      {
//        MemTrack.push_back(instr->memory_dest);
//        RegTrack.erase(itR);
//      }
//    }
//    else
//    {
//      // Other instructions
//      // Skip if we aren't watching output register. Happens most often.
//      // Else: Erase tracked register and save what wrote to it.
//      if (!match_reg0)
//        continue;
//      else if (!match_reg1 && !match_reg2)
//        RegTrack.erase(itR);
//
//      // If tracked register is written, track r1 / r2.
//      if (!match_reg1 && !instr->reg1.empty())
//        RegTrack.push_back(instr->reg1);
//      if (!match_reg2 && !instr->reg2.empty())
//        RegTrack.push_back(instr->reg2);
//    }
//
//    // Stop if we run out of things to track
//    if ((RegTrack.empty() && MemTrack.empty()) || DiffOutput.size() > 200)
//      break;
//  }
//}
//
// void CodeDiffDialog::DisplayDiff()
//{
//  DiffOutput.clear();
//  RegTrack.clear();
//  MemTrack.clear();
//  m_Diff_output->clear();
//
//  // DO account for memory values and SP/RTOC
//  RegTrack.push_back(m_Diff_target->text().toStdString());
//
//  // Add BP changes to change codeDiff substr?
//  if (m_backDiff->isChecked())
//    IterateBackwards();
//  else
//    IterateForwards();
//
//  // m_sizes->setPlaceholderText(QString::number(CodeDiff.size()) +
//  //                            QStringLiteral("  Output:  ") +
//  //                            QString::number(DiffOutput.size()));
//
//  m_sizes->setPlaceholderText(QStringLiteral("Diff: %1, Proc: %2")
//                                  .arg(QString::number(CodeDiff.size()))
//                                  .arg(QString::number(DiffOutput.size())));
//
//  auto* item =
//      new QListWidgetItem(QStringLiteral("This is a ts test est test etst etst\nTestestestestes "
//                                         "test\ntesteste testest \n testestest"));
//  m_Diff_output->addItem(item);
//
//  for (auto out = DiffOutput.begin(); out != DiffOutput.end(); out++)
//  {
//    auto* item2 = new QListWidgetItem(QString::fromStdString(
//        StringFromFormat("%08x : %s", out->address, out->instruction.c_str())));
//    m_Diff_output->addItem(item2);
//  }
//}
//
// void CodeDiffDialog::UpdateBreakpoints()
//{
//  // May need better method of clear
//  m_bp1->clear();
//  m_bp2->clear();
//
//  auto bp_vec = PowerPC::breakpoints.GetBreakPoints();
//
//  for (auto& i : bp_vec)
//  {
//    std::string instr = PowerPC::debug_interface.Disassemble(i.address);
//    m_bp1->addItem(QStringLiteral("%1 : %2")
//                       .arg(i.address, 8, 16, QLatin1Char('0'))
//                       .arg(QString::fromStdString(instr)),
//                   i.address);
//    m_bp2->addItem(QStringLiteral("%1 : %2")
//                       .arg(i.address, 8, 16, QLatin1Char('0'))
//                       .arg(QString::fromStdString(instr)),
//                   i.address);
//  }
//
//  m_bp2->setCurrentIndex(1);
//  m_bp1->setEditText(QStringLiteral("PC: %1").arg(PC, 8, 16, QLatin1Char('0')));
//  m_bp1->setItemData(-1, PC);
//}

void ConnectWidgets()
{
}
