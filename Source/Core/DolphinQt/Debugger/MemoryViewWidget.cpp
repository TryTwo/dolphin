// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/MemoryViewWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <qtimer.h>

#include <cctype>
#include <cmath>

#include "Core/Core.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt/Debugger/EditSymbolDialog.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

// "Most mouse types work in steps of 15 degrees, in which case the delta value is a multiple of
// 120; i.e., 120 units * 1/8 = 15 degrees." (http://doc.qt.io/qt-5/qwheelevent.html#angleDelta)
constexpr double SCROLL_FRACTION_DEGREES = 15.;

MemoryViewWidget::MemoryViewWidget(QWidget* parent) : QTableWidget(parent)
{
  horizontalHeader()->hide();
  verticalHeader()->hide();
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setShowGrid(false);
  // setStyleSheet(QStringLiteral("QTableView {selection-background-color: #0090FF;
  // selection-color:#FFFFFF}"));
  // setAlternatingRowColors(true);

  setFont(Settings::Instance().GetDebugFont());

  m_timer = new QTimer;
  m_timer->setInterval(600);
  m_auto_update_action = new QAction(tr("Auto update memory values (600ms)"), this);
  m_auto_update_action->setCheckable(true);

  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, &QWidget::setFont);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &MemoryViewWidget::Update);
  connect(this, &MemoryViewWidget::customContextMenuRequested, this,
          &MemoryViewWidget::OnContextMenu);
  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &MemoryViewWidget::Update);
  connect(m_auto_update_action, &QAction::toggled, [this](bool checked) {
    if (checked)
      m_timer->start();
    else
      m_timer->stop();
  });
  connect(m_timer, &QTimer::timeout, [this] {
    if (Core::GetState() == Core::State::Running)
      AutoUpdate();
  });

  // Update on stepping. Is there a better way than this?
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, [this] {
    if (Core::GetState() == Core::State::Paused)
      Update();
  });

  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, &MemoryViewWidget::Update);
  setContextMenuPolicy(Qt::CustomContextMenu);

  Update();
}

static int GetColumnCount(MemoryViewWidget::Type type)
{
  switch (type)
  {
  case MemoryViewWidget::Type::U32xASCII:
  case MemoryViewWidget::Type::U32xFloat32:
    return 2;
  case MemoryViewWidget::Type::U8:
    return 16;
  case MemoryViewWidget::Type::U16:
    return 8;
  case MemoryViewWidget::Type::U32:
  case MemoryViewWidget::Type::ASCII:
  case MemoryViewWidget::Type::Float32:
    return 4;
  default:
    return 0;
  }
}

