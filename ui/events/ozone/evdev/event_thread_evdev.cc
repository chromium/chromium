// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_thread_evdev.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/input_controller_evdev.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev_proxy.h"
#include "ui/events/ozone/evdev/input_device_opener_evdev.h"

namespace ui {

namespace {

// Internal base::Thread subclass for events thread.
class EvdevThread : public base::Thread {
 public:
  EvdevThread(std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
              CursorDelegateEvdev* cursor,
              EventThreadStartCallback callback,
              InputControllerEvdev* input_controller)
      : base::Thread("evdev"),
        dispatcher_(std::move(dispatcher)),
        cursor_(cursor),
        init_callback_(std::move(callback)),
        init_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        input_controller_(input_controller) {}
  ~EvdevThread() override { Stop(); }

  void Init() override {
    TRACE_EVENT0("evdev", "EvdevThread::Init");
    input_device_factory_ = std::make_unique<InputDeviceFactoryEvdev>(
        std::move(dispatcher_), cursor_,
        std::make_unique<InputDeviceOpenerEvdev>(), input_controller_);

    auto proxy = std::make_unique<InputDeviceFactoryEvdevProxy>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        input_device_factory_->GetWeakPtr());

    if (cursor_)
      cursor_->InitializeOnEvdev();

    init_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(init_callback_), std::move(proxy)));
  }

  void CleanUp() override {
    TRACE_EVENT0("evdev", "EvdevThread::CleanUp");
    input_device_factory_.reset();
  }

 private:
  // Initialization bits passed from main thread.
  std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher_;
  raw_ptr<CursorDelegateEvdev> cursor_;
  EventThreadStartCallback init_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> init_runner_;
  raw_ptr<InputControllerEvdev> input_controller_;

  // Thread-internal state.
  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_;
};

}  // namespace

EventThreadEvdev::EventThreadEvdev() {
}

EventThreadEvdev::~EventThreadEvdev() {
}

void EventThreadEvdev::Start(
    std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
    CursorDelegateEvdev* cursor,
    EventThreadStartCallback callback,
    InputControllerEvdev* input_controller) {
  TRACE_EVENT0("evdev", "EventThreadEvdev::Start");
  thread_ = std::make_unique<EvdevThread>(
      std::move(dispatcher), cursor, std::move(callback), input_controller);
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::UI;
  thread_options.thread_type = base::ThreadType::kDisplayCritical;
  if (!thread_->StartWithOptions(std::move(thread_options)))
    LOG(FATAL) << "Failed to create input thread";
}

}  // namespace ui
