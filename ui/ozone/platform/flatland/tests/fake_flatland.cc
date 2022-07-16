// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/tests/fake_flatland.h"

#include "base/check.h"
#include "base/logging.h"

namespace ui {

FakeFlatland::FakeFlatland() : binding_(this) {}

FakeFlatland::~FakeFlatland() = default;

fidl::InterfaceHandle<fuchsia::ui::composition::Flatland> FakeFlatland::Connect(
    async_dispatcher_t* dispatcher) {
  CHECK(!binding_.is_bound());

  fidl::InterfaceHandle<fuchsia::ui::composition::Flatland> flatland;
  binding_.Bind(flatland.NewRequest(), dispatcher);

  return flatland;
}

void FakeFlatland::Disconnect(fuchsia::ui::composition::FlatlandError error) {
  binding_.events().OnError(std::move(error));
  binding_.Unbind();
}

void FakeFlatland::SetPresentHandler(PresentHandler present_handler) {
  present_handler_ = std::move(present_handler);
}

void FakeFlatland::FireOnNextFrameBeginEvent(
    fuchsia::ui::composition::OnNextFrameBeginValues
        on_next_frame_begin_values) {
  binding_.events().OnNextFrameBegin(std::move(on_next_frame_begin_values));
}

void FakeFlatland::FireOnFramePresentedEvent(
    fuchsia::scenic::scheduling::FramePresentedInfo frame_presented_info) {
  binding_.events().OnFramePresented(std::move(frame_presented_info));
}

void FakeFlatland::NotImplemented_(const std::string& name) {
  LOG(ERROR) << "FakeFlatland does not implement " << name;
}

void FakeFlatland::Present(fuchsia::ui::composition::PresentArgs args) {
  // TODO(fxb/85619): ApplyCommands()
  present_handler_.Run(std::move(args));
}

void FakeFlatland::SetDebugName(std::string debug_name) {
  debug_name_ = std::move(debug_name);
}

}  // namespace ui