void MemoryViewWidget::Update()
{
  if (m_updating)
    return;

  m_updating = true;

  clearSelection();

  setColumnCount(3 + GetColumnCount(m_type));

  if (rowCount() == 0)
    setRowCount(1);

  const QFontMetrics fm(Settings::Instance().GetDebugFont());
  const int fonth = fm.height();
  verticalHeader()->setDefaultSectionSize(fonth + 3);
  horizontalHeader()->setMinimumSectionSize(fonth + 3);

  // Calculate (roughly) how many rows will fit in our table
  int rows = std::round((height() / static_cast<float>(rowHeight(0))) - 0.25);
  setRowCount(rows);

  // Get target memory to tag it if it exists

  u32 address_align = m_address;

  if (m_align && ((m_address & 0xf) != 0))
  {
    address_align = m_address & 0xfffffff0;
  }

  for (int i = 0; i < rows; i++)
  {
    // Two column mode has rows increment by 0x4 instead of 0x10
    const u32 rowmod = ((GetColumnCount(m_type) == 2) ? 4 : 16);
    u32 addr = address_align - (rowCount() / 2) * rowmod + i * rowmod;

    auto* bp_item = new QTableWidgetItem;
    bp_item->setFlags(Qt::NoItemFlags);
    bp_item->setData(Qt::UserRole, addr);

    setItem(i, 0, bp_item);

    auto* addr_item = new QTableWidgetItem(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));

    addr_item->setData(Qt::UserRole, addr);
    addr_item->setFlags(Qt::ItemIsSelectable);

    setItem(i, 1, addr_item);

    // Don't update values unless game is started
    if ((Core::GetState() != Core::State::Paused && Core::GetState() != Core::State::Running) ||
        !PowerPC::HostIsRAMAddress(addr))
    {
      for (int c = 2; c < columnCount(); c++)
      {
        auto* item = new QTableWidgetItem(QStringLiteral("-"));
        item->setFlags(Qt::NoItemFlags);
        item->setData(Qt::UserRole, addr);

        setItem(i, c, item);
      }

      continue;
    }

    std::string desc;
    int color = 0xFFFFFF;
    const Common::Note* note = g_symbolDB.GetNoteFromAddr(addr);
    if (note == nullptr)
    {
      desc = PowerPC::debug_interface.GetDescription(addr);
    }
    else
    {
      color = PowerPC::debug_interface.GetNoteColor(addr);
      desc = note->name;
    }

    auto* description_item = new QTableWidgetItem(QString::fromStdString(desc));
    description_item->setBackground(QColor(color));

    description_item->setForeground(Qt::blue);
    description_item->setFlags(Qt::NoItemFlags);

    setItem(i, columnCount() - 1, description_item);

    bool row_breakpoint = true;
    const int columns = GetColumnCount(m_type);

    for (int c = 0; c < columns; c++)
    {
      auto* hex_item = new QTableWidgetItem;
      hex_item->setFlags(Qt::ItemIsSelectable);
      const u32 address = (columns == 2) ? addr : addr + c * (16 / columns);

      // Also change AutoUpdate color exclusions if target address color changes..
      if (address == m_target)
        hex_item->setBackground(QColor(220, 235, 235, 255));
      else if (PowerPC::memchecks.OverlapsMemcheck(address, 16 / ((columns == 2) ? 4 : columns)))
        hex_item->setBackgroundColor(Qt::red);
      else
      {
        // Color required for auto-update to see it as white.
        hex_item->setBackgroundColor(QColor(0xFFFFFF));
        row_breakpoint = false;
      }

      setItem(i, 2 + c, hex_item);

      if (PowerPC::HostIsRAMAddress(address))
        hex_item->setData(Qt::UserRole, address);
    }

    if (row_breakpoint)
    {
      bp_item->setData(
          Qt::DecorationRole,
          Resources::GetScaledThemeIcon("debugger_breakpoint").pixmap(QSize(fonth - 2, fonth - 2)));
    }
  }

  AutoUpdate();

  setColumnWidth(0, fonth + 3);
  for (int i = 1; i < columnCount(); i++)
  {
    resizeColumnToContents(i);
    // Add some extra spacing because the default width is too small in most cases
    setColumnWidth(i, columnWidth(i) * 1.1);
  }

  viewport()->update();
  update();
  m_updating = false;
}

void MemoryViewWidget::AutoUpdate()
{
  if (Core::GetState() != Core::State::Paused && Core::GetState() != Core::State::Running)
    return;

  u32 address_align = m_address;

  if (m_align && ((m_address & 0xf) != 0))
  {
    address_align = m_address & 0xfffffff0;
  }

  Core::RunAsCPUThread([&] {
    const int columns = GetColumnCount(m_type);

    for (int i = 0; i < rowCount(); i++)
    {
      const u32 rowmod = ((GetColumnCount(m_type) == 2) ? 4 : 16);
      const u32 addr = address_align - (rowCount() / 2) * rowmod + i * rowmod;

      auto update_values = [&](auto value_to_string) {
        for (int c = 0; c < GetColumnCount(m_type); c++)
        {
          auto* hex_item = item(i, 2 + c);
          const u32 address = (columns == 2) ? addr : addr + c * (16 / columns);

          if (PowerPC::HostIsRAMAddress(address))
          {
            QString value = value_to_string(address);

            if (columns == 2 && c == 0)
            {
              hex_item->setText(QStringLiteral("%1").arg(PowerPC::HostRead_U32(address), 8, 16,
                                                         QLatin1Char('0')));
            }
            else if (hex_item->text() != value)
            {
              if (!hex_item->text().isEmpty())
                hex_item->setBackgroundColor(QColor(0x77FFFF));

              hex_item->setText(value);
            }
            else if (hex_item->backgroundColor() != QColor(0xFFFFFF) &&
                     hex_item->backgroundColor() != QColor(Qt::red) &&
                     hex_item->backgroundColor() != QColor(220, 235, 235, 255))
            {
              hex_item->setBackgroundColor(hex_item->backgroundColor().lighter(107));
            }
            else if (PowerPC::memchecks.OverlapsMemcheck(address, 16 / GetColumnCount(m_type)))
            {
              hex_item->setBackground(Qt::red);
            }
          }
          else
          {
            hex_item->setText(QStringLiteral("-"));
          }
        }
      };

      switch (m_type)
      {
      case Type::U8:
        update_values([](u32 address) {
          const u8 value = PowerPC::HostRead_U8(address);
          return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0'));
        });
        break;
      case Type::ASCII:
      case Type::U32xASCII:
        update_values([](u32 address) {
          QString asciistring = QStringLiteral("");
          // Group ASCII in sets of four.
          for (u32 i = 0; i < 4; i++)
          {
            char value = PowerPC::HostRead_U8(address + i);
            asciistring.append(std::isprint(value) ? QChar::fromLatin1(value) :
                                                     QStringLiteral("."));
          }
          return asciistring;
        });
        break;
      case Type::U16:
        update_values([](u32 address) {
          const u16 value = PowerPC::HostRead_U16(address);
          return QStringLiteral("%1").arg(value, 4, 16, QLatin1Char('0'));
        });
        break;
      case Type::U32:
        update_values([](u32 address) {
          const u32 value = PowerPC::HostRead_U32(address);
          return QStringLiteral("%1").arg(value, 8, 16, QLatin1Char('0'));
        });
        break;
      case Type::Float32:
      case Type::U32xFloat32:
        update_values([](u32 address) { return QString::number(PowerPC::HostRead_F32(address)); });
        break;
      }
    }
  });
}

