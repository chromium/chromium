// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_platform.h"

#include <memory>
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

OzonePlatform* g_instance = nullptr;

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

OzonePlatform::OzonePlatform() {
  DCHECK(!g_instance) << "There should only be a single OzonePlatform.";
  g_instance = this;
}

OzonePlatform::~OzonePlatform() = default;

// static
void OzonePlatform::InitializeForUI(const InitParams& args) {
  EnsureInstance();
  if (g_instance->initialized_ui_)
    return;
  g_instance->initialized_ui_ = true;
  g_instance->single_process_ = args.single_process;
  g_instance->InitializeUI(args);
  // This is deliberately created after initializing so that the platform can
  // create its own version of DDM.
  DeviceDataManager::CreateInstance();
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
const char* OzonePlatform::GetPlatformName() {
  return GetOzonePlatformName();
}

IPC::MessageFilter* OzonePlatform::GetGpuMessageFilter() {
  return nullptr;
}

PlatformClipboard* OzonePlatform::GetPlatformClipboard() {
  // Platforms that support system clipboard must override this method.
  return nullptr;
}

bool OzonePlatform::IsNativePixmapConfigSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) const {
  // Platform that support NativePixmap must override this method.
  return false;
}

const OzonePlatform::PlatformProperties&
OzonePlatform::GetPlatformProperties() {
  static const base::NoDestructor<OzonePlatform::PlatformProperties> properties;
  return *properties;
}

const OzonePlatform::InitializedHostProperties&
OzonePlatform::GetInitializedHostProperties() {
  DCHECK(initialized_ui_);

  static InitializedHostProperties host_properties;
  return host_properties;
}

void OzonePlatform::AddInterfaces(mojo::BinderMap* binders) {}

void OzonePlatform::AfterSandboxEntry() {
  // This should not be called in single-process mode.
  DCHECK(!single_process_);
}

}  // namespace ui
