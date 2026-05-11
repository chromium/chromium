// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/view_subregion_anchor.h"

#include <functional>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElement1Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElement2Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHostId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSubregionAnchorId);
}  // namespace

class ViewSubregionAnchorTest
    : public test::InteractiveViewsTestMixin<ViewsTestBase> {
 public:
  ViewSubregionAnchorTest() = default;
  ~ViewSubregionAnchorTest() override = default;

  void SetUp() override {
    InteractiveViewsTestMixin::SetUp();
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto* const contents = new View();
    widget_->SetContentsView(contents);
    contents->SetLayoutManager(std::make_unique<FlexLayout>());
    view1_ = contents->AddChildView(new StaticSizedView(gfx::Size(100, 100)));
    view1_->SetProperty(kElementIdentifierKey, kElement1Id);
    view2_ = contents->AddChildView(new StaticSizedView(gfx::Size(100, 100)));
    view2_->SetProperty(kElementIdentifierKey, kElement2Id);
    host_ = view1_->AddChildView(std::make_unique<View>());
    host_->SetProperty(kElementIdentifierKey, kHostId);
    anchor_ = std::make_unique<ViewSubregionAnchor>(kSubregionAnchorId, *host_);
    anchor_->MaybeUpdateAnchor(gfx::Rect(25, 25, 50, 50));
    test::WidgetVisibleWaiter waiter(widget_.get());
    widget_->Show();
    waiter.Wait();
    widget_->LayoutRootViewIfNecessary();
  }

  void TearDown() override {
    view1_ = nullptr;
    view2_ = nullptr;
    host_ = nullptr;
    anchor_.reset();
    widget_.reset();
    InteractiveViewsTestMixin::TearDown();
  }

  ui::ElementContext GetContext() const {
    return ElementTrackerViews::GetContextForWidget(widget_.get());
  }

 protected:
  raw_ptr<View> view1_ = nullptr;
  raw_ptr<View> view2_ = nullptr;
  raw_ptr<View> host_ = nullptr;
  std::unique_ptr<ViewSubregionAnchor> anchor_;
  std::unique_ptr<Widget> widget_;
};

TEST_F(ViewSubregionAnchorTest, VisibilityTracksHost) {
  RunTestSequenceInContext(
      GetContext(), WaitForShow(kSubregionAnchorId),
      Do([this] { host_->SetVisible(false); }), WaitForHide(kSubregionAnchorId),
      Do([this] { host_->SetVisible(true); }), WaitForShow(kSubregionAnchorId));
}

TEST_F(ViewSubregionAnchorTest, VisibilityTracksHierarchy) {
  RunTestSequenceInContext(GetContext(), WaitForShow(kSubregionAnchorId),
                           Do([this] { view1_->SetVisible(false); }),
                           WaitForHide(kSubregionAnchorId),
                           Do([this] { view1_->SetVisible(true); }),
                           WaitForShow(kSubregionAnchorId));
}

TEST_F(ViewSubregionAnchorTest, GetBoundsInScreen) {
  gfx::Rect bounds;
  RunTestSequenceInContext(
      GetContext(),
      WithElement(
          kSubregionAnchorId,
          [&](ui::TrackedElement* el) { bounds = el->GetScreenBounds(); }),
      Check([&, this] { return view1_->GetBoundsInScreen().Contains(bounds); },
            "Anchor bounds inside view bounds."));
}

TEST_F(ViewSubregionAnchorTest, MaybeUpdateAnchor) {
  gfx::Rect original_bounds;
  gfx::Rect new_bounds;
  RunTestSequenceInContext(
      GetContext(),
      WithElement(kSubregionAnchorId,
                  [&](ui::TrackedElement* el) {
                    original_bounds = el->GetScreenBounds();
                  }),
      Do([this] { anchor_->MaybeUpdateAnchor(gfx::Rect(30, 30, 40, 40)); }),
      WaitForEvent(kSubregionAnchorId,
                   ViewSubregionAnchor::kAnchorBoundsChangedEvent),
      WithElement(
          kSubregionAnchorId,
          [&](ui::TrackedElement* el) { new_bounds = el->GetScreenBounds(); }),
      Check([&] { return original_bounds.Contains(new_bounds); },
            "New bounds inside old bonds."));
}

TEST_F(ViewSubregionAnchorTest, MoveHostDirectly) {
  gfx::Rect original_bounds;
  gfx::Rect new_bounds;
  RunTestSequenceInContext(
      GetContext(),
      WithElement(kSubregionAnchorId,
                  [&](ui::TrackedElement* el) {
                    original_bounds = el->GetScreenBounds();
                  }),
      Do([this] { view2_->AddChildViewRaw(host_.get()); }),
      WaitForShow(kSubregionAnchorId),
      WithElement(
          kSubregionAnchorId,
          [&](ui::TrackedElement* el) { new_bounds = el->GetScreenBounds(); }),
      Check([&] { return !original_bounds.Intersects(new_bounds); },
            "Old and new bounds don't overlap."),
      Check([&,
             this] { return view2_->GetBoundsInScreen().Contains(new_bounds); },
            "New anchor bounds inside view bounds."));
}

