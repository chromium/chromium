// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(views::InkDropHost*)

namespace views {

namespace {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(InkDropHost, kInkDropKey, nullptr)

// TODO(pbos): Remove this by changing the constructor parameters to
// InkDropImpl.
std::unique_ptr<InkDrop> CreateInkDropImpl(
    InkDropHost* host,
    InkDropImpl::AutoHighlightMode auto_highlight_mode,
    bool highlight_on_hover,
    bool highlight_on_focus) {
  auto ink_drop = std::make_unique<InkDropImpl>(host, host->host_view()->size(),
                                                auto_highlight_mode);
  ink_drop->SetShowHighlightOnHover(highlight_on_hover);
  ink_drop->SetShowHighlightOnFocus(highlight_on_focus);
  return ink_drop;
}

}  // namespace

InkDrop::~InkDrop() = default;

InkDropHost* InkDrop::Install(View* host,
                              std::unique_ptr<InkDropHost> ink_drop) {
  return host->SetProperty(kInkDropKey, std::move(ink_drop));
}

void InkDrop::Remove(View* host) {
  host->ClearProperty(kInkDropKey);
}

const InkDropHost* InkDrop::Get(const View* host) {
  return host->GetProperty(kInkDropKey);
}

std::unique_ptr<InkDrop> InkDrop::CreateInkDropForSquareRipple(
    InkDropHost* host,
    bool highlight_on_hover,
    bool highlight_on_focus,
    bool show_highlight_on_ripple) {
  return CreateInkDropImpl(host,
                           show_highlight_on_ripple
                               ? InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE
                               : InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE,
                           highlight_on_hover, highlight_on_focus);
}

void InkDrop::UseInkDropForSquareRipple(InkDropHost* host,
                                        bool highlight_on_hover,
                                        bool highlight_on_focus,
                                        bool show_highlight_on_ripple) {
  host->SetCreateInkDropCallback(base::BindRepeating(
      &InkDrop::CreateInkDropForSquareRipple, host, highlight_on_hover,
      highlight_on_focus, show_highlight_on_ripple));
}

std::unique_ptr<InkDrop> InkDrop::CreateInkDropForFloodFillRipple(
    InkDropHost* host,
    bool highlight_on_hover,
    bool highlight_on_focus,
    bool show_highlight_on_ripple) {
  return CreateInkDropImpl(host,
                           show_highlight_on_ripple
                               ? InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE
                               : InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE,
                           highlight_on_hover, highlight_on_focus);
}

void InkDrop::UseInkDropForFloodFillRipple(InkDropHost* host,
                                           bool highlight_on_hover,
                                           bool highlight_on_focus,
                                           bool show_highlight_on_ripple) {
  host->SetCreateInkDropCallback(base::BindRepeating(
      &InkDrop::CreateInkDropForFloodFillRipple, host, highlight_on_hover,
      highlight_on_focus, show_highlight_on_ripple));
}

std::unique_ptr<InkDrop> InkDrop::CreateInkDropWithoutAutoHighlight(
    InkDropHost* host,
    bool highlight_on_hover,
    bool highlight_on_focus) {
  return CreateInkDropImpl(host, InkDropImpl::AutoHighlightMode::NONE,
                           highlight_on_hover, highlight_on_focus);
}

void InkDrop::UseInkDropWithoutAutoHighlight(InkDropHost* host,
                                             bool highlight_on_hover,
                                             bool highlight_on_focus) {
  host->SetCreateInkDropCallback(
      base::BindRepeating(&InkDrop::CreateInkDropWithoutAutoHighlight, host,
                          highlight_on_hover, highlight_on_focus));
}

void InkDrop::AddObserver(InkDropObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void InkDrop::RemoveObserver(InkDropObserver* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

InkDrop::InkDrop() = default;

void InkDrop::NotifyInkDropAnimationStarted() {
  observers_.Notify(&InkDropObserver::InkDropAnimationStarted);
}

void InkDrop::NotifyInkDropRippleAnimationEnded(InkDropState ink_drop_state) {
  observers_.Notify(&InkDropObserver::InkDropRippleAnimationEnded,
                    ink_drop_state);
}

InkDropContainerView::InkDropContainerView() {
  // Ensure the container View is found as the EventTarget instead of this.
  SetCanProcessEventsWithinSubtree(false);
  SetProperty(kViewIgnoredByLayoutKey, true);
}

InkDropContainerView::~InkDropContainerView() = default;

bool InkDropContainerView::GetAutoMatchParentBounds() const {
  return auto_match_parent_bounds_;
}

void InkDropContainerView::SetAutoMatchParentBounds(
    bool auto_match_parent_bounds) {
  if (auto_match_parent_bounds_ == auto_match_parent_bounds) {
    return;
  }
  auto_match_parent_bounds_ = auto_match_parent_bounds;
  OnPropertyChanged(&auto_match_parent_bounds_,
                    PropertyEffects::kPropertyEffectsNone);
  if (parent()) {
    OnViewBoundsChanged(parent());
  }
}

void InkDropContainerView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.child == this) {
    if (details.is_add) {
      observer_.Observe(details.parent);
    } else {
      observer_.Reset();
    }
  }
}

void InkDropContainerView::OnViewBoundsChanged(View* observed_view) {
  if (!auto_match_parent_bounds_) {
    return;
  }
  gfx::Rect bounds = observed_view->GetLocalBounds();
  SetBoundsRect(bounds);
}

BEGIN_METADATA(InkDropContainerView)
ADD_PROPERTY_METADATA(bool, AutoMatchParentBounds)
END_METADATA

}  // namespace views
