// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_thread_evdev.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev_proxy.h"

namespace ui {

namespace {

// Internal base::Thread subclass for events thread.
class EvdevThread : public base::Thread {
 public:
  EvdevThread(std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
              CursorDelegateEvdev* cursor,
              EventThreadStartCallback callback)
      : base::Thread("evdev"),
        dispatcher_(std::move(dispatcher)),
        cursor_(cursor),
        init_callback_(std::move(callback)),
        init_runner_(base::ThreadTaskRunnerHandle::Get()) {}
  ~EvdevThread() override { Stop(); }

  void Init() override {
    TRACE_EVENT0("evdev", "EvdevThread::Init");
    input_device_factory_ =
        new InputDeviceFactoryEvdev(std::move(dispatcher_), cursor_);

    std::unique_ptr<InputDeviceFactoryEvdevProxy> proxy(
        new InputDeviceFactoryEvdevProxy(base::ThreadTaskRunnerHandle::Get(),
                                         input_device_factory_->GetWeakPtr()));

    if (cursor_)
      cursor_->InitializeOnEvdev();

    init_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(init_callback_), std::move(proxy)));
  }

  void CleanUp() override {
    TRACE_EVENT0("evdev", "EvdevThread::CleanUp");
    delete input_device_factory_;
  }

 private:
  // Initialization bits passed from main thread.
  std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher_;
  CursorDelegateEvdev* cursor_;
  EventThreadStartCallback init_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> init_runner_;

  // Thread-internal state.
  InputDeviceFactoryEvdev* input_device_factory_ = nullptr;
};

}  // namespace

EventThreadEvdev::EventThreadEvdev() {
}

EventThreadEvdev::~EventThreadEvdev() {
}

void EventThreadEvdev::Start(
    std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
    CursorDelegateEvdev* cursor,
    EventThreadStartCallback callback) {
  TRACE_EVENT0("evdev", "EventThreadEvdev::Start");
  thread_.reset(
      new EvdevThread(std::move(dispatcher), cursor, std::move(callback)));
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::UI;
  thread_options.priority = base::ThreadPriority::DISPLAY;
  if (!thread_->StartWithOptions(thread_options))
    LOG(FATAL) << "Failed to create input thread";
}

}  // namespace ui
