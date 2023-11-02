// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/platform_event_source.h"

#include <algorithm>
#include <ostream>

#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "base/threading/thread_local.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/scoped_event_dispatcher.h"

namespace ui {

namespace {

// PlatformEventSource singleton is thread local so that different instances
// can be used on different threads (e.g. browser thread should be able to
// access PlatformEventSource owned by the UI Service's thread).
base::LazyInstance<base::ThreadLocalPointer<PlatformEventSource>>::Leaky
    lazy_tls_ptr = LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool PlatformEventSource::ignore_native_platform_events_ = false;

PlatformEventSource::PlatformEventSource()
    : overridden_dispatcher_(nullptr), overridden_dispatcher_restored_(false) {
  CHECK(!lazy_tls_ptr.Pointer()->Get())
      << "Only one platform event source can be created.";
  lazy_tls_ptr.Pointer()->Set(this);
}

PlatformEventSource::~PlatformEventSource() {
  CHECK_EQ(this, lazy_tls_ptr.Pointer()->Get());
  lazy_tls_ptr.Pointer()->Set(nullptr);
}

PlatformEventSource* PlatformEventSource::GetInstance() {
  return lazy_tls_ptr.Pointer()->Get();
}

bool PlatformEventSource::ShouldIgnoreNativePlatformEvents() {
  return ignore_native_platform_events_;
}

void PlatformEventSource::SetIgnoreNativePlatformEvents(bool ignore_events) {
  ignore_native_platform_events_ = ignore_events;
}

void PlatformEventSource::AddPlatformEventDispatcher(
    PlatformEventDispatcher* dispatcher) {
  CHECK(dispatcher);
  dispatchers_.AddObserver(dispatcher);
  OnDispatcherListChanged();
}

void PlatformEventSource::RemovePlatformEventDispatcher(
    PlatformEventDispatcher* dispatcher) {
  dispatchers_.RemoveObserver(dispatcher);
  OnDispatcherListChanged();
}

std::unique_ptr<ScopedEventDispatcher> PlatformEventSource::OverrideDispatcher(
    PlatformEventDispatcher* dispatcher) {
  CHECK(dispatcher);
  overridden_dispatcher_restored_ = false;
  return std::make_unique<ScopedEventDispatcher>(&overridden_dispatcher_,
                                                 dispatcher);
}

void PlatformEventSource::AddPlatformEventObserver(
    PlatformEventObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void PlatformEventSource::RemovePlatformEventObserver(
    PlatformEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

uint32_t PlatformEventSource::DispatchEvent(PlatformEvent platform_event) {
  uint32_t action = POST_DISPATCH_PERFORM_DEFAULT;

  for (PlatformEventObserver& observer : observers_)
    observer.WillProcessEvent(platform_event);
  // Give the overridden dispatcher a chance to dispatch the event first.
  if (overridden_dispatcher_)
    action = overridden_dispatcher_->DispatchEvent(platform_event);

  if (action & POST_DISPATCH_PERFORM_DEFAULT) {
    for (PlatformEventDispatcher& dispatcher : dispatchers_) {
      if (dispatcher.CanDispatchEvent(platform_event))
        action = dispatcher.DispatchEvent(platform_event);
      if (action & POST_DISPATCH_STOP_PROPAGATION)
        break;
    }
  }
  for (PlatformEventObserver& observer : observers_)
    observer.DidProcessEvent(platform_event);

  overridden_dispatcher_restored_ = false;

  return action;
}

void PlatformEventSource::OnDispatcherListChanged() {
}

void PlatformEventSource::OnOverriddenDispatcherRestored() {
  CHECK(overridden_dispatcher_);
  overridden_dispatcher_restored_ = true;
}

}  // namespace ui
