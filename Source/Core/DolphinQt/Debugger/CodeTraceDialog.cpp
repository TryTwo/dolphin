// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/CodeTraceDialog.h"

#include <chrono>
#include <regex>
#include <vector>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>

#include "Common/StringUtil.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt/Host.h"

#include "DolphinQt/Debugger/CodeWidget.h"

CodeTraceDialog::CodeTraceDialog(CodeWidget* parent) : QDialog(parent), m_parent(parent)
{
  setWindowTitle(tr("Trace"));
  CreateWidgets();
  ConnectWidgets();
  UpdateBreakpoints();
}

CodeTraceDialog::~CodeTraceDialog()
{
  TraceOutput.clear();
}

void CodeTraceDialog::reject()
{
  // CodeTrace.clear();
  std::vector<TCodeTrace>().swap(CodeTrace);
  TraceOutput.clear();
  RegTrack.clear();
  MemTrack.clear();
  m_trace_output->clear();
  InfoDisp();
  QDialog::reject();
  ;
}

void CodeTraceDialog::CreateWidgets()
{
  this->resize(882, 619);
  auto* input_layout = new QHBoxLayout;
  m_trace_target = new QLineEdit();
  m_trace_target->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_trace_target->setPlaceholderText(tr("Register or Mem Address"));
  m_bp1 = new QComboBox();
  m_bp1->setEditable(true);
  m_bp1->setCurrentText(tr("Start BP or address"));
  m_bp2 = new QComboBox();
  m_bp2->setEditable(true);
  m_bp2->setCurrentText(tr("Stop BP or address"));

  input_layout->addWidget(m_trace_target);
  input_layout->addWidget(m_bp1);
  input_layout->addWidget(m_bp2);

  auto* boxes_layout = new QHBoxLayout;
  m_backtrace = new QCheckBox(tr("BackTrace"));
  m_verbose = new QCheckBox(tr("Verbose"));
  m_clear_on_loop = new QCheckBox(tr("Reset if breakpoint loops"));

  m_sizes = new QLineEdit();

  m_reprocess = new QPushButton(tr("Reprocess"));
  m_run_trace = new QPushButton(tr("Run Trace"));

  boxes_layout->addWidget(m_backtrace);
  boxes_layout->addWidget(m_verbose);
  boxes_layout->addWidget(m_clear_on_loop);
  boxes_layout->addWidget(m_reprocess);

  boxes_layout->addWidget(m_sizes);

  boxes_layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Maximum));
  boxes_layout->addWidget(m_run_trace);

  m_trace_output = new QListWidget();
  m_trace_output->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  // m_trace_output->setGeometry(QRect(20, 100, 841, 471));

  auto* layout = new QVBoxLayout();
  layout->addLayout(input_layout);
  layout->addLayout(boxes_layout);
  layout->addWidget(m_trace_output);

  InfoDisp();

  setLayout(layout);

  m_error_msg = NULL;
}

void CodeTraceDialog::ConnectWidgets()
{
  connect(m_parent, &CodeWidget::BreakpointsChanged, this, &CodeTraceDialog::UpdateBreakpoints);
  connect(m_run_trace, &QPushButton::pressed, this, &CodeTraceDialog::RunTrace);
  connect(m_reprocess, &QPushButton::pressed, this, &CodeTraceDialog::DisplayTrace);
  //  connect(this, &CodeTraceDialog::closeEvent, this, &CodeTraceDialog::OnClose);
}