TEST_F(ViewSubregionAnchorTest, RemoveAndAddHost) {
  gfx::Rect original_bounds;
  gfx::Rect new_bounds;
  RunTestSequenceInContext(
      GetContext(),
      WithElement(kSubregionAnchorId,
                  [&](ui::TrackedElement* el) {
                    original_bounds = el->GetScreenBounds();
                  }),
      Check(
          [&, this] {
            return view1_->GetBoundsInScreen().Contains(original_bounds);
          },
          "Old anchor bounds inside view bounds."),
      Do([this] {
        view2_->AddChildView(view1_->RemoveChildViewT(host_.get()));
      }),
      WaitForShow(kSubregionAnchorId),
      WithElement(
          kSubregionAnchorId,
          [&](ui::TrackedElement* el) { new_bounds = el->GetScreenBounds(); }),
      Check([&] { return !original_bounds.Intersects(new_bounds); },
            "Old and new bounds don't overlap."),
      Check([&,
             this] { return view2_->GetBoundsInScreen().Contains(new_bounds); },
            "New anchor bounds inside view bounds."));
}

TEST_F(ViewSubregionAnchorTest, MoveAnchor) {
  gfx::Rect original_bounds;
  gfx::Rect new_bounds;
  RunTestSequenceInContext(
      GetContext(),
      WithElement(kSubregionAnchorId,
                  [&](ui::TrackedElement* el) {
                    original_bounds = el->GetScreenBounds();
                  }),
      Check(
          [&, this] {
            return view1_->GetBoundsInScreen().Contains(original_bounds);
          },
          "Old anchor bounds inside view bounds."),
      Do([this] { anchor_->MoveTo(*view2_); }),
      WaitForEvent(kSubregionAnchorId,
                   ViewSubregionAnchor::kAnchorBoundsChangedEvent),
      WithElement(
          kSubregionAnchorId,
          [&](ui::TrackedElement* el) { new_bounds = el->GetScreenBounds(); }),
      Check([&] { return !original_bounds.Intersects(new_bounds); },
            "Old and new bounds don't overlap."),
      Check([&] { return original_bounds.size() == new_bounds.size(); },
            "Old and new bounds are same size."),
      Check([&,
             this] { return view2_->GetBoundsInScreen().Contains(new_bounds); },
            "New anchor bounds inside view bounds."));
}

TEST_F(ViewSubregionAnchorTest, MoveAnchorWithNewBounds) {
  gfx::Rect original_bounds;
  gfx::Rect new_bounds;
  RunTestSequenceInContext(
      GetContext(),
      WithElement(kSubregionAnchorId,
                  [&](ui::TrackedElement* el) {
                    original_bounds = el->GetScreenBounds();
                  }),
      Check(
          [&, this] {
            return view1_->GetBoundsInScreen().Contains(original_bounds);
          },
          "Old anchor bounds inside view bounds."),
      Do([this] { anchor_->MoveTo(*view2_, gfx::Rect(30, 30, 40, 40)); }),
      WaitForEvent(kSubregionAnchorId,
                   ViewSubregionAnchor::kAnchorBoundsChangedEvent),
      WithElement(
          kSubregionAnchorId,
          [&](ui::TrackedElement* el) { new_bounds = el->GetScreenBounds(); }),
      Check([&] { return !original_bounds.Intersects(new_bounds); },
            "Old and new bounds don't overlap."),
      Check(
          [&] {
            return original_bounds.width() > new_bounds.width() &&
                   original_bounds.height() > new_bounds.height();
          },
          "New bounds are smaller."),
      Check([&,
             this] { return view2_->GetBoundsInScreen().Contains(new_bounds); },
            "New anchor bounds inside view bounds."));
}

TEST_F(ViewSubregionAnchorTest, MoveAnchorToNotVisibleView) {
  RunTestSequenceInContext(
      GetContext(), Do([this] { view2_->SetVisible(false); }),
      Do([this] { anchor_->MoveTo(*view2_); }), WaitForHide(kSubregionAnchorId),
      Do([this] { view2_->SetVisible(true); }),
      WaitForShow(kSubregionAnchorId));
}

TEST_F(ViewSubregionAnchorTest, MoveAnchorToVisibleView) {
  RunTestSequenceInContext(
      GetContext(), Do([this] { view1_->SetVisible(false); }),
      WaitForHide(kSubregionAnchorId), Do([this] { anchor_->MoveTo(*view2_); }),
      WaitForShow(kSubregionAnchorId));
}

}  // namespace views
