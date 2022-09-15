// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/platform_event_source_test_api.h"

#include "ui/events/platform/platform_event_source.h"

namespace ui {
namespace test {

PlatformEventSourceTestAPI::PlatformEventSourceTestAPI(
    PlatformEventSource* event_source)
    : event_source_(event_source) {}

PlatformEventSourceTestAPI::~PlatformEventSourceTestAPI() {}

void PlatformEventSourceTestAPI::Dispatch(PlatformEvent platform_event) {
  event_source_->DispatchEvent(platform_event);
}

}  // namespace test
}  // namespace ui
