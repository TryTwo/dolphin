// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/AdvancedWidget.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>
#include "QFontMetrics"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ConfigControls/ConfigInteger.h"
#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/VideoConfig.h"

AdvancedWidget::AdvancedWidget(GraphicsWindow* parent)
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
  AddDescriptions();

  connect(parent, &GraphicsWindow::BackendChanged, this, &AdvancedWidget::OnBackendChanged);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    OnEmulationStateChanged(state != Core::State::Uninitialized);
  });
  connect(m_manual_texture_sampling, &QCheckBox::toggled, [this, parent] {
    SaveSettings();
    emit parent->UseFastTextureSamplingChanged();
  });

  OnBackendChanged();
  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()) !=
                          Core::State::Uninitialized);
}

void AdvancedWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Performance
  auto* performance_box = new QGroupBox(tr("Performance Statistics"));
  auto* performance_layout = new QGridLayout();
  performance_box->setLayout(performance_layout);

  m_show_fps = new ConfigBool(tr("Show FPS"), Config::GFX_SHOW_FPS);
  m_show_ftimes = new ConfigBool(tr("Show Frame Times"), Config::GFX_SHOW_FTIMES);
  m_show_vps = new ConfigBool(tr("Show VPS"), Config::GFX_SHOW_VPS);
  m_show_vtimes = new ConfigBool(tr("Show VBlank Times"), Config::GFX_SHOW_VTIMES);
  m_show_graphs = new ConfigBool(tr("Show Performance Graphs"), Config::GFX_SHOW_GRAPHS);
  m_show_speed = new ConfigBool(tr("Show % Speed"), Config::GFX_SHOW_SPEED);
  m_show_speed_colors = new ConfigBool(tr("Show Speed Colors"), Config::GFX_SHOW_SPEED_COLORS);
  m_perf_samp_window = new ConfigInteger(0, 10000, Config::GFX_PERF_SAMP_WINDOW, 100);
  m_perf_samp_window->SetTitle(tr("Performance Sample Window (ms)"));
  m_log_render_time =
      new ConfigBool(tr("Log Render Time to File"), Config::GFX_LOG_RENDER_TIME_TO_FILE);

  performance_layout->addWidget(m_show_fps, 0, 0);
  performance_layout->addWidget(m_show_ftimes, 0, 1);
  performance_layout->addWidget(m_show_vps, 1, 0);
  performance_layout->addWidget(m_show_vtimes, 1, 1);
  performance_layout->addWidget(m_show_speed, 2, 0);
  performance_layout->addWidget(m_show_graphs, 2, 1);
  performance_layout->addWidget(new QLabel(tr("Performance Sample Window (ms):")), 3, 0);
  performance_layout->addWidget(m_perf_samp_window, 3, 1);
  performance_layout->addWidget(m_log_render_time, 4, 0);
  performance_layout->addWidget(m_show_speed_colors, 4, 1);

  // Debugging
  auto* debugging_box = new QGroupBox(tr("Debugging"));
  auto* debugging_layout = new QGridLayout();
  debugging_box->setLayout(debugging_layout);

  m_enable_wireframe = new ConfigBool(tr("Enable Wireframe"), Config::GFX_ENABLE_WIREFRAME);
  m_show_statistics = new ConfigBool(tr("Show Statistics"), Config::GFX_OVERLAY_STATS);
  m_show_proj_statistics =
      new ConfigBool(tr("Show Projection Statistics"), Config::GFX_OVERLAY_PROJ_STATS);
  m_enable_format_overlay =
      new ConfigBool(tr("Texture Format Overlay"), Config::GFX_TEXFMT_OVERLAY_ENABLE);
  m_enable_api_validation =
      new ConfigBool(tr("Enable API Validation Layers"), Config::GFX_ENABLE_VALIDATION_LAYER);

  debugging_layout->addWidget(m_enable_wireframe, 0, 0);
  debugging_layout->addWidget(m_show_statistics, 0, 1);
  debugging_layout->addWidget(m_enable_format_overlay, 1, 0);
  debugging_layout->addWidget(m_show_proj_statistics, 1, 1);
  debugging_layout->addWidget(m_enable_api_validation, 2, 0);

  // Utility
  auto* utility_box = new QGroupBox(tr("Utility"));
  auto* utility_layout = new QGridLayout();
  utility_box->setLayout(utility_layout);

  m_load_custom_textures = new ConfigBool(tr("Load Custom Textures"), Config::GFX_HIRES_TEXTURES);
  m_prefetch_custom_textures =
      new ConfigBool(tr("Prefetch Custom Textures"), Config::GFX_CACHE_HIRES_TEXTURES);
  m_dump_efb_target = new ConfigBool(tr("Dump EFB Target"), Config::GFX_DUMP_EFB_TARGET);
  m_dump_xfb_target = new ConfigBool(tr("Dump XFB Target"), Config::GFX_DUMP_XFB_TARGET);
  m_disable_vram_copies =
      new ConfigBool(tr("Disable EFB VRAM Copies"), Config::GFX_HACK_DISABLE_COPY_TO_VRAM);
  m_enable_graphics_mods = new ToolTipCheckBox(tr("Enable Graphics Mods"));

  utility_layout->addWidget(m_load_custom_textures, 0, 0);
  utility_layout->addWidget(m_prefetch_custom_textures, 0, 1);

  utility_layout->addWidget(m_disable_vram_copies, 1, 0);
  utility_layout->addWidget(m_enable_graphics_mods, 1, 1);

  utility_layout->addWidget(m_dump_efb_target, 2, 0);
  utility_layout->addWidget(m_dump_xfb_target, 2, 1);

  // Texture dumping
  auto* texture_dump_box = new QGroupBox(tr("Texture Dumping"));
  auto* texture_dump_layout = new QGridLayout();
  texture_dump_box->setLayout(texture_dump_layout);
  m_dump_textures = new ConfigBool(tr("Enable"), Config::GFX_DUMP_TEXTURES);
  m_dump_base_textures = new ConfigBool(tr("Dump Base Textures"), Config::GFX_DUMP_BASE_TEXTURES);
  m_dump_mip_textures = new ConfigBool(tr("Dump Mip Maps"), Config::GFX_DUMP_MIP_TEXTURES);

  texture_dump_layout->addWidget(m_dump_textures, 0, 0);

  texture_dump_layout->addWidget(m_dump_base_textures, 1, 0);
  texture_dump_layout->addWidget(m_dump_mip_textures, 1, 1);

  // Frame dumping
  auto* dump_box = new QGroupBox(tr("Frame Dumping"));
  auto* dump_layout = new QGridLayout();
  dump_box->setLayout(dump_layout);

  m_frame_dumps_resolution_type =
      new ConfigChoice({tr("Window Resolution"), tr("Aspect Ratio Corrected Internal Resolution"),
                        tr("Raw Internal Resolution")},
                       Config::GFX_FRAME_DUMPS_RESOLUTION_TYPE);
  m_dump_use_ffv1 = new ConfigBool(tr("Use Lossless Codec (FFV1)"), Config::GFX_USE_FFV1);
  m_dump_bitrate = new ConfigInteger(0, 1000000, Config::GFX_BITRATE_KBPS, 1000);
  m_png_compression_level = new ConfigInteger(0, 9, Config::GFX_PNG_COMPRESSION_LEVEL);

  dump_layout->addWidget(new QLabel(tr("Resolution Type:")), 0, 0);
  dump_layout->addWidget(m_frame_dumps_resolution_type, 0, 1);
