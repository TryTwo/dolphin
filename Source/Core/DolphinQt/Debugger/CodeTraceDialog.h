// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class CodeWidget;
class QCheckBox;
class QLineEdit;
class QLabel;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QSpinBox;

struct CodeTrace
{
  u32 address = 0;
  std::string instruction = "";
  std::string reg0 = "";
  std::string reg1 = "";
  std::string reg2 = "";
  u32 memory_dest = 0;
  bool is_store = false;
  bool is_load = false;
};

struct TraceOutput
{
  u32 address;
  u32 mem_addr = 0;
  std::string instruction;
};

class CodeTraceDialog : public QDialog
{
  Q_OBJECT
public:
  explicit CodeTraceDialog(CodeWidget* parent);

  void reject() override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void ClearAll();
  void OnRecordTrace(bool checked);
  void SaveInstruction();
  void ForwardTrace();
  void Backtrace();
  void CodePath();
  void DisplayTrace();
  void OnChangeRange();
  bool UpdateIterator(std::vector<CodeTrace>::iterator& begin_itr,
                      std::vector<CodeTrace>::iterator& end_itr);
  bool CompareInstruction(std::string instruction, std::vector<std::string> instruction_type);
  void UpdateBreakpoints();
  void InfoDisp();

  void OnContextMenu();

  QListWidget* m_output_list;
  QLineEdit* m_trace_target;
  QComboBox* m_bp1;
  QComboBox* m_bp2;
  QCheckBox* m_backtrace;
  QCheckBox* m_verbose;
  QCheckBox* m_clear_on_loop;
  QCheckBox* m_change_range;
  QPushButton* m_reprocess;
  QLabel* m_record_limit_label;
  QLabel* m_results_limit_label;
  QSpinBox* m_record_limit_input;
  QSpinBox* m_results_limit_input;

  QPushButton* m_record_trace;
  CodeWidget* m_parent;

  std::vector<CodeTrace> m_code_trace;
  std::vector<TraceOutput> m_trace_out;
  std::vector<std::string> m_reg;
  std::vector<u32> m_mem;

  size_t m_record_limit = 150000;
  size_t m_results_limit = 2000;
  QString m_error_msg;

  bool m_recording = false;
};
