// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <zircon/rights.h>

#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/no_destructor.h"

namespace ui {
namespace fuchsia {
namespace {

ScenicPresentViewCallback& GetScenicViewPresenterInternal() {
  static base::NoDestructor<ScenicPresentViewCallback> view_presenter;
  return *view_presenter;
}

FlatlandPresentViewCallback& GetFlatlandViewPresenterInternal() {
  static base::NoDestructor<FlatlandPresentViewCallback> view_presenter;
  return *view_presenter;
}

::fuchsia::ui::views::ViewRef CloneViewRef(
    const ::fuchsia::ui::views::ViewRef& view_ref) {
  ::fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}

}  // namespace

void InitializeViewTokenAndPresentView(
    ui::PlatformWindowInitProperties* window_properties_out) {
  DCHECK(window_properties_out);

  // Generate ViewToken and ViewHolderToken for the new view.
  auto view_tokens = scenic::ViewTokenPair::New();
  window_properties_out->view_token = std::move(view_tokens.view_token);

  // Create a ViewRefPair so the view can be registered to the SemanticsManager.
  window_properties_out->view_ref_pair = scenic::ViewRefPair::New();

  // Request Presenter to show the view full-screen.
  auto presenter = base::ComponentContextForProcess()
                       ->svc()
                       ->Connect<::fuchsia::element::GraphicalPresenter>();

  ::fuchsia::element::ViewSpec view_spec;
  view_spec.set_view_holder_token(std::move(view_tokens.view_holder_token));
  view_spec.set_view_ref(
      CloneViewRef(window_properties_out->view_ref_pair.view_ref));
  presenter->PresentView(std::move(view_spec), nullptr,
                         window_properties_out->view_controller.NewRequest(),
                         [](auto) {});
}

void SetScenicViewPresenter(ScenicPresentViewCallback view_presenter) {
  GetScenicViewPresenterInternal() = std::move(view_presenter);
}

const ScenicPresentViewCallback& GetScenicViewPresenter() {
  return GetScenicViewPresenterInternal();
}

void SetFlatlandViewPresenter(FlatlandPresentViewCallback view_presenter) {
  GetFlatlandViewPresenterInternal() = std::move(view_presenter);
}

const FlatlandPresentViewCallback& GetFlatlandViewPresenter() {
  return GetFlatlandViewPresenterInternal();
}

void IgnorePresentCallsForTest() {
  SetScenicViewPresenter(
      base::BindRepeating([](::fuchsia::ui::views::ViewHolderToken view_holder,
                             ::fuchsia::ui::views::ViewRef view_ref)
                              -> ::fuchsia::element::ViewControllerPtr {
        DCHECK(view_holder.value);
        DCHECK(view_ref.reference);
        DVLOG(1) << "Present call ignored for test.";
        return nullptr;
      }));
  SetFlatlandViewPresenter(base::BindRepeating(
      [](::fuchsia::ui::views::ViewportCreationToken viewport_creation_token)
          -> ::fuchsia::element::ViewControllerPtr {
        DCHECK(viewport_creation_token.value);
        DVLOG(1) << "Present call ignored for test.";
        return nullptr;
      }));
}

}  // namespace fuchsia
}  // namespace ui