#if defined(HAVE_FFMPEG)
  dump_layout->addWidget(m_dump_use_ffv1, 1, 0);
  dump_layout->addWidget(new QLabel(tr("Bitrate (kbps):")), 2, 0);
  dump_layout->addWidget(m_dump_bitrate, 2, 1);
#endif
  dump_layout->addWidget(new QLabel(tr("PNG Compression Level:")), 3, 0);
  m_png_compression_level->SetTitle(tr("PNG Compression Level"));
  dump_layout->addWidget(m_png_compression_level, 3, 1);

  // Misc.
  auto* misc_box = new QGroupBox(tr("Misc"));
  auto* misc_layout = new QGridLayout();
  misc_box->setLayout(misc_layout);

  m_enable_cropping = new ConfigBool(tr("Crop"), Config::GFX_CROP);
  m_enable_prog_scan = new ToolTipCheckBox(tr("Enable Progressive Scan"));
  m_backend_multithreading =
      new ConfigBool(tr("Backend Multithreading"), Config::GFX_BACKEND_MULTITHREADING);
  m_prefer_vs_for_point_line_expansion = new ConfigBool(
      // i18n: VS is short for vertex shaders.
      tr("Prefer VS for Point/Line Expansion"), Config::GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION);
  m_cpu_cull = new ConfigBool(tr("Cull Vertices on the CPU"), Config::GFX_CPU_CULL);

  misc_layout->addWidget(m_enable_cropping, 0, 0);
  misc_layout->addWidget(m_enable_prog_scan, 0, 1);
  misc_layout->addWidget(m_backend_multithreading, 1, 0);
  misc_layout->addWidget(m_prefer_vs_for_point_line_expansion, 1, 1);
  misc_layout->addWidget(m_cpu_cull, 2, 0);
#ifdef _WIN32
  m_borderless_fullscreen =
      new ConfigBool(tr("Borderless Fullscreen"), Config::GFX_BORDERLESS_FULLSCREEN);

  misc_layout->addWidget(m_borderless_fullscreen, 2, 1);
