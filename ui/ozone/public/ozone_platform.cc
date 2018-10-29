// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_platform.h"

#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

namespace {

bool g_platform_initialized_ui = false;
bool g_platform_initialized_gpu = false;

OzonePlatform* g_instance = nullptr;

OzonePlatform::StartupCallback& GetInstanceCallback() {
  static base::NoDestructor<OzonePlatform::StartupCallback> callback;
  return *callback;
}

base::Lock& GetOzoneInstanceLock() {
  static base::Lock lock;
  return lock;
}

}  // namespace

OzonePlatform::PlatformProperties::PlatformProperties() = default;

OzonePlatform::PlatformProperties::PlatformProperties(
    bool needs_request,
    bool custom_frame_default,
    bool can_use_system_title_bar,
    bool requires_mojo_for_ipc,
    std::vector<gfx::BufferFormat> buffer_formats)
    : needs_view_owner_request(needs_request),
      custom_frame_pref_default(custom_frame_default),
      use_system_title_bar(can_use_system_title_bar),
      requires_mojo(requires_mojo_for_ipc),
      supported_buffer_formats(buffer_formats) {}

OzonePlatform::PlatformProperties::~PlatformProperties() = default;

OzonePlatform::PlatformProperties::PlatformProperties(
    const PlatformProperties& other) = default;

OzonePlatform::OzonePlatform() {
  GetOzoneInstanceLock().AssertAcquired();
  DCHECK(!g_instance) << "There should only be a single OzonePlatform.";
  g_instance = this;
  g_platform_initialized_ui = false;
  g_platform_initialized_gpu = false;
}

OzonePlatform::~OzonePlatform() = default;

// static
void OzonePlatform::InitializeForUI(const InitParams& args) {
  EnsureInstance();
  if (g_platform_initialized_ui)
    return;
  g_platform_initialized_ui = true;
  g_instance->InitializeUI(args);
  // This is deliberately created after initializing so that the platform can
  // create its own version of DDM.
  DeviceDataManager::CreateInstance();
  auto& instance_callback = GetInstanceCallback();
  if (instance_callback)
    std::move(instance_callback).Run(g_instance);
}

// static
void OzonePlatform::InitializeForGPU(const InitParams& args) {
  EnsureInstance();
  if (g_platform_initialized_gpu)
    return;
  g_platform_initialized_gpu = true;
  g_instance->InitializeGPU(args);
  if (!args.single_process) {
    auto& instance_callback = GetInstanceCallback();
    if (instance_callback)
      std::move(instance_callback).Run(g_instance);
  }
}

// static
OzonePlatform* OzonePlatform::GetInstance() {
  base::AutoLock lock(GetOzoneInstanceLock());
  DCHECK(g_instance) << "OzonePlatform is not initialized";
  return g_instance;
}

// static
OzonePlatform* OzonePlatform::EnsureInstance() {
  base::AutoLock lock(GetOzoneInstanceLock());
  if (!g_instance) {
    TRACE_EVENT1("ozone",
                 "OzonePlatform::Initialize",
                 "platform",
                 GetOzonePlatformName());
    std::unique_ptr<OzonePlatform> platform =
        PlatformObject<OzonePlatform>::Create();

    // TODO(spang): Currently need to leak this object.
    OzonePlatform* pl = platform.release();
    DCHECK_EQ(g_instance, pl);
  }
  return g_instance;
}

// static
void OzonePlatform::RegisterStartupCallback(StartupCallback callback) {
  OzonePlatform* inst = nullptr;
  {
    base::AutoLock lock(GetOzoneInstanceLock());
    if (!g_instance || !g_platform_initialized_ui) {
      auto& instance_callback = GetInstanceCallback();
      instance_callback = std::move(callback);
      return;
    }
    inst = g_instance;
  }
  std::move(callback).Run(inst);
}

IPC::MessageFilter* OzonePlatform::GetGpuMessageFilter() {
  return nullptr;
}

std::unique_ptr<PlatformScreen> OzonePlatform::CreateScreen() {
  return nullptr;
}

const OzonePlatform::PlatformProperties&
OzonePlatform::GetPlatformProperties() {
  static const base::NoDestructor<OzonePlatform::PlatformProperties> properties;
  return *properties;
}

base::MessageLoop::Type OzonePlatform::GetMessageLoopTypeForGpu() {
  return base::MessageLoop::TYPE_DEFAULT;
}

void OzonePlatform::AddInterfaces(service_manager::BinderRegistry* registry) {}

void OzonePlatform::AfterSandboxEntry() {}

}  // namespace ui