// const u32 MemoryViewWidget::PCTargetMemory()
//{
//  // If PC targets a memory location, output it
//  if (Core::GetState() != Core::State::Paused)
//    return 0;
//
//  std::string instruction;
//  Core::RunAsCPUThread([&] { instruction = PowerPC::debug_interface.Disassemble(PC); });
//  if ((instruction.compare(0, 2, "st") != 0 && instruction.compare(0, 1, "l") != 0 &&
//       instruction.compare(0, 5, "psq_l") != 0 && instruction.compare(0, 5, "psq_s") != 0) ||
//      instruction.compare(0, 2, "li") == 0)
//    return 0;
//  else
//    return PowerPC::debug_interface.GetMemoryAddressFromInstruction(instruction);
//}

void MemoryViewWidget::SetType(Type type)
{
  if (m_type == type)
    return;

  m_type = type;
  Update();
}

void MemoryViewWidget::SetBPType(BPType type)
{
  m_bp_type = type;
}

void MemoryViewWidget::SetAddress(u32 address)
{
  if (m_address == address)
    return;

  m_target = address;
  m_address = address;
  Update();
}

void MemoryViewWidget::SetAlignment(bool align)
{
  m_align = align;
  Update();
}

void MemoryViewWidget::SetBPLoggingEnabled(bool enabled)
{
  m_do_log = enabled;
}

void MemoryViewWidget::resizeEvent(QResizeEvent*)
{
  Update();
}

void MemoryViewWidget::keyPressEvent(QKeyEvent* event)
{
  switch (event->key())
  {
  case Qt::Key_Up:
    m_address -= 16;
    Update();
    return;
  case Qt::Key_Down:
    m_address += 16;
    Update();
    return;
  case Qt::Key_PageUp:
    m_address -= rowCount() * 16;
    Update();
    return;
  case Qt::Key_PageDown:
    m_address += rowCount() * 16;
    Update();
    return;
  default:
    QWidget::keyPressEvent(event);
    break;
  }
}

u32 MemoryViewWidget::GetContextAddress() const
{
  return m_context_address;
}

void MemoryViewWidget::ToggleRowBreakpoint(bool row)
{
  TMemCheck check;

  const u32 addr = row ? GetContextAddress() & 0xFFFFFFFC : GetContextAddress();
  const auto length =
      (GetColumnCount(m_type) == 2) ? 4 : (row ? 16 : (16 / GetColumnCount(m_type)));

  if (!PowerPC::memchecks.OverlapsMemcheck(addr, length))
  {
    check.start_address = addr;
    check.end_address = check.start_address + length - 1;
    check.is_ranged = length > 0;
    check.is_break_on_read = (m_bp_type == BPType::ReadOnly || m_bp_type == BPType::ReadWrite);
    check.is_break_on_write = (m_bp_type == BPType::WriteOnly || m_bp_type == BPType::ReadWrite);
    check.log_on_hit = m_do_log;
    check.break_on_hit = true;

    PowerPC::memchecks.Add(check);
  }
  else
  {
    PowerPC::memchecks.Remove(addr);
  }

  emit BreakpointsChanged();
  Update();
}

void MemoryViewWidget::ToggleBreakpoint()
{
  ToggleRowBreakpoint(false);
}

void MemoryViewWidget::wheelEvent(QWheelEvent* event)
{
  auto delta =
      -static_cast<int>(std::round((event->angleDelta().y() / (SCROLL_FRACTION_DEGREES * 8))));

  if (delta == 0)
    return;

  m_address += delta * 16;
  Update();
}

