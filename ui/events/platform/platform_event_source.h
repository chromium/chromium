// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_PLATFORM_EVENT_SOURCE_H_
#define UI_EVENTS_PLATFORM_PLATFORM_EVENT_SOURCE_H_

#include <stdint.h>

#include <memory>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/observer_list.h"
#include "ui/events/events_export.h"
#include "ui/events/platform_event.h"

namespace ui {

class PlatformEventDispatcher;
class PlatformEventObserver;
class ScopedEventDispatcher;

// PlatformEventSource receives events from a source and dispatches the events
// to the appropriate dispatchers.
class EVENTS_EXPORT PlatformEventSource {
 public:
  PlatformEventSource(const PlatformEventSource&) = delete;
  PlatformEventSource& operator=(const PlatformEventSource&) = delete;

  virtual ~PlatformEventSource();

  // Returns the thread-local singleton.
  static PlatformEventSource* GetInstance();

  // Returns true when platform events should not be sent to the rest of the
  // pipeline. Mainly when Chrome is run in a test environment and it doesn't
  // expect any events from the platform and all events are synthesized by the
  // test environment.
  static bool ShouldIgnoreNativePlatformEvents();

  // Sets whether to ignore platform events and drop them or to forward them to
  // the rest of the input pipeline.
  static void SetIgnoreNativePlatformEvents(bool ignore_events);

  // Adds a dispatcher to the dispatcher list. If a dispatcher is added during
  // dispatching an event, then the newly added dispatcher also receives that
  // event.
  void AddPlatformEventDispatcher(PlatformEventDispatcher* dispatcher);

  // Removes a dispatcher from the dispatcher list. Dispatchers can safely be
  // removed from the dispatcher list during an event is being dispatched,
  // without affecting the dispatch of the event to other existing dispatchers.
  void RemovePlatformEventDispatcher(PlatformEventDispatcher* dispatcher);

  // Installs a PlatformEventDispatcher that receives all the events. The
  // dispatcher can process the event, or request that the default dispatchers
  // be invoked by setting |POST_DISPATCH_PERFORM_DEFAULT| flag from the
  // |DispatchEvent()| override.
  // The returned |ScopedEventDispatcher| object is a handler for the overridden
  // dispatcher. When this handler is destroyed, it removes the overridden
  // dispatcher, and restores the previous override-dispatcher (or NULL if there
  // wasn't any).
  std::unique_ptr<ScopedEventDispatcher> OverrideDispatcher(
      PlatformEventDispatcher* dispatcher);

  void AddPlatformEventObserver(PlatformEventObserver* observer);
  void RemovePlatformEventObserver(PlatformEventObserver* observer);

  // Creates PlatformEventSource and sets it as a thread-local singleton.
  static std::unique_ptr<PlatformEventSource> CreateDefault();

  virtual void ResetStateForTesting() {}

 protected:
  typedef base::ObserverList<PlatformEventObserver>::Unchecked
      PlatformEventObserverList;

  PlatformEventSource();

  // Dispatches |platform_event| to the dispatchers. If there is an override
  // dispatcher installed using |OverrideDispatcher()|, then that dispatcher
  // receives the event first. |POST_DISPATCH_QUIT_LOOP| flag is set in the
  // returned value if the event-source should stop dispatching events at the
  // current message-loop iteration.
  virtual uint32_t DispatchEvent(PlatformEvent platform_event);

  PlatformEventObserverList& observers() { return observers_; }

 private:
  friend class ScopedEventDispatcher;

  // Use a base::ObserverList<> instead of an std::vector<> to store the list of
  // dispatchers, so that adding/removing dispatchers during an event dispatch
  // is well-defined.
  using PlatformEventDispatcherList =
      base::ObserverList<PlatformEventDispatcher>::Unchecked;

  // This is invoked when the list of dispatchers changes (i.e. a new dispatcher
  // is added, or a dispatcher is removed).
  virtual void OnDispatcherListChanged();

  void OnOverriddenDispatcherRestored();

  const base::AutoReset<PlatformEventSource*> resetter_;

  PlatformEventDispatcherList dispatchers_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION PlatformEventDispatcher* overridden_dispatcher_;

  // Used to keep track of whether the current override-dispatcher has been
  // reset and a previous override-dispatcher has been restored.
  bool overridden_dispatcher_restored_;

  static bool ignore_native_platform_events_;

  PlatformEventObserverList observers_;
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_PLATFORM_EVENT_SOURCE_H_
