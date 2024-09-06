// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_platform.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"
#include "ui/ozone/public/platform_global_shortcut_listener.h"
#include "ui/ozone/public/platform_keyboard_hook.h"
#include "ui/ozone/public/platform_menu_utils.h"
#include "ui/ozone/public/platform_screen.h"
#include "ui/ozone/public/platform_user_input_monitor.h"

namespace ui {

namespace {
OzonePlatform* g_instance = nullptr;

bool g_fail_initialize_ui_for_test = false;

void EnsureInstance() {
  if (g_instance)
    return;

  TRACE_EVENT1("ozone", "OzonePlatform::Initialize", "platform",
               GetOzonePlatformName());
  std::unique_ptr<OzonePlatform> platform =
      PlatformObject<OzonePlatform>::Create();

  // TODO(spang): Currently need to leak this object.
  OzonePlatform* pl = platform.release();
  DCHECK_EQ(g_instance, pl);
}

}  // namespace

OzonePlatform::PlatformRuntimeProperties::SupportsForTest
    OzonePlatform::PlatformRuntimeProperties::override_supports_ssd_for_test =
        OzonePlatform::PlatformRuntimeProperties::SupportsForTest::kNotSet;

OzonePlatform::PlatformRuntimeProperties::SupportsForTest OzonePlatform::
    PlatformRuntimeProperties::override_supports_per_window_scaling_for_test =
        OzonePlatform::PlatformRuntimeProperties::SupportsForTest::kNotSet;

OzonePlatform::PlatformProperties::PlatformProperties() = default;
OzonePlatform::PlatformProperties::~PlatformProperties() = default;

OzonePlatform::PlatformRuntimeProperties::PlatformRuntimeProperties() = default;

OzonePlatform::OzonePlatform() {
  DCHECK(!g_instance) << "There should only be a single OzonePlatform.";
  g_instance = this;
}

OzonePlatform::~OzonePlatform() = default;

// static
void OzonePlatform::PreEarlyInitialization() {
  EnsureInstance();
  if (g_instance->prearly_initialized_)
    return;
  g_instance->prearly_initialized_ = true;
  g_instance->PreEarlyInitialize();
}

// static
bool OzonePlatform::InitializeForUI(const InitParams& args) {
  EnsureInstance();
  if (g_instance->initialized_ui_)
    return true;
  g_instance->single_process_ = args.single_process;
  if (!g_instance->InitializeUI(args))
    return false;
  g_instance->initialized_ui_ = true;
  // This is deliberately created after initializing so that the platform can
  // create its own version of DDM.
  DeviceDataManager::CreateInstance();
  return true;
}

// static
void OzonePlatform::InitializeForGPU(const InitParams& args) {
  EnsureInstance();
  if (g_instance->initialized_gpu_)
    return;
  g_instance->initialized_gpu_ = true;
  g_instance->single_process_ = args.single_process;
  g_instance->InitializeGPU(args);
}

// static
OzonePlatform* OzonePlatform::GetInstance() {
  DCHECK(g_instance) << "OzonePlatform is not initialized";
  return g_instance;
}

// static
bool OzonePlatform::IsInitialized() {
  return !!g_instance;
}

// static
std::string OzonePlatform::GetPlatformNameForTest() {
  return GetOzonePlatformName();
}

PlatformClipboard* OzonePlatform::GetPlatformClipboard() {
  // Platforms that support system clipboard must override this method.
  return nullptr;
}

PlatformGLEGLUtility* OzonePlatform::GetPlatformGLEGLUtility() {
  return nullptr;
}

PlatformMenuUtils* OzonePlatform::GetPlatformMenuUtils() {
  return nullptr;
}

PlatformUtils* OzonePlatform::GetPlatformUtils() {
  return nullptr;
}

PlatformGlobalShortcutListener*
OzonePlatform::GetPlatformGlobalShortcutListener(
    PlatformGlobalShortcutListenerDelegate* delegate) {
  return nullptr;
}

std::unique_ptr<PlatformKeyboardHook> OzonePlatform::CreateKeyboardHook(
    PlatformKeyboardHookTypes type,
    base::RepeatingCallback<void(KeyEvent* event)> callback,
    std::optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget) {
  return nullptr;
}

bool OzonePlatform::IsNativePixmapConfigSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) const {
  // Platform that support NativePixmap must override this method.
  return false;
}

bool OzonePlatform::ShouldUseCustomFrame() {
  return GetPlatformProperties().custom_frame_pref_default;
}

const OzonePlatform::PlatformProperties&
OzonePlatform::GetPlatformProperties() {
  static const base::NoDestructor<OzonePlatform::PlatformProperties> properties;
  return *properties;
}

const OzonePlatform::PlatformRuntimeProperties&
OzonePlatform::GetPlatformRuntimeProperties() {
  DCHECK(initialized_ui_ || initialized_gpu_);

  static const PlatformRuntimeProperties properties;
  return properties;
}

void OzonePlatform::AddInterfaces(mojo::BinderMap* binders) {}

void OzonePlatform::AfterSandboxEntry() {
  // This should not be called in single-process mode.
  DCHECK(!single_process_);
}

std::unique_ptr<PlatformUserInputMonitor>
OzonePlatform::GetPlatformUserInputMonitor(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner) {
  return {};
}

void OzonePlatform::PostCreateMainMessageLoop(
    base::OnceCallback<void()> shutdown_cb,
    scoped_refptr<base::SingleThreadTaskRunner> input_event_task_runner) {}

void OzonePlatform::PostMainMessageLoopRun() {}

// static
bool OzonePlatform::ShouldFailInitializeUIForTest() {
  return g_fail_initialize_ui_for_test;
}

// static
void OzonePlatform::SetFailInitializeUIForTest(bool fail) {
  g_fail_initialize_ui_for_test = fail;
}

void OzonePlatform::PreEarlyInitialize() {}

}  // namespace ui