void MemoryViewWidget::mousePressEvent(QMouseEvent* event)
{
  auto* item = itemAt(event->pos());
  if (item == nullptr)
    return;

  bool good;
  const u32 addr = item->data(Qt::UserRole).toUInt(&good);

  if (!good)
    return;

  m_context_address = addr;

  switch (event->button())
  {
  case Qt::LeftButton:

    if (event->modifiers() & Qt::ShiftModifier)
    {
      clearSelection();
      item->setSelected(true);

      QString setaddr = QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0'));
      emit SendSearchValue(setaddr);
    }
    else if (event->modifiers() & Qt::ControlModifier)
    {
      clearSelection();
      item->setSelected(true);

      const auto length = 32 / ((GetColumnCount(m_type) == 2) ? 4 : GetColumnCount(m_type));
      u64 value = PowerPC::HostRead_U64(addr);
      QString setvalue = QStringLiteral("%1").arg(value, 16, 16, QLatin1Char('0')).left(length);
      emit SendDataValue(setvalue);
    }
    else if (column(item) == 0)
    {
      ToggleRowBreakpoint(true);
    }
    else
    {
      // Scroll with LClick
      if (GetColumnCount(m_type) == 2)
        SetAddress(addr & 0xFFFFFFFC);
      else
        SetAddress(addr & 0xFFFFFFF0);

      Update();
    }

    break;
  default:
    break;
  }
}

void MemoryViewWidget::OnCopyAddress()
{
  u32 addr = GetContextAddress();
  QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
}

void MemoryViewWidget::OnCopyHex()
{
  u32 addr = GetContextAddress();

  const auto length = 16 / ((GetColumnCount(m_type) == 2) ? 4 : GetColumnCount(m_type));

  u64 value = PowerPC::HostRead_U64(addr);

  QApplication::clipboard()->setText(
      QStringLiteral("%1").arg(value, length * 2, 16, QLatin1Char('0')).left(length * 2));
}

void MemoryViewWidget::OnAddNote()
{
  u32 note_address = GetContextAddress();
  note_address = note_address & 0xFFFFFFF0;
  std::string name = "";
  u32 size = 4;

  EditSymbolDialog* dialog = new EditSymbolDialog(this, note_address, size, name);

  if (dialog->exec() != QDialog::Accepted)
    return;

  PowerPC::debug_interface.UpdateNote(note_address, size, name);

  emit NotesChanged();
  Update();
}

void MemoryViewWidget::OnEditNote()
{
  u32 context_address = GetContextAddress();
  context_address = context_address & 0xFFFFFFF0;
  Common::Note* note = g_symbolDB.GetNoteFromAddr(context_address);

  std::string name = "";
  u32 size = 4;
  u32 note_address;

  if (note != nullptr)
  {
    name = note->name;
    size = note->size;
    note_address = note->address;
  }
  else
  {
    note_address = context_address;
  }

  EditSymbolDialog* dialog = new EditSymbolDialog(this, note_address, size, name);

  if (dialog->exec() != QDialog::Accepted)
    return;

  if (note == nullptr || note->name != name || note->size != size)
    PowerPC::debug_interface.UpdateNote(note_address, size, name);

  emit NotesChanged();
  Update();
}

void MemoryViewWidget::OnDeleteNote()
{
  u32 context_address = GetContextAddress();
  context_address = context_address & 0xFFFFFFF0;
  Common::Note* note = g_symbolDB.GetNoteFromAddr(context_address);
  g_symbolDB.DeleteNote(note->address);
  emit NotesChanged();
  Update();
}

void MemoryViewWidget::OnContextMenu()
{
  auto* menu = new QMenu(this);

  menu->addAction(tr("Copy Address"), this, &MemoryViewWidget::OnCopyAddress);

  auto* copy_hex = menu->addAction(tr("Copy Hex"), this, &MemoryViewWidget::OnCopyHex);

  copy_hex->setEnabled(Core::GetState() != Core::State::Uninitialized &&
                       PowerPC::HostIsRAMAddress(GetContextAddress()));

  menu->addSeparator();

  menu->addAction(tr("Add Note"), this, &MemoryViewWidget::OnAddNote);
  menu->addAction(tr("Edit Note"), this, &MemoryViewWidget::OnEditNote);
  menu->addAction(tr("Delete Note"), this, &MemoryViewWidget::OnDeleteNote);
  menu->addSeparator();
  menu->addAction(tr("Show in code"), this, [this] { emit ShowCode(GetContextAddress()); });

  menu->addSeparator();

  menu->addAction(tr("Toggle Breakpoint"), this, &MemoryViewWidget::ToggleBreakpoint);

  menu->addSeparator();

  menu->addAction(m_auto_update_action);

  menu->exec(QCursor::pos());
}
