// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

#include "build/chromeos_buildflags.h"

namespace ui {
namespace {

GestureConfiguration* instance = nullptr;

}  // namespace

// static
GestureConfiguration* GestureConfiguration::GetInstance() {
  if (instance)
    return instance;

  return GestureConfiguration::GetPlatformSpecificInstance();
}

GestureConfiguration::GestureConfiguration() = default;

GestureConfiguration::~GestureConfiguration() = default;

}  // namespace ui
