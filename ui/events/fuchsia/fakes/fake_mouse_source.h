// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_FUCHSIA_FAKES_FAKE_MOUSE_SOURCE_H_
#define UI_EVENTS_FUCHSIA_FAKES_FAKE_MOUSE_SOURCE_H_

#include <fuchsia/ui/pointer/cpp/fidl.h>

#include <vector>

namespace ui {

// A test stub to act as the protocol server. A test can control what is sent
// back by this server implementation, via the ScheduleCallback call.
class FakeMouseSource : public fuchsia::ui::pointer::MouseSource {
 public:
  FakeMouseSource();
  ~FakeMouseSource() override;

  // |fuchsia.ui.pointer.MouseSource|
  void Watch(MouseSource::WatchCallback callback) override;

  // Have the server issue events to the client's hanging-get Watch call.
  void ScheduleCallback(std::vector<fuchsia::ui::pointer::MouseEvent> events);

 private:
  // Client-side logic to invoke on Watch() call's return. A test triggers it
  // with ScheduleCallback().
  MouseSource::WatchCallback callback_;
};

}  // namespace ui

#endif  // UI_EVENTS_FUCHSIA_FAKES_FAKE_MOUSE_SOURCE_H_
