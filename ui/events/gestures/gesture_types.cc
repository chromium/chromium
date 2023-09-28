// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_types.h"
#include "ui/events/gestures/gesture_provider_aura.h"

namespace ui {

GestureConsumer::GestureConsumer() = default;
GestureConsumer::~GestureConsumer() = default;

bool GestureConsumer::RequiresDoubleTapGestureEvents() const {
  return false;
}

const std::string& GestureConsumer::GetName() const {
  static const std::string name("GestureConsumer");
  return name;
}

base::WeakPtr<GestureConsumer> GestureConsumer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<GestureProviderAura> GestureConsumer::TakeProvider() {
  return std::move(provider_);
}

void GestureConsumer::reset_gesture_provider() {
  provider_.reset();
}

void GestureConsumer::set_gesture_provider(
    std::unique_ptr<GestureProviderAura> provider) {
  provider_ = std::move(provider);
  if (provider_) {
    provider_->set_gesture_consumer(this);
  }
}

}  // namespace ui
