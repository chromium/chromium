// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_recognizer.h"

#include "ui/events/gestures/gesture_recognizer_observer.h"

namespace ui {

GestureRecognizer::GestureRecognizer() = default;
GestureRecognizer::~GestureRecognizer() = default;

void GestureRecognizer::AddObserver(GestureRecognizerObserver* observer) {
  observers_.AddObserver(observer);
}

void GestureRecognizer::RemoveObserver(GestureRecognizerObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ui
