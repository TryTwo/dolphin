// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class CodeWidget;
class QCheckBox;
class QLineEdit;
class QComboBox;
class QListWidget;
class QListWidgetItem;

struct TCodeTrace
{
  u32 address = 0;
  std::string instruction = "";
  std::string reg0 = "";
  // u32 reg0_val;
  std::string reg1 = "";
  // u32 reg1_val;
  std::string reg2 = "";
  // u32 reg2_val;
  u32 memory_dest = 0;
  bool is_store = false;
  bool is_load = false;
  // bool is_fpr = false;
};

struct TTraceOutput
{
  u32 address;
  std::string instruction;
};

class CodeTraceDialog : public QDialog
{
  Q_OBJECT
public:
  explicit CodeTraceDialog(CodeWidget* parent);
  ~CodeTraceDialog();

  void reject() override;

private:
  void CreateWidgets();

  void TraceCode();

  void IterateForwards();

  void IterateBackwards();

  void ConnectWidgets();

  void RunTrace();
  void DisplayTrace();

  void UpdateBreakpoints();
  void InfoDisp();

  QListWidget* m_trace_output;
  QLineEdit* m_trace_target;
  QComboBox* m_bp1;
  QComboBox* m_bp2;
  QCheckBox* m_backtrace;
  QCheckBox* m_verbose;
  QCheckBox* m_clear_on_loop;
  QPushButton* m_run_trace;
  QPushButton* m_reprocess;
  CodeWidget* m_parent;

  QLineEdit* m_sizes;

  std::vector<TCodeTrace> CodeTrace;
  std::vector<TTraceOutput> TraceOutput;
  std::vector<std::string> RegTrack;
  std::vector<u32> MemTrack;
  std::string testtest;
  // Make modifiable?
  const size_t m_max_code_trace = 20000;
  const size_t m_max_trace_output = 200;
  QListWidgetItem* m_error_msg;
  uint pass = 1;
};
