// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "ui/ozone/common/linux/gbm_wrapper.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_message_proxy.h"
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
  GbmDeviceGenerator() {}
  ~GbmDeviceGenerator() override {}

  // DrmDeviceGenerator:
  scoped_refptr<DrmDevice> CreateDevice(const base::FilePath& path,
                                        base::File file,
                                        bool is_primary_device) override {
    auto gbm = CreateGbmDevice(file.GetPlatformFile());
    if (!gbm) {
      PLOG(ERROR) << "Unable to initialize GBM for " << path.value();
      return nullptr;
    }

    auto drm = base::MakeRefCounted<DrmDevice>(
        path, std::move(file), is_primary_device, std::move(gbm));
    if (!drm->Initialize())
      return nullptr;
    return drm;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GbmDeviceGenerator);
};

}  // namespace

DrmThreadProxy::DrmThreadProxy() = default;

DrmThreadProxy::~DrmThreadProxy() = default;

// Used only with the paramtraits implementation.
void DrmThreadProxy::BindThreadIntoMessagingProxy(
    InterThreadMessagingProxy* messaging_proxy) {
  messaging_proxy->SetDrmThread(&drm_thread_);
}

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
                                  gfx::BufferFormat format,
                                  gfx::BufferUsage usage,
                                  uint32_t flags,
                                  std::unique_ptr<GbmBuffer>* buffer,
                                  scoped_refptr<DrmFramebuffer>* framebuffer) {
  DCHECK(drm_thread_.task_runner())
      << "no task runner! in DrmThreadProxy::CreateBuffer";
  base::OnceClosure task =
      base::BindOnce(&DrmThread::CreateBuffer, base::Unretained(&drm_thread_),
                     widget, size, format, usage, flags, buffer, framebuffer);
  PostSyncTask(
      drm_thread_.task_runner(),
      base::BindOnce(&DrmThread::RunTaskAfterWindowReady,
                     base::Unretained(&drm_thread_), widget, std::move(task)));
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
                     base::ThreadTaskRunnerHandle::Get(), std::move(callback)));
  // Since browser's UI thread blocks until a buffer is returned, we shouldn't
  // block on |widget| because a blocked UI thread cannot register |widget| and
  // causes a deadlock. We still want to block on a graphics device, though.
  // TODO(samans): Remove this hack once OOP-D launches.
  gfx::AcceleratedWidget blocking_widget = gfx::kNullAcceleratedWidget;
  drm_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DrmThread::RunTaskAfterWindowReady,
                                base::Unretained(&drm_thread_), blocking_widget,
                                std::move(task), nullptr));
}

void DrmThreadProxy::CreateBufferFromHandle(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle,
    std::unique_ptr<GbmBuffer>* buffer,
    scoped_refptr<DrmFramebuffer>* framebuffer) {
  base::OnceClosure task = base::BindOnce(
      &DrmThread::CreateBufferFromHandle, base::Unretained(&drm_thread_),
      widget, size, format, std::move(handle), buffer, framebuffer);
  PostSyncTask(
      drm_thread_.task_runner(),
      base::BindOnce(&DrmThread::RunTaskAfterWindowReady,
                     base::Unretained(&drm_thread_), widget, std::move(task)));
}

void DrmThreadProxy::SetClearOverlayCacheCallback(
    base::RepeatingClosure callback) {
  DCHECK(drm_thread_.task_runner());

  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::SetClearOverlayCacheCallback,
                     base::Unretained(&drm_thread_),
                     CreateSafeRepeatingCallback(std::move(callback))));
}

void DrmThreadProxy::CheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlaySurfaceCandidate>& candidates,
    OverlayCapabilitiesCallback callback) {
  DCHECK(drm_thread_.task_runner());
  base::OnceClosure task = base::BindOnce(
      &DrmThread::CheckOverlayCapabilities, base::Unretained(&drm_thread_),
      widget, candidates, CreateSafeOnceCallback(std::move(callback)));

  drm_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DrmThread::RunTaskAfterWindowReady,
                                base::Unretained(&drm_thread_), widget,
                                std::move(task), nullptr));
}

void DrmThreadProxy::AddDrmDeviceReceiver(
    mojo::PendingReceiver<ozone::mojom::DrmDevice> receiver) {
  DCHECK(drm_thread_.task_runner()) << "DrmThreadProxy::AddDrmDeviceReceiver "
                                       "drm_thread_ task runner missing";

  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::AddDrmDeviceReceiver,
                     base::Unretained(&drm_thread_), std::move(receiver)));
}

}  // namespace ui
