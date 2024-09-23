// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_FUCHSIA_INITIALIZE_PRESENTER_API_VIEW_H_
#define UI_PLATFORM_WINDOW_FUCHSIA_INITIALIZE_PRESENTER_API_VIEW_H_

#include <fuchsia/element/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {
namespace fuchsia {

using ScenicPresentViewCallback =
    base::RepeatingCallback<::fuchsia::element::ViewControllerPtr(
        ::fuchsia::ui::views::ViewHolderToken,
        ::fuchsia::ui::views::ViewRef)>;

using FlatlandPresentViewCallback =
    base::RepeatingCallback<::fuchsia::element::ViewControllerPtr(
        ::fuchsia::ui::views::ViewportCreationToken)>;

// Register and exposes an API that let OzonePlatformScenic present new views.
// TODO(crbug.com/42050332): Once workstation offers the right FIDL API to open
// new windows, this can be removed.
COMPONENT_EXPORT(PLATFORM_WINDOW)
void SetScenicViewPresenter(ScenicPresentViewCallback view_presenter);

COMPONENT_EXPORT(PLATFORM_WINDOW)
const ScenicPresentViewCallback& GetScenicViewPresenter();

// Register and exposes an API that let OzonePlatformFlatland present new views.
COMPONENT_EXPORT(PLATFORM_WINDOW)
void SetFlatlandViewPresenter(FlatlandPresentViewCallback view_presenter);

COMPONENT_EXPORT(PLATFORM_WINDOW)
const FlatlandPresentViewCallback& GetFlatlandViewPresenter();

// Ignores presentation requests, for tests which don't rely on a functioning
// Presenter service.
COMPONENT_EXPORT(PLATFORM_WINDOW) void IgnorePresentCallsForTest();

}  // namespace fuchsia
}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_FUCHSIA_INITIALIZE_PRESENTER_API_VIEW_H_
