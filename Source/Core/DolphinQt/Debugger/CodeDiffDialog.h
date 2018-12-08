// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class CodeWidget;
class QCheckBox;
class QLabel;
class QListWidget;

namespace Profiler
{
struct ProfileStats;
}

class CodeDiffDialog : public QDialog
{
  Q_OBJECT
public:
  explicit CodeDiffDialog(CodeWidget* parent);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void TTest();

  // void TraceCode();

  // void IterateForwards();

  // void IterateBackwards();

  // void RunTrace();
  // void DisplayTrace();

  // void UpdateBreakpoints();

  QListWidget* m_diff_output;
  QLabel* m_exclude_amt;
  QLabel* m_current_amt;
  QLabel* m_include_amt;
  QPushButton* m_exclude_btn;
  QPushButton* m_include_btn;
  QPushButton* m_record_btn;
  CodeWidget* m_parent;

  QLabel* m_sizes;

  std::vector<std::string> RegTrack;
  std::vector<u32> MemTrack;
  std::string testtest;
  unsigned int pass = 1;
};
