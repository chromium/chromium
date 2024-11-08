// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/linux/gbm_wrapper.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_thread.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"

namespace ui {

namespace {

void OnBufferCreatedOnDrmThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    DrmThreadProxy::CreateBufferAsyncCallback callback,
    std::unique_ptr<GbmBuffer> buffer,
    scoped_refptr<DrmFramebuffer> framebuffer) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), std::move(buffer),
                                       std::move(framebuffer)));
}

class GbmDeviceGenerator : public DrmDeviceGenerator {
 public:
  GbmDeviceGenerator() = default;

  GbmDeviceGenerator(const GbmDeviceGenerator&) = delete;
  GbmDeviceGenerator& operator=(const GbmDeviceGenerator&) = delete;

  ~GbmDeviceGenerator() override = default;

  // DrmDeviceGenerator:
  scoped_refptr<DrmDevice> CreateDevice(const base::FilePath& path,
                                        base::ScopedFD fd,
                                        bool is_primary_device) override {
    auto gbm = CreateGbmDevice(fd.get());
    if (!gbm) {
      PLOG(ERROR) << "Unable to initialize GBM for " << path.value();
      return nullptr;
    }

    auto drm = base::MakeRefCounted<DrmDevice>(
        path, std::move(fd), is_primary_device, std::move(gbm));
    if (!drm->Initialize())
      return nullptr;
    return drm;
  }
};

}  // namespace

DrmThreadProxy::DrmThreadProxy() = default;

DrmThreadProxy::~DrmThreadProxy() = default;

void DrmThreadProxy::StartDrmThread(base::OnceClosure receiver_drainer) {
  drm_thread_.Start(std::move(receiver_drainer),
                    std::make_unique<GbmDeviceGenerator>());
}

std::unique_ptr<DrmWindowProxy> DrmThreadProxy::CreateDrmWindowProxy(
    gfx::AcceleratedWidget widget) {
  return std::make_unique<DrmWindowProxy>(widget, &drm_thread_);
}

void DrmThreadProxy::CreateBuffer(gfx::AcceleratedWidget widget,
                                  const gfx::Size& size,
                                  const gfx::Size& framebuffer_size,
                                  gfx::BufferFormat format,
                                  gfx::BufferUsage usage,
                                  uint32_t flags,
                                  std::unique_ptr<GbmBuffer>* buffer,
                                  scoped_refptr<DrmFramebuffer>* framebuffer) {
  TRACE_EVENT0("drm", "DrmThreadProxy::CreateBuffer");
  DCHECK(drm_thread_.task_runner())
      << "no task runner! in DrmThreadProxy::CreateBuffer";
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::OnceClosure task = base::BindOnce(
      &DrmThread::CreateBuffer, base::Unretained(&drm_thread_), widget, size,
      framebuffer_size, format, usage, flags, buffer, framebuffer);
  PostSyncTask(drm_thread_.task_runner(),
               base::BindOnce(&DrmThread::RunTaskAfterDeviceReady,
                              base::Unretained(&drm_thread_), std::move(task)));
}

void DrmThreadProxy::CreateBufferAsync(gfx::AcceleratedWidget widget,
                                       const gfx::Size& size,
                                       gfx::BufferFormat format,
                                       gfx::BufferUsage usage,
                                       uint32_t flags,
                                       CreateBufferAsyncCallback callback) {
  DCHECK(drm_thread_.task_runner())
      << "no task runner! in DrmThreadProxy::CreateBufferAsync";
  base::OnceClosure task = base::BindOnce(
      &DrmThread::CreateBufferAsync, base::Unretained(&drm_thread_), widget,
      size, format, usage, flags,
      base::BindOnce(OnBufferCreatedOnDrmThread,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     std::move(callback)));
  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::RunTaskAfterDeviceReady,
                     base::Unretained(&drm_thread_), std::move(task), nullptr));
}

