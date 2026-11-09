// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace ui {
namespace fuchsia {
namespace {

FlatlandPresentViewCallback& GetFlatlandViewPresenterInternal() {
  static base::NoDestructor<FlatlandPresentViewCallback> view_presenter;
  return *view_presenter;
}

}  // namespace

void SetFlatlandViewPresenter(FlatlandPresentViewCallback view_presenter) {
  GetFlatlandViewPresenterInternal() = std::move(view_presenter);
}

const FlatlandPresentViewCallback& GetFlatlandViewPresenter() {
  return GetFlatlandViewPresenterInternal();
}

void IgnorePresentCallsForTest() {
  SetFlatlandViewPresenter(base::BindRepeating(
      [](::fuchsia::ui::views::ViewportCreationToken viewport_creation_token)
          -> ::fuchsia::element::ViewControllerPtr {
        CHECK(viewport_creation_token.value, base::NotFatalUntil::M163);
        DVLOG(1) << "Present call ignored for test.";
        return nullptr;
      }));
}

}  // namespace fuchsia
}  // namespace ui