void CodeTraceDialog::RunTrace()
{
  CodeTrace.clear();
  CodeTrace.reserve(m_max_code_trace);
  u32 start_bp;
  u32 end_bp;

  if (!CPU::IsStepping())
    return;

  CPU::PauseAndLock(true, false);
  PowerPC::breakpoints.ClearAllTemporary();

  // Keep stepping until the next return instruction or timeout after five seconds
  using clock = std::chrono::steady_clock;
  clock::time_point timeout = clock::now() + std::chrono::seconds(5);
  PowerPC::CoreMode old_mode = PowerPC::GetMode();
  PowerPC::SetMode(PowerPC::CoreMode::Interpreter);

  //// test bad values
  // if (m_bp1->currentIndex() == -1)
  //  start_bp = m_bp1->currentText().toUInt();
  // else
  //  start_bp = m_bp1->currentData().toUInt();

  // if (m_bp2->currentIndex() == -1)
  //  end_bp = m_bp2->currentText().toUInt();
  // else
  //  end_bp = m_bp2->currentData().toUInt();
  start_bp = m_bp1->currentData().toUInt();
  end_bp = m_bp2->currentData().toUInt();
  // Loop until either the current instruction is a return instruction with no Link flag
  // or a breakpoint is detected so it can step at the breakpoint. If the PC is currently
  // on a breakpoint, skip it.
  UGeckoInstruction inst = PowerPC::HostRead_Instruction(PC);
  do
  {
    TraceCode();
    PowerPC::SingleStep();

    if (PC == start_bp && m_clear_on_loop->isChecked())
      CodeTrace.clear();

  } while (clock::now() < timeout && !PowerPC::breakpoints.IsAddressBreakPoint(PC) &&
           CodeTrace.size() <= m_max_code_trace);

  // const Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(m_code_view->GetAddress());
  PowerPC::SetMode(old_mode);
  CPU::PauseAndLock(false, false);

  // Is this needed?
  emit Host::GetInstance()->UpdateDisasmDialog();

  // Add output to Display

  CodeTraceDialog::DisplayTrace();
}

void CodeTraceDialog::TraceCode()
{
  if (CodeTrace.size() >= m_max_code_trace)
    return;
  TCodeTrace tmp_trace;
  std::string tmp = PowerPC::debug_interface.Disassemble(PC);
  std::regex replace_sp("\\W(sp)");
  std::regex replace_rtoc("(rtoc)");
  tmp = std::regex_replace(tmp, replace_sp, "r1");
  tmp = std::regex_replace(tmp, replace_sp, "r2");
  tmp_trace.instruction = tmp;
  tmp_trace.address = PC;

  // Pull all register numbers out and store them.
  std::regex reg("\\W([rfp]\\d+)[^r^f]*(?:([rf]\\d+))?[^r^f\\D]*(?:([rf]\\d+))?");
  std::smatch match;

  if (std::regex_search(tmp, match, reg))
  {
    tmp_trace.reg0 = match.str(1);
    if (match[2].matched)
      tmp_trace.reg1 = match.str(2);
    if (match[3].matched)
      tmp_trace.reg2 = match.str(3);

    // if (match.str(1) != "r")
    //  tmp_trace.is_fpr = true;

    // Get Memory Destination if load/store.
    if (tmp.compare(0, 2, "st") == 0 || tmp.compare(0, 5, "psq_s") == 0)
    {
      tmp_trace.memory_dest = PowerPC::debug_interface.GetMemoryAddressFromInstruction(tmp);
      tmp_trace.is_store = true;
    }
    else if ((tmp.compare(0, 1, "l") == 0 && tmp.compare(1, 1, "i") != 0) ||
             tmp.compare(0, 5, "psq_l") == 0)
    {
      tmp_trace.memory_dest = PowerPC::debug_interface.GetMemoryAddressFromInstruction(tmp);
      tmp_trace.is_load = true;
    }
  }

  CodeTrace.push_back(tmp_trace);
}

