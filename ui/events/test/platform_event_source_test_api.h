// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_PLATFORM_EVENT_SOURCE_TEST_API_H_
#define UI_EVENTS_TEST_PLATFORM_EVENT_SOURCE_TEST_API_H_

#include "base/macros.h"
#include "ui/events/platform_event.h"

namespace ui {

class PlatformEventSource;

namespace test {

class PlatformEventSourceTestAPI {
 public:
  explicit PlatformEventSourceTestAPI(PlatformEventSource* event_source);
  ~PlatformEventSourceTestAPI();

  void Dispatch(PlatformEvent platform_event);

 private:
  PlatformEventSource* event_source_;

  DISALLOW_COPY_AND_ASSIGN(PlatformEventSourceTestAPI);
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_PLATFORM_EVENT_SOURCE_TEST_API_H_
