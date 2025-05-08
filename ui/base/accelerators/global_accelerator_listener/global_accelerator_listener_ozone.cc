// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_ozone.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "build/config/linux/dbus/buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/ozone/public/ozone_platform.h"

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
#include "base/environment.h"
#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_linux.h"
#endif

using content::BrowserThread;

namespace {
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
BASE_FEATURE(kGlobalShortcutsPortal,
             "GlobalShortcutsPortal",
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr char kChannelEnvVar[] = "CHROME_VERSION_EXTRA";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kSessionPrefix[] = "chrome";
#else
constexpr char kSessionPrefix[] = "chromium";
#endif

constexpr char kSessionSuffix[] = "_global_shortcuts";

std::string GetSessionPrefixChannel() {
  auto env = base::Environment::Create();
  auto channel = env->GetVar(kChannelEnvVar);
  if (channel == "beta") {
    return "_beta";
  }
  if (channel == "unstable") {
    return "_unstable";
  }
  if (channel == "canary") {
    return "_canary";
  }
  // No suffix for stable. Also if the channel is unknown, the most likely
  // scenario is the user is running the binary directly and not getting the
  // environment variable set, so assume stable to minimize potential risk of
  // settings or data loss.
  return "";
}

std::string GetSessionName() {
  // The session name must not ever change, otherwise user registered
  // shortcuts will be lost.
  return kSessionPrefix + GetSessionPrefixChannel() + kSessionSuffix;
}
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
}  // namespace

namespace ui {

// static
GlobalAcceleratorListener* GlobalAcceleratorListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static const base::NoDestructor<std::unique_ptr<GlobalAcceleratorListener>>
      instance(GlobalAcceleratorListenerOzone::Create());
  if (instance->get()) {
    return instance->get();
  }

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
  if (base::FeatureList::IsEnabled(kGlobalShortcutsPortal)) {
    static GlobalAcceleratorListenerLinux* const linux_instance =
        new GlobalAcceleratorListenerLinux(nullptr, GetSessionName());
    return linux_instance;
  }
#endif

  return nullptr;
}

// static
std::unique_ptr<GlobalAcceleratorListener>
GlobalAcceleratorListenerOzone::Create() {
  auto listener = std::make_unique<GlobalAcceleratorListenerOzone>(
      base::PassKey<GlobalAcceleratorListenerOzone>());
  if (listener->platform_global_shortcut_listener_) {
    return listener;
  }
  return nullptr;
}

GlobalAcceleratorListenerOzone::GlobalAcceleratorListenerOzone(
    base::PassKey<GlobalAcceleratorListenerOzone>) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  platform_global_shortcut_listener_ =
      ui::OzonePlatform::GetInstance()->GetPlatformGlobalShortcutListener(this);
}

GlobalAcceleratorListenerOzone::~GlobalAcceleratorListenerOzone() {
  if (is_listening_) {
    StopListening();
  }

  if (platform_global_shortcut_listener_) {
    platform_global_shortcut_listener_->ResetDelegate();
  }
}

void GlobalAcceleratorListenerOzone::StartListening() {
  DCHECK(!is_listening_);
  DCHECK(!registered_hot_keys_.empty());

  if (platform_global_shortcut_listener_) {
    platform_global_shortcut_listener_->StartListening();
  }

  is_listening_ = true;
}

void GlobalAcceleratorListenerOzone::StopListening() {
  DCHECK(is_listening_);
  DCHECK(registered_hot_keys_.empty());

  if (platform_global_shortcut_listener_) {
    platform_global_shortcut_listener_->StopListening();
  }

  is_listening_ = false;
}

bool GlobalAcceleratorListenerOzone::StartListeningForAccelerator(
    const ui::Accelerator& accelerator) {
  DCHECK(!base::Contains(registered_hot_keys_, accelerator));

  if (!platform_global_shortcut_listener_) {
    return false;
  }

  const bool registered =
      platform_global_shortcut_listener_->RegisterAccelerator(
          accelerator.key_code(), accelerator.IsAltDown(),
          accelerator.IsCtrlDown(), accelerator.IsShiftDown());
  if (registered) {
    registered_hot_keys_.insert(accelerator);
  }
  return registered;
}

void GlobalAcceleratorListenerOzone::StopListeningForAccelerator(
    const ui::Accelerator& accelerator) {
  DCHECK(base::Contains(registered_hot_keys_, accelerator));
  // Otherwise how could the accelerator be registered?
  DCHECK(platform_global_shortcut_listener_);

  platform_global_shortcut_listener_->UnregisterAccelerator(
      accelerator.key_code(), accelerator.IsAltDown(), accelerator.IsCtrlDown(),
      accelerator.IsShiftDown());
  registered_hot_keys_.erase(accelerator);
}

void GlobalAcceleratorListenerOzone::OnKeyPressed(ui::KeyboardCode key_code,
                                                  bool is_alt_down,
                                                  bool is_ctrl_down,
                                                  bool is_shift_down) {
  int modifiers = 0;
  if (is_alt_down) {
    modifiers |= ui::EF_ALT_DOWN;
  }
  if (is_ctrl_down) {
    modifiers |= ui::EF_CONTROL_DOWN;
  }
  if (is_shift_down) {
    modifiers |= ui::EF_SHIFT_DOWN;
  }

  NotifyKeyPressed(ui::Accelerator(key_code, modifiers));
}

void GlobalAcceleratorListenerOzone::OnPlatformListenerDestroyed() {
  platform_global_shortcut_listener_ = nullptr;
}

}  // namespace ui
