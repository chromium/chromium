// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_gpu_test_helper.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

namespace {
const int kGpuProcessHostId = 1;
}  // namespace

class FakeGpuConnection {
 public:
  FakeGpuConnection(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& gpu_io_task_runner)
      : ui_task_runner_(ui_task_runner),
        gpu_io_task_runner_(gpu_io_task_runner) {}
  ~FakeGpuConnection() {}

  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) {
    mojo::GenericPendingReceiver receiver =
        mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe));
    CHECK(binders_.TryBind(&receiver))
        << "Unable to find mojo interface " << interface_name;
  }

  void InitOnIO() {
    ui::OzonePlatform::GetInstance()->AddInterfaces(&binders_);
    auto interface_binder = base::BindRepeating(
        &FakeGpuConnection::BindInterface, base::Unretained(this));
    ui::OzonePlatform::GetInstance()
        ->GetGpuPlatformSupportHost()
        ->OnGpuServiceLaunched(kGpuProcessHostId, ui_task_runner_,
                               gpu_io_task_runner_, interface_binder,
                               base::DoNothing());
  }

 private:
  mojo::BinderMap binders_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_io_task_runner_;
};

OzoneGpuTestHelper::OzoneGpuTestHelper() {
}

OzoneGpuTestHelper::~OzoneGpuTestHelper() {
}

bool OzoneGpuTestHelper::Initialize(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
  io_helper_thread_ = std::make_unique<base::Thread>("IOHelperThread");
  if (!io_helper_thread_->StartWithOptions(
          base::Thread::Options(base::MessagePumpType::IO, 0)))
    return false;

  fake_gpu_connection_ = std::make_unique<FakeGpuConnection>(
      ui_task_runner, io_helper_thread_->task_runner());
  io_helper_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeGpuConnection::InitOnIO,
                                base::Unretained(fake_gpu_connection_.get())));
  io_helper_thread_->FlushForTesting();

  // Give the UI thread a chance to run any tasks posted from the IO thread
  // after the GPU process is launched. This is needed for Ozone DRM, see
  // https://crbug.com/830233.
  base::RunLoop run_loop;
  ui_task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  return true;
}

}  // namespace ui