void DrmThreadProxy::CreateBufferFromHandle(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle,
    std::unique_ptr<GbmBuffer>* buffer,
    scoped_refptr<DrmFramebuffer>* framebuffer) {
  TRACE_EVENT0("drm", "DrmThreadProxy::CreateBufferFromHandle");
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::OnceClosure task = base::BindOnce(
      &DrmThread::CreateBufferFromHandle, base::Unretained(&drm_thread_),
      widget, size, format, std::move(handle), buffer, framebuffer);
  PostSyncTask(drm_thread_.task_runner(),
               base::BindOnce(&DrmThread::RunTaskAfterDeviceReady,
                              base::Unretained(&drm_thread_), std::move(task)));
}

void DrmThreadProxy::SetDisplaysConfiguredCallback(
    base::RepeatingClosure callback) {
  DCHECK(drm_thread_.task_runner());

  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::SetDisplaysConfiguredCallback,
                     base::Unretained(&drm_thread_),
                     CreateSafeRepeatingCallback(std::move(callback))));
}

void DrmThreadProxy::CheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlaySurfaceCandidate>& candidates,
    DrmThread::OverlayCapabilitiesCallback callback) {
  DCHECK(drm_thread_.task_runner());
  base::OnceClosure task = base::BindOnce(
      &DrmThread::CheckOverlayCapabilities, base::Unretained(&drm_thread_),
      widget, candidates, CreateSafeOnceCallback(std::move(callback)));

  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::RunTaskAfterDeviceReady,
                     base::Unretained(&drm_thread_), std::move(task), nullptr));
}

std::vector<OverlayStatus> DrmThreadProxy::CheckOverlayCapabilitiesSync(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlaySurfaceCandidate>& candidates) {
  TRACE_EVENT0("drm", "DrmThreadProxy::CheckOverlayCapabilitiesSync");
  DCHECK(drm_thread_.task_runner());
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  std::vector<OverlayStatus> result;
  base::OnceClosure task = base::BindOnce(
      &DrmThread::CheckOverlayCapabilitiesSync, base::Unretained(&drm_thread_),
      widget, candidates, &result);
  PostSyncTask(drm_thread_.task_runner(),
               base::BindOnce(&DrmThread::RunTaskAfterDeviceReady,
                              base::Unretained(&drm_thread_), std::move(task)));
  return result;
}

void DrmThreadProxy::GetHardwareCapabilities(
    gfx::AcceleratedWidget widget,
    const HardwareCapabilitiesCallback& receive_callback) {
  TRACE_EVENT0("drm", "DrmThreadProxy::GetHardwareCapabilities");
  DCHECK(drm_thread_.task_runner());
  base::RepeatingClosure task = base::BindRepeating(
      &DrmThread::GetHardwareCapabilities, base::Unretained(&drm_thread_),
      widget, CreateSafeRepeatingCallback(receive_callback));
  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::RunTaskAfterDeviceReady,
                     base::Unretained(&drm_thread_), std::move(task), nullptr));
}

void DrmThreadProxy::AddDrmDeviceReceiver(
    mojo::PendingReceiver<ozone::mojom::DrmDevice> receiver) {
  DCHECK(drm_thread_.task_runner()) << "DrmThreadProxy::AddDrmDeviceReceiver "
                                       "drm_thread_ task runner missing";

  if (drm_thread_.task_runner()->BelongsToCurrentThread()) {
    drm_thread_.AddDrmDeviceReceiver(std::move(receiver));
  } else {
    drm_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&DrmThread::AddDrmDeviceReceiver,
                       base::Unretained(&drm_thread_), std::move(receiver)));
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
DrmThreadProxy::GetDrmThreadTaskRunner() {
  return drm_thread_.task_runner();
}

bool DrmThreadProxy::WaitUntilDrmThreadStarted() {
  return drm_thread_.WaitUntilThreadStarted();
}

void DrmThreadProxy::SetDrmModifiersFilter(
    std::unique_ptr<DrmModifiersFilter> filter) {
  DCHECK(drm_thread_.task_runner());
  base::OnceClosure task =
      base::BindOnce(&DrmThread::SetDrmModifiersFilter,
                     base::Unretained(&drm_thread_), std::move(filter));
  drm_thread_.task_runner()->PostTask(FROM_HERE, std::move(task));
}

}  // namespace ui