void CodeTraceDialog::IterateForwards()
{
  std::vector<std::string> exclude{"dc", "ic", "mt", "c"};
  TTraceOutput tempout;

  for (auto instr = CodeTrace.begin(); instr != CodeTrace.end(); instr++)
  {
    // Not an instruction we care about.
    if (instr->reg0.empty())
      continue;

    // test instr reg2 empty
    auto itR = std::find(RegTrack.begin(), RegTrack.end(), instr->reg0);
    auto itM = std::find(MemTrack.begin(), MemTrack.end(), instr->memory_dest);
    const bool match_reg12 =
        (std::find(RegTrack.begin(), RegTrack.end(), instr->reg1) != RegTrack.end() ||
         std::find(RegTrack.begin(), RegTrack.end(), instr->reg2) != RegTrack.end());
    const bool match_reg0 = (itR != RegTrack.end());

    int test;
    if (*itM == 0)
      test = 1;
    if (m_verbose->isChecked() && (match_reg12 || match_reg0 || itM != MemTrack.end()))
    {
      tempout.instruction = instr->instruction;
      tempout.address = instr->address;
      TraceOutput.push_back(tempout);
    }

    // or goto
    bool cont = false;

    for (auto& s : exclude)
    {
      if (instr->instruction.compare(0, s.length(), s) == 0)
        cont = true;
    }

    if (cont)
      continue;

    // Save/Load
    if (instr->memory_dest)
    {
      // If using tracked memory. Add register to tracked if Load. Remove tracked memory if
      // pverwrotten.
      if (itM != MemTrack.end())
      {
        if (instr->is_load && !match_reg0)
          RegTrack.push_back(instr->reg0);
        else if (instr->is_store && !match_reg0)
          MemTrack.erase(itM);
      }
      else if (instr->is_store && match_reg0)
      {
        // If load/store but not using tracked memory & if storing tracked register, track memory
        // location too.
        MemTrack.push_back(instr->memory_dest);
      }
    }
    else
    {
      // Other instructions
      // Skip if no matches. Happens most often.
      if (!match_reg0 && !match_reg12)
        continue;
      // If tracked register data is being stored in a new register, save new register.
      else if (match_reg12 && !match_reg0)
        RegTrack.push_back(instr->reg0);
      // If tracked register is overwritten, stop tracking.
      else if (match_reg0 && !match_reg12)
        RegTrack.erase(itR);
    }

    if ((RegTrack.empty() && MemTrack.empty()) || TraceOutput.size() >= m_max_trace_output)
      break;
  }
}

void CodeTraceDialog::IterateBackwards()
{
  std::vector<std::string> exclude{"dc", "ic", "mt", "c"};  // mf
  pass = 1;
  TTraceOutput tempout;

  for (auto instr = CodeTrace.end(); instr != CodeTrace.begin(); instr--)
  {
    // Not an instruction we care about
    testtest = instr->instruction;
    pass++;

    if (instr->reg0.empty())
      continue;

    auto itR = std::find(RegTrack.begin(), RegTrack.end(), instr->reg0);
    auto itM = std::find(MemTrack.begin(), MemTrack.end(), instr->memory_dest);
    const bool match_reg1 =
        std::find(RegTrack.begin(), RegTrack.end(), instr->reg1) != RegTrack.end();
    const bool match_reg2 =
        std::find(RegTrack.begin(), RegTrack.end(), instr->reg2) != RegTrack.end();
    const bool match_reg0 = (itR != RegTrack.end());

    // Output stuff like compares if they contain a tracked register
    if (m_verbose && (match_reg1 || match_reg2 || match_reg0 || itM != MemTrack.end()))
    {
      tempout.instruction = instr->instruction;
      tempout.address = instr->address;
      TraceOutput.push_back(tempout);
    }

    // or goto

    // Exclude a few instruction types, such as compare
    bool cont = false;
    for (auto& s : exclude)
    {
      if (instr->instruction.compare(0, s.length(), s) == 0)
        cont = true;
    }

    if (cont)
      continue;

    // Save/Load
    if (instr->memory_dest)
    {
      // Backtrace: what wrote to tracked Memory & remove memory track. Load doesn't point to where
      // it came from. Else if: what loaded to tracked register & remove register from track.
      if (itM != MemTrack.end())
      {
        // if (instr->is_load && !match_reg0)
        //  sidenote;
        if (instr->is_store && !match_reg0)
          RegTrack.push_back(instr->reg0);
      }
      else if (instr->is_load && match_reg0)
      {
        MemTrack.push_back(instr->memory_dest);
        RegTrack.erase(itR);
      }
    }
    else
    {
      // Other instructions
      // Skip if we aren't watching output register. Happens most often.
      // Else: Erase tracked register and save what wrote to it.
      if (!match_reg0)
        continue;
      else if (!match_reg1 && !match_reg2)
        RegTrack.erase(itR);

      // If tracked register is written, track r1 / r2.
      if (!match_reg1 && !instr->reg1.empty())
        RegTrack.push_back(instr->reg1);
      if (!match_reg2 && !instr->reg2.empty())
        RegTrack.push_back(instr->reg2);
    }

    // Stop if we run out of things to track
    if ((RegTrack.empty() && MemTrack.empty()) || TraceOutput.size() >= m_max_trace_output)
      break;
  }
}

