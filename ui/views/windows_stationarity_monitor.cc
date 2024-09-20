// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/windows_stationarity_monitor.h"

namespace views {

WindowsStationarityMonitor::WindowsStationarityMonitor() = default;

WindowsStationarityMonitor::~WindowsStationarityMonitor() = default;

void WindowsStationarityMonitor::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WindowsStationarityMonitor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WindowsStationarityMonitor::NotifyWindowStationaryStateChanged() {
  observers_.Notify(&Observer::OnWindowStationaryStateChanged);
}

}  // namespace views
