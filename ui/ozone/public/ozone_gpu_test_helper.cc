// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_gpu_test_helper.h"

#include "base/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ipc/message_filter.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

namespace {

const int kGpuProcessHostId = 1;

void DispatchToGpuPlatformSupportHostTask(IPC::Message* msg) {
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnMessageReceived(*msg);
  delete msg;
}

void DispatchToGpuPlatformSupportTaskOnIO(IPC::Message* msg) {
  IPC::MessageFilter* filter =
      ui::OzonePlatform::GetInstance()->GetGpuMessageFilter();
  if (filter)
    filter->OnMessageReceived(*msg);
  delete msg;
}

}  // namespace

class FakeGpuProcess : public IPC::Channel {
 public:
  FakeGpuProcess(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : ui_task_runner_(ui_task_runner) {}
  ~FakeGpuProcess() override {}

  void InitOnIO() {
    IPC::MessageFilter* filter =
        ui::OzonePlatform::GetInstance()->GetGpuMessageFilter();

    if (filter)
      filter->OnFilterAdded(this);
  }

  // IPC::Channel implementation:
  bool Send(IPC::Message* msg) override {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DispatchToGpuPlatformSupportHostTask, msg));
    return true;
  }

  bool Connect() override {
    NOTREACHED();
    return false;
  }

  void Close() override { NOTREACHED(); }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
};

class FakeGpuProcessHost {
 public:
  FakeGpuProcessHost(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& gpu_io_task_runner)
      : ui_task_runner_(ui_task_runner),
        gpu_io_task_runner_(gpu_io_task_runner) {}
  ~FakeGpuProcessHost() {}

  void InitOnIO() {
    base::RepeatingCallback<void(IPC::Message*)> sender =
        base::BindRepeating(&DispatchToGpuPlatformSupportTaskOnIO);

    ui::OzonePlatform::GetInstance()
        ->GetGpuPlatformSupportHost()
        ->OnGpuProcessLaunched(kGpuProcessHostId, ui_task_runner_,
                               gpu_io_task_runner_, std::move(sender));
  }

 private:
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

  fake_gpu_process_ = std::make_unique<FakeGpuProcess>(ui_task_runner);
  io_helper_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeGpuProcess::InitOnIO,
                                base::Unretained(fake_gpu_process_.get())));

  fake_gpu_process_host_ = std::make_unique<FakeGpuProcessHost>(
      ui_task_runner, io_helper_thread_->task_runner());
  io_helper_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeGpuProcessHost::InitOnIO,
                     base::Unretained(fake_gpu_process_host_.get())));
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
