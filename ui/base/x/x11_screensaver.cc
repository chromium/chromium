// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_screensaver.h"

#include <memory>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/property_cache.h"
#include "ui/gfx/x/screensaver.h"

namespace ui {

namespace {

bool IsMitScreensaverActive(x11::ScreenSaver::State state) {
  switch (state) {
    case x11::ScreenSaver::State::Off:
    case x11::ScreenSaver::State::Disabled:
      return false;
    case x11::ScreenSaver::State::On:
    case x11::ScreenSaver::State::Cycle:
      return true;
  }
  NOTREACHED();
}

class ScreensaverStatusWatcher : public x11::EventObserver {
 public:
  ScreensaverStatusWatcher() {
    auto* connection = x11::Connection::Get();

    x_screensaver_status_ = std::make_unique<x11::PropertyCache>(
        connection, connection->default_root(),
        std::vector<x11::Atom>{x11::GetAtom("_SCREENSAVER_STATUS")});

    connection->AddEventObserver(this);
    connection->screensaver().SelectInput(connection->default_root(),
                                          x11::ScreenSaver::Event::NotifyMask);

    auto reply =
        connection->screensaver().QueryInfo(connection->default_root()).Sync();
    if (reply) {
      mit_screensaver_active_ = IsMitScreensaverActive(
          static_cast<x11::ScreenSaver::State>(reply->state));
    }
  }

  ScreensaverStatusWatcher(const ScreensaverStatusWatcher&) = delete;
  ScreensaverStatusWatcher& operator=(const ScreensaverStatusWatcher&) = delete;

  ~ScreensaverStatusWatcher() override = default;

  bool ScreensaverActive() {
    if (mit_screensaver_active_) {
      return true;
    }

    // Ironically, xscreensaver does not use the MIT-SCREENSAVER extension,
    // so add a special check for xscreensaver.
    auto* status = x_screensaver_status_->GetAs<x11::Atom>(
        x11::GetAtom("_SCREENSAVER_STATUS"));
    return status && *status == x11::GetAtom("LOCK");
  }

 private:
  void OnEvent(const x11::Event& event) override {
    if (auto* notify = event.As<x11::ScreenSaver::NotifyEvent>()) {
      mit_screensaver_active_ = IsMitScreensaverActive(notify->state);
    }
  }

  std::unique_ptr<x11::PropertyCache> x_screensaver_status_;
  bool mit_screensaver_active_ = false;
};

}  // namespace

bool IsXScreensaverActive() {
  // Avoid calling into potentially missing X11 APIs in headless mode.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless)) {
    return false;
  }

  static base::NoDestructor<ScreensaverStatusWatcher> watcher;
  return watcher->ScreensaverActive();
}

}  // namespace ui
