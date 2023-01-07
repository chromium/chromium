// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_PLATFORM_EVENT_SOURCE_TEST_API_H_
#define UI_EVENTS_TEST_PLATFORM_EVENT_SOURCE_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "ui/events/platform_event.h"

namespace ui {

class PlatformEventSource;

namespace test {

class PlatformEventSourceTestAPI {
 public:
  explicit PlatformEventSourceTestAPI(PlatformEventSource* event_source);

  PlatformEventSourceTestAPI(const PlatformEventSourceTestAPI&) = delete;
  PlatformEventSourceTestAPI& operator=(const PlatformEventSourceTestAPI&) =
      delete;

  ~PlatformEventSourceTestAPI();

  void Dispatch(PlatformEvent platform_event);

 private:
  raw_ptr<PlatformEventSource> event_source_;
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_PLATFORM_EVENT_SOURCE_TEST_API_H_