void CodeTraceDialog::DisplayTrace()
{
  TraceOutput.clear();
  RegTrack.clear();
  MemTrack.clear();
  m_trace_output->clear();
  TraceOutput.reserve(m_max_trace_output);
  m_trace_output->addItem(m_error_msg);

  if (CodeTrace.size() >= m_max_code_trace)
    new QListWidgetItem(tr("Trace timed out, stopped before end breakpoint"), m_trace_output);

  // Setup tracking and determine if its a register or memory address.
  bool good;
  u32 mem_temp = m_trace_target->text().toUInt(&good, 16);

  if (good)
  {
    MemTrack.push_back(mem_temp);
  }
  else
  {
    QString reg_tmp = m_trace_target->text();
    reg_tmp.replace(QStringLiteral("sp"), QStringLiteral("r1"), Qt::CaseInsensitive);
    reg_tmp.replace(QStringLiteral("rtoc"), QStringLiteral("r2"), Qt::CaseInsensitive);
    RegTrack.push_back(reg_tmp.toStdString());
  }

  // Add BP changes to change codetrace substr?
  if (m_backtrace->isChecked())
    IterateBackwards();
  else
    IterateForwards();
  if (TraceOutput.size() >= m_max_trace_output)
    new QListWidgetItem(tr("Max trace size reached, stopped early"));

  m_sizes->setPlaceholderText(QStringLiteral("Trace: %1, Proc: %2")
                                  .arg(QString::number(CodeTrace.size()))
                                  .arg(QString::number(TraceOutput.size())));

  for (auto out = TraceOutput.begin(); out != TraceOutput.end(); out++)
  {
    new QListWidgetItem(QString::fromStdString(
                            StringFromFormat("%08x : %s", out->address, out->instruction.c_str())),
                        m_trace_output);
    // m_trace_output->addItem(item2);
  }
}

void CodeTraceDialog::UpdateBreakpoints()
{
  // May need better method of clear
  m_bp1->clear();
  m_bp2->clear();

  auto bp_vec = PowerPC::breakpoints.GetBreakPoints();

  for (auto& i : bp_vec)
  {
    std::string instr = PowerPC::debug_interface.Disassemble(i.address);
    m_bp1->addItem(QStringLiteral("%1 : %2")
                       .arg(i.address, 8, 16, QLatin1Char('0'))
                       .arg(QString::fromStdString(instr)),
                   i.address);
    m_bp2->addItem(QStringLiteral("%1 : %2")
                       .arg(i.address, 8, 16, QLatin1Char('0'))
                       .arg(QString::fromStdString(instr)),
                   i.address);
  }

  m_bp1->setEditText(QStringLiteral("PC: %1").arg(PC, 8, 16, QLatin1Char('0')));
  m_bp1->setItemData(-1, PC);

  m_bp2->setCurrentIndex(0);
  if (m_bp2->currentData() == m_bp1->currentData())
    m_bp2->setCurrentIndex(1);
}

void CodeTraceDialog::InfoDisp()
{
  new QListWidgetItem(
      QStringLiteral(
          "Used to track a register or memory address and its uses.\nInputs:\nRegister: Input "
          "example "
          "r5 or f31 or "
          "p2 or 80601234 for memory. Only takes one value at a time.\nStarting breakpoint should "
          "always be PC when running a trace. Can change when reprocessing.\nEnding breakpoint: "
          "Where "
          "the trace will stop. If backtracing, should be the line you want to backtrace "
          "from.\nBacktrace: A reverse trace that shows where a value came from, the first output "
          "line "
          "is the most recent use of tracked item.\n    Backtracing = off will show where a "
          "tracked "
          "item "
          "is going "
          "after the start breakpoint.\nVerbose: Will record all references to what is being "
          "tracked, rather than just where it is moving to/from.\nReset on Loop: Will clear the "
          "trace "
          "if starting breakpoint is looped through. Insuring only the final loop to the end "
          "breakpoint is recorded.\nReprocess: You don't have to run a trace multiple times if the "
          "first trace covered the area of code you need.\nYou can change any value or option and "
          "do a "
          "retrace. Changing the breakpoints will narrow the search and changing the second "
          "breakpoint "
          "will let you backtrace from a new location, if it was captured in the original trace. "),
      m_trace_output);
}

// Right click stuff. Delete?
// Error messages.

// r0 includes cr0