#endif

  // Scaled EFB Copy Exclusions
  auto* efb_box = new QGroupBox(tr("Scaled EFB Copy Exclusions"));
  auto* efb_layout = new QVBoxLayout();
  auto* efb_layout_width_integer = new QHBoxLayout();
  auto* efb_layout_top = new QHBoxLayout();
  auto* efb_layout_bottom = new QHBoxLayout();
  efb_box->setLayout(efb_layout);

  m_scaled_efb_exclude_enable =
      new ConfigBool(tr("Enabled"), Config::GFX_EFB_SCALE_EXCLUDE_ENABLED);
  m_scaled_efb_exclude_alt = new ConfigBool(tr("Filter Less"), Config::GFX_EFB_SCALE_EXCLUDE_ALT);
  m_scaled_efb_exclude_blur = new ConfigBool(tr("Edit Bloom"), Config::GFX_EFB_SCALE_EXCLUDE_BLUR);
  m_scaled_efb_exclude_downscale =
      new ConfigBool(tr("Downscale Bloom"), Config::GFX_EFB_SCALE_EXCLUDE_DOWNSCALE);
  m_scaled_efb_exclude_slider_width =
      new ConfigSlider(0, EFB_WIDTH, Config::GFX_EFB_SCALE_EXCLUDE_WIDTH, 1);
  m_scaled_efb_exclude_integer_width =
      new ConfigInteger(0, EFB_WIDTH, Config::GFX_EFB_SCALE_EXCLUDE_WIDTH, 1);
  // Multipled by 5 for percentage
  m_scaled_efb_exclude_slider_bloom_strength =
      new ConfigSlider(0, 25, Config::GFX_EFB_SCALE_EXCLUDE_BLOOM_STRENGTH, 20);
  m_scaled_efb_exclude_slider_blur_radius =
      new ConfigSlider(0, 10, Config::GFX_EFB_SCALE_EXCLUDE_BLUR_RADIUS, 1);
  auto* bloom_strength_label = new QLabel(tr("Strength:"));
  auto* blur_radius_label = new QLabel(tr("Radius:"));
  m_bloom_strength_val_label = new QLabel(tr("100%"));
  m_bloom_strength_val_label->setFixedWidth(32);
  m_blur_radius_val_label = new QLabel(tr("1"));
  m_blur_radius_val_label->setFixedWidth(16);

  if (!m_scaled_efb_exclude_enable->isChecked())
  {
    m_scaled_efb_exclude_slider_width->setEnabled(false);
    m_scaled_efb_exclude_alt->setEnabled(false);
    m_scaled_efb_exclude_blur->setEnabled(false);
    m_scaled_efb_exclude_downscale->setEnabled(false);
    m_scaled_efb_exclude_integer_width->setEnabled(false);
    m_scaled_efb_exclude_slider_blur_radius->setEnabled(false);
    m_scaled_efb_exclude_slider_bloom_strength->setEnabled(false);
  }

  m_scaled_efb_exclude_slider_blur_radius->setTickPosition(QSlider::TicksBelow);
  // m_scaled_efb_exclude_slider_blur_radius->setTickInterval(1);
  m_scaled_efb_exclude_slider_bloom_strength->setTickPosition(QSlider::TicksBelow);
  // m_scaled_efb_exclude_slider_bloom_strength->setTickInterval(5);

  QFontMetrics fm(font());
  m_scaled_efb_exclude_integer_width->setFixedWidth(fm.lineSpacing() * 4);

  efb_layout_top->addWidget(m_scaled_efb_exclude_enable);
  efb_layout_top->addStretch();
  efb_layout_top->addWidget(m_scaled_efb_exclude_downscale);
  efb_layout_top->addWidget(m_scaled_efb_exclude_alt);
  efb_layout_top->addWidget(m_scaled_efb_exclude_blur);
  efb_layout_width_integer->addWidget(new QLabel(tr("Width < ")));
  efb_layout_width_integer->addWidget(m_scaled_efb_exclude_integer_width);
  efb_layout_width_integer->addWidget(m_scaled_efb_exclude_slider_width);
  efb_layout_bottom->addWidget(blur_radius_label);
  efb_layout_bottom->addWidget(m_blur_radius_val_label);
  efb_layout_bottom->addWidget(m_scaled_efb_exclude_slider_blur_radius);
  efb_layout_bottom->addWidget(bloom_strength_label);
  efb_layout_bottom->addWidget(m_bloom_strength_val_label);
  efb_layout_bottom->addWidget(m_scaled_efb_exclude_slider_bloom_strength);
  efb_layout->addLayout(efb_layout_top);
  efb_layout->addLayout(efb_layout_width_integer);
  efb_layout->addLayout(efb_layout_bottom);

  // Experimental.
  auto* experimental_box = new QGroupBox(tr("Experimental"));
  auto* experimental_layout = new QGridLayout();
  experimental_box->setLayout(experimental_layout);

  m_defer_efb_access_invalidation =
      new ConfigBool(tr("Defer EFB Cache Invalidation"), Config::GFX_HACK_EFB_DEFER_INVALIDATION);
  m_manual_texture_sampling =
      new ConfigBool(tr("Manual Texture Sampling"), Config::GFX_HACK_FAST_TEXTURE_SAMPLING, true);

  experimental_layout->addWidget(m_defer_efb_access_invalidation, 0, 0);
  experimental_layout->addWidget(m_manual_texture_sampling, 0, 1);

  main_layout->addWidget(performance_box);
  main_layout->addWidget(debugging_box);
  main_layout->addWidget(utility_box);
  main_layout->addWidget(texture_dump_box);
  main_layout->addWidget(dump_box);
  main_layout->addWidget(misc_box);
  main_layout->addWidget(efb_box);
  main_layout->addWidget(experimental_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void AdvancedWidget::ConnectWidgets()
{
  connect(m_load_custom_textures, &QCheckBox::toggled, this, &AdvancedWidget::SaveSettings);
  connect(m_dump_use_ffv1, &QCheckBox::toggled, this, &AdvancedWidget::SaveSettings);
  connect(m_enable_prog_scan, &QCheckBox::toggled, this, &AdvancedWidget::SaveSettings);
  connect(m_scaled_efb_exclude_enable, &ConfigBool::toggled, [=](bool checked) {
    m_scaled_efb_exclude_slider_width->setEnabled(checked);
    m_scaled_efb_exclude_alt->setEnabled(checked);
    m_scaled_efb_exclude_integer_width->setEnabled(checked);
    m_scaled_efb_exclude_downscale->setEnabled(checked);
    if (m_scaled_efb_exclude_blur->isChecked() == true)
    {
      m_scaled_efb_exclude_slider_bloom_strength->setEnabled(checked);
      m_scaled_efb_exclude_slider_blur_radius->setEnabled(checked);
    }

    m_scaled_efb_exclude_blur->setEnabled(checked);
  });
  connect(m_scaled_efb_exclude_slider_bloom_strength, &ConfigSlider::valueChanged, this,
          [=](int value) { m_bloom_strength_val_label->setText(tr("%1%").arg(value * 5)); });
  connect(m_scaled_efb_exclude_slider_blur_radius, &ConfigSlider::valueChanged, this,
          [=](int value) {
            if (value == 0)
              m_blur_radius_val_label->setText(tr("off"));
            else
              m_blur_radius_val_label->setText(tr("%1").arg(value));
          });

  connect(m_scaled_efb_exclude_blur, &QCheckBox::toggled, [=](bool checked) {
    m_scaled_efb_exclude_slider_bloom_strength->setEnabled(checked);
    m_scaled_efb_exclude_slider_blur_radius->setEnabled(checked);
  });

  // A &QSlider signal won't fire when game ini's trigger a change, due to a signalblock in
  // GraphicsSlider
  //	connect(m_scaled_efb_exclude_slider_width, &ConfigSlider::valueChanged, [=] {
  //		m_scaled_efb_exclude_integer_width->setValue(m_scaled_efb_exclude_slider_width->value());
  //		});

  connect(m_dump_textures, &QCheckBox::toggled, this, &AdvancedWidget::SaveSettings);
  connect(m_enable_graphics_mods, &QCheckBox::toggled, this, &AdvancedWidget::SaveSettings);
}

void AdvancedWidget::LoadSettings()
{
  m_prefetch_custom_textures->setEnabled(Config::Get(Config::GFX_HIRES_TEXTURES));
  m_dump_bitrate->setEnabled(!Config::Get(Config::GFX_USE_FFV1));

  m_enable_prog_scan->setChecked(Config::Get(Config::SYSCONF_PROGRESSIVE_SCAN));
  m_dump_mip_textures->setEnabled(Config::Get(Config::GFX_DUMP_TEXTURES));
  m_dump_base_textures->setEnabled(Config::Get(Config::GFX_DUMP_TEXTURES));

  m_bloom_strength_val_label->setText(
      (tr("%1%").arg(Config::Get(Config::GFX_EFB_SCALE_EXCLUDE_BLOOM_STRENGTH) * 5)));
  m_blur_radius_val_label->setText(
      tr("%1").arg(Config::Get(Config::GFX_EFB_SCALE_EXCLUDE_BLUR_RADIUS)));

  SignalBlocking(m_enable_graphics_mods)->setChecked(Settings::Instance().GetGraphicModsEnabled());
}

void AdvancedWidget::SaveSettings()
{
  m_prefetch_custom_textures->setEnabled(Config::Get(Config::GFX_HIRES_TEXTURES));
  m_dump_bitrate->setEnabled(!Config::Get(Config::GFX_USE_FFV1));

  Config::SetBase(Config::SYSCONF_PROGRESSIVE_SCAN, m_enable_prog_scan->isChecked());
  m_dump_mip_textures->setEnabled(Config::Get(Config::GFX_DUMP_TEXTURES));
  m_dump_base_textures->setEnabled(Config::Get(Config::GFX_DUMP_TEXTURES));
  Settings::Instance().SetGraphicModsEnabled(m_enable_graphics_mods->isChecked());
}

void AdvancedWidget::OnBackendChanged()
{
  m_backend_multithreading->setEnabled(g_Config.backend_info.bSupportsMultithreading);
  m_prefer_vs_for_point_line_expansion->setEnabled(
      g_Config.backend_info.bSupportsGeometryShaders &&
      g_Config.backend_info.bSupportsVSLinePointExpand);
  AddDescriptions();
}

void AdvancedWidget::OnEmulationStateChanged(bool running)
{
  m_enable_prog_scan->setEnabled(!running);
}

void AdvancedWidget::AddDescriptions()
{
  static const char TR_SHOW_FPS_DESCRIPTION[] =
      QT_TR_NOOP("Shows the number of distinct frames rendered per second as a measure of "
                 "visual smoothness.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_FTIMES_DESCRIPTION[] =
      QT_TR_NOOP("Shows the average time in ms between each distinct rendered frame alongside "
                 "the standard deviation.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_VPS_DESCRIPTION[] =
      QT_TR_NOOP("Shows the number of frames rendered per second as a measure of "
                 "emulation speed.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_VTIMES_DESCRIPTION[] =
      QT_TR_NOOP("Shows the average time in ms between each rendered frame alongside "
                 "the standard deviation.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_GRAPHS_DESCRIPTION[] =
      QT_TR_NOOP("Shows frametime graph along with statistics as a representation of "
                 "emulation performance.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_SPEED_DESCRIPTION[] =
      QT_TR_NOOP("Shows the % speed of emulation compared to full speed."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_SPEED_COLORS_DESCRIPTION[] =
      QT_TR_NOOP("Changes the color of the FPS counter depending on emulation speed."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "checked.</dolphin_emphasis>");
  static const char TR_PERF_SAMP_WINDOW_DESCRIPTION[] =
      QT_TR_NOOP("The amount of time the FPS and VPS counters will sample over."
                 "<br><br>The higher the value, the more stable the FPS/VPS counter will be, "
                 "but the slower it will be to update."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "at 1000ms.</dolphin_emphasis>");
  static const char TR_LOG_RENDERTIME_DESCRIPTION[] = QT_TR_NOOP(
      "Logs the render time of every frame to User/Logs/render_time.txt.<br><br>Use this "
      "feature to measure Dolphin's performance.<br><br><dolphin_emphasis>If "
      "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_WIREFRAME_DESCRIPTION[] =
      QT_TR_NOOP("Renders the scene as a wireframe.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_STATS_DESCRIPTION[] =
      QT_TR_NOOP("Shows various rendering statistics.<br><br><dolphin_emphasis>If unsure, "
                 "leave this unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_PROJ_STATS_DESCRIPTION[] =
      QT_TR_NOOP("Shows various projection statistics.<br><br><dolphin_emphasis>If unsure, "
                 "leave this unchecked.</dolphin_emphasis>");
  static const char TR_TEXTURE_FORMAT_DESCRIPTION[] =
      QT_TR_NOOP("Modifies textures to show the format they're encoded in.<br><br>May require "
                 "an emulation reset to apply.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_VALIDATION_LAYER_DESCRIPTION[] =
      QT_TR_NOOP("Enables validation of API calls made by the video backend, which may assist in "
                 "debugging graphical issues. On the Vulkan and D3D backends, this also enables "
                 "debug symbols for the compiled shaders.<br><br><dolphin_emphasis>If unsure, "
                 "leave this unchecked.</dolphin_emphasis>");
  static const char TR_DUMP_TEXTURE_DESCRIPTION[] =
      QT_TR_NOOP("Dumps decoded game textures based on the other flags to "
                 "User/Dump/Textures/&lt;game_id&gt;/.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");
  static const char TR_DUMP_MIP_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Whether to dump mipmapped game textures to "
      "User/Dump/Textures/&lt;game_id&gt;/.  This includes arbitrary mipmapped textures if "
      "'Arbitrary Mipmap Detection' is enabled in Enhancements.<br><br>"
      "<dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_DUMP_BASE_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Whether to dump base game textures to "
      "User/Dump/Textures/&lt;game_id&gt;/.  This includes arbitrary base textures if 'Arbitrary "
      "Mipmap Detection' is enabled in Enhancements.<br><br><dolphin_emphasis>If unsure, leave "
      "this checked.</dolphin_emphasis>");
  static const char TR_LOAD_CUSTOM_TEXTURE_DESCRIPTION[] =
      QT_TR_NOOP("Loads custom textures from User/Load/Textures/&lt;game_id&gt;/ and "
                 "User/Load/DynamicInputTextures/&lt;game_id&gt;/.<br><br><dolphin_emphasis>If "
                 "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_CACHE_CUSTOM_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Caches custom textures to system RAM on startup.<br><br>This can require exponentially "
      "more RAM but fixes possible stuttering.<br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
  static const char TR_DUMP_EFB_DESCRIPTION[] =
      QT_TR_NOOP("Dumps the contents of EFB copies to User/Dump/Textures/.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_DUMP_XFB_DESCRIPTION[] =
      QT_TR_NOOP("Dumps the contents of XFB copies to User/Dump/Textures/.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_DISABLE_VRAM_COPIES_DESCRIPTION[] =
      QT_TR_NOOP("Disables the VRAM copy of the EFB, forcing a round-trip to RAM. Inhibits all "
                 "upscaling.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_LOAD_GRAPHICS_MODS_DESCRIPTION[] =
      QT_TR_NOOP("Loads graphics mods from User/Load/GraphicsMods/.<br><br><dolphin_emphasis>If "
                 "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_FRAME_DUMPS_RESOLUTION_TYPE_DESCRIPTION[] = QT_TR_NOOP(
      "Selects how frame dumps (videos) and screenshots are going to be captured.<br>If the game "
      "or window resolution change during a recording, multiple video files might be created.<br>"
      "Note that color correction and cropping are always ignored by the captures."
      "<br><br><b>Window Resolution</b>: Uses the output window resolution (without black bars)."
      "<br>This is a simple dumping option that will capture the image more or less as you see it."
      "<br><b>Aspect Ratio Corrected Internal Resolution</b>: "
      "Uses the Internal Resolution (XFB size), and corrects it by the target aspect ratio.<br>"
      "This option will consistently dump at the specified Internal Resolution "
      "regardless of how the image is displayed during recording."
      "<br><b>Raw Internal Resolution</b>: Uses the Internal Resolution (XFB size) "
      "without correcting it with the target aspect ratio.<br>"
      "This will provide a clean dump without any aspect ratio correction so users have as raw as "
      "possible input for external editing software.<br><br><dolphin_emphasis>If unsure, leave "
      "this at \"Aspect Ratio Corrected Internal Resolution\".</dolphin_emphasis>");
#if defined(HAVE_FFMPEG)
  static const char TR_USE_FFV1_DESCRIPTION[] =
      QT_TR_NOOP("Encodes frame dumps using the FFV1 codec.<br><br><dolphin_emphasis>If "
                 "unsure, leave this unchecked.</dolphin_emphasis>");
#endif
  static const char TR_PNG_COMPRESSION_LEVEL_DESCRIPTION[] =
      QT_TR_NOOP("Specifies the zlib compression level to use when saving PNG images (both for "
                 "screenshots and framedumping).<br><br>"
                 "Since PNG uses lossless compression, this does not affect the image quality; "
                 "instead, it is a trade-off between file size and compression time.<br><br>"
                 "A value of 0 uses no compression at all.  A value of 1 uses very little "
                 "compression, while the maximum value of 9 applies a lot of compression.  "
                 "However, for PNG files, levels between 3 and 6 are generally about as good as "
                 "level 9 but finish in significantly less time.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this at 6.</dolphin_emphasis>");
  static const char TR_CROPPING_DESCRIPTION[] = QT_TR_NOOP(
      "Crops the picture from its native aspect ratio (which rarely exactly matches 4:3 or 16:9),"
      " to the specific user target aspect ratio (e.g. 4:3 or 16:9).<br><br>"
      "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_PROGRESSIVE_SCAN_DESCRIPTION[] = QT_TR_NOOP(
      "Enables progressive scan if supported by the emulated software. Most games don't have "
      "any issue with this.<br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
  static const char TR_BACKEND_MULTITHREADING_DESCRIPTION[] =
      QT_TR_NOOP("Enables multithreaded command submission in backends where supported. Enabling "
                 "this option may result in a performance improvement on systems with more than "
                 "two CPU cores. Currently, this is limited to the Vulkan backend.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_PREFER_VS_FOR_POINT_LINE_EXPANSION_DESCRIPTION[] =
      QT_TR_NOOP("On backends that support both using the geometry shader and the vertex shader "
                 "for expanding points and lines, selects the vertex shader for the job.  May "
                 "affect performance."
                 "<br><br>%1");
  static const char TR_CPU_CULL_DESCRIPTION[] =
      QT_TR_NOOP("Cull vertices on the CPU to reduce the number of draw calls required.  "
                 "May affect performance and draw statistics.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_DEFER_EFB_ACCESS_INVALIDATION_DESCRIPTION[] = QT_TR_NOOP(
      "Defers invalidation of the EFB access cache until a GPU synchronization command "
      "is executed. If disabled, the cache will be invalidated with every draw call. "
      "<br><br>May improve performance in some games which rely on CPU EFB Access at the cost "
      "of stability.<br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
  static const char TR_MANUAL_TEXTURE_SAMPLING_DESCRIPTION[] = QT_TR_NOOP(
      "Use a manual implementation of texture sampling instead of the graphics backend's built-in "
      "functionality.<br><br>"
      "This setting can fix graphical issues in some games on certain GPUs, most commonly vertical "
      "lines on FMVs. In addition to this, enabling Manual Texture Sampling will allow for correct "
      "emulation of texture wrapping special cases (at 1x IR or when scaled EFB is disabled, and "
      "with custom textures disabled) and better emulates Level of Detail calculation.<br><br>"
      "This comes at the cost of potentially worse performance, especially at higher internal "
      "resolutions; additionally, Anisotropic Filtering is currently incompatible with Manual "
      "Texture Sampling.<br><br>"
      "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_SCALED_EFB_EXCLUDE_DESCRIPTION[] = QT_TR_NOOP(
      "EFB copies can have different sizes. Scaling up small EFB copies can create graphical "
      "issues, like poor bloom. These sliders will exclude efb copies from scaling based on their "
      "width and/or height in pixels. <br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
  static const char TR_SCALED_EFB_EXCLUDE_WIDTH_DESCRIPTION[] = QT_TR_NOOP(
      "This slider will exclude EFB copies from scaling based on their "
      "width in pixels. <br><br>0 = "
      "exclude nothing. <br><br>640 = exclude everything, the same as Scaled EFB Copy = "
      "off.<br><br>"
      "Start on the left and slowly move the slider to the right until the graphical issue "
      "improves. Values of 161, 300 or 630 may "
      "be good.");
  static const char TR_SCALED_EFB_EXCLUDE_BLUR_DESCRIPTION[] =
      QT_TR_NOOP("Allows EFB to be upscaled, but then Blurs it to produce a higher quality bloom. "
                 "Fixes shimmering issues that the normal bloom exclusions cause in various games "
                 "with large bloom EFBs. Can fail to fix bloom in games with small bloom EFBs."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SCALED_EFB_EXCLUDE_ALT_DESCRIPTION[] =
      QT_TR_NOOP("Only excludes textures that are written on top of eachother. Fixes low-res "
                 "issues in some games."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");

#ifdef _WIN32
  static const char TR_BORDERLESS_FULLSCREEN_DESCRIPTION[] = QT_TR_NOOP(
      "Implements fullscreen mode with a borderless window spanning the whole screen instead of "
      "using exclusive mode. Allows for faster transitions between fullscreen and windowed mode, "
      "but slightly increases input latency, makes movement less smooth and slightly decreases "
      "performance.<br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
#endif

  static const char IF_UNSURE_UNCHECKED[] =
      QT_TR_NOOP("<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");

  m_show_fps->SetDescription(tr(TR_SHOW_FPS_DESCRIPTION));
  m_show_ftimes->SetDescription(tr(TR_SHOW_FTIMES_DESCRIPTION));
  m_show_vps->SetDescription(tr(TR_SHOW_VPS_DESCRIPTION));
  m_show_vtimes->SetDescription(tr(TR_SHOW_VTIMES_DESCRIPTION));
  m_show_graphs->SetDescription(tr(TR_SHOW_GRAPHS_DESCRIPTION));
  m_show_speed->SetDescription(tr(TR_SHOW_SPEED_DESCRIPTION));
  m_log_render_time->SetDescription(tr(TR_LOG_RENDERTIME_DESCRIPTION));
  m_show_speed_colors->SetDescription(tr(TR_SHOW_SPEED_COLORS_DESCRIPTION));

  m_enable_wireframe->SetDescription(tr(TR_WIREFRAME_DESCRIPTION));
  m_show_statistics->SetDescription(tr(TR_SHOW_STATS_DESCRIPTION));
  m_show_proj_statistics->SetDescription(tr(TR_SHOW_PROJ_STATS_DESCRIPTION));
  m_enable_format_overlay->SetDescription(tr(TR_TEXTURE_FORMAT_DESCRIPTION));
  m_enable_api_validation->SetDescription(tr(TR_VALIDATION_LAYER_DESCRIPTION));
  m_perf_samp_window->SetDescription(tr(TR_PERF_SAMP_WINDOW_DESCRIPTION));
  m_dump_textures->SetDescription(tr(TR_DUMP_TEXTURE_DESCRIPTION));
  m_dump_mip_textures->SetDescription(tr(TR_DUMP_MIP_TEXTURE_DESCRIPTION));
  m_dump_base_textures->SetDescription(tr(TR_DUMP_BASE_TEXTURE_DESCRIPTION));
  m_load_custom_textures->SetDescription(tr(TR_LOAD_CUSTOM_TEXTURE_DESCRIPTION));
  m_prefetch_custom_textures->SetDescription(tr(TR_CACHE_CUSTOM_TEXTURE_DESCRIPTION));
  m_dump_efb_target->SetDescription(tr(TR_DUMP_EFB_DESCRIPTION));
  m_dump_xfb_target->SetDescription(tr(TR_DUMP_XFB_DESCRIPTION));
  m_disable_vram_copies->SetDescription(tr(TR_DISABLE_VRAM_COPIES_DESCRIPTION));
  m_enable_graphics_mods->SetDescription(tr(TR_LOAD_GRAPHICS_MODS_DESCRIPTION));
  m_frame_dumps_resolution_type->SetDescription(tr(TR_FRAME_DUMPS_RESOLUTION_TYPE_DESCRIPTION));
  m_scaled_efb_exclude_enable->SetTitle(tr("Scaled EFB Copy Exclusions"));
  m_scaled_efb_exclude_enable->SetDescription(tr(TR_SCALED_EFB_EXCLUDE_DESCRIPTION));
  m_scaled_efb_exclude_integer_width->SetTitle(tr("Width"));
  m_scaled_efb_exclude_integer_width->SetDescription(tr(TR_SCALED_EFB_EXCLUDE_WIDTH_DESCRIPTION));
  m_scaled_efb_exclude_slider_width->SetTitle(tr("Width"));
  m_scaled_efb_exclude_slider_width->SetDescription(tr(TR_SCALED_EFB_EXCLUDE_WIDTH_DESCRIPTION));
  m_scaled_efb_exclude_alt->SetTitle(tr("Reduce amount of exclusions"));
  m_scaled_efb_exclude_alt->SetDescription(tr(TR_SCALED_EFB_EXCLUDE_ALT_DESCRIPTION));
  m_scaled_efb_exclude_blur->SetTitle(tr("Upscale and blur"));
  m_scaled_efb_exclude_blur->SetDescription(tr(TR_SCALED_EFB_EXCLUDE_BLUR_DESCRIPTION));
#ifdef HAVE_FFMPEG
  m_dump_use_ffv1->SetDescription(tr(TR_USE_FFV1_DESCRIPTION));
#endif
  m_png_compression_level->SetDescription(tr(TR_PNG_COMPRESSION_LEVEL_DESCRIPTION));
  m_enable_cropping->SetDescription(tr(TR_CROPPING_DESCRIPTION));
  m_enable_prog_scan->SetDescription(tr(TR_PROGRESSIVE_SCAN_DESCRIPTION));
  m_backend_multithreading->SetDescription(tr(TR_BACKEND_MULTITHREADING_DESCRIPTION));
  QString vsexpand_extra;
  if (!g_Config.backend_info.bSupportsGeometryShaders)
    vsexpand_extra = tr("Forced on because %1 doesn't support geometry shaders.")
                         .arg(tr(g_Config.backend_info.DisplayName.c_str()));
  else if (!g_Config.backend_info.bSupportsVSLinePointExpand)
    vsexpand_extra = tr("Forced off because %1 doesn't support VS expansion.")
                         .arg(tr(g_Config.backend_info.DisplayName.c_str()));
  else
    vsexpand_extra = tr(IF_UNSURE_UNCHECKED);
  m_prefer_vs_for_point_line_expansion->SetDescription(
      tr(TR_PREFER_VS_FOR_POINT_LINE_EXPANSION_DESCRIPTION).arg(vsexpand_extra));
  m_cpu_cull->SetDescription(tr(TR_CPU_CULL_DESCRIPTION));
#ifdef _WIN32
  m_borderless_fullscreen->SetDescription(tr(TR_BORDERLESS_FULLSCREEN_DESCRIPTION));
#endif
  m_defer_efb_access_invalidation->SetDescription(tr(TR_DEFER_EFB_ACCESS_INVALIDATION_DESCRIPTION));
  m_manual_texture_sampling->SetDescription(tr(TR_MANUAL_TEXTURE_SAMPLING_DESCRIPTION));
}
