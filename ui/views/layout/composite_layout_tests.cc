// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

// This test suite simulates the kind of nested layouts we find in e.g. the
// toolbar without actually pulling in browser code. It's designed to test
// interactions between multiple levels of Flex and Animating layouts, in a
// situation that resembles how they are actually used.
//
// The test cases are designed to probe edge cases and interactions that are
// difficult to simulate in either the FlexLayout or AnimatingLayoutManager unit
// tests. They are not browser tests however, and uses TaskEnvironment and
// AnimationContainerTestApi to step animations rather than running them in
// realtime. This makes these tests as quick as unit tests, so they do not incur
// the costs associated with browser tests.
//
// This suite is part of views_unittests.

namespace views {

namespace {

constexpr base::TimeDelta kDefaultAnimationDuration = base::Seconds(1);
constexpr int kIconDimension = 20;
constexpr gfx::Size kIconSize(kIconDimension, kIconDimension);
constexpr int kLabelWidth = 70;
constexpr gfx::Size kLabelSize(kLabelWidth, kIconDimension);
constexpr int kBarMinimumWidth = 70;
constexpr int kBarPreferredWidth = 200;
constexpr gfx::Size kBarMinimumSize(kBarMinimumWidth, kIconDimension);
constexpr gfx::Size kBarPreferredSize(kBarPreferredWidth, kIconDimension);
constexpr gfx::Size kDefaultToolbarSize(400, kIconDimension);

// Base class for elements in the toolbar that animate; a stand-in for e.g.
// ToolbarIconContainer.
class SimulatedToolbarElement : public View {
  METADATA_HEADER(SimulatedToolbarElement, View)

 public:
  AnimatingLayoutManager* layout() {
    return static_cast<AnimatingLayoutManager*>(GetLayoutManager());
  }
  const AnimatingLayoutManager* layout() const {
    return static_cast<const AnimatingLayoutManager*>(GetLayoutManager());
  }

 protected:
  SimulatedToolbarElement() {
    auto* const animating_layout =
        SetLayoutManager(std::make_unique<AnimatingLayoutManager>());
    animating_layout
        ->SetBoundsAnimationMode(
            AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis)
        .SetOrientation(LayoutOrientation::kHorizontal)
        .SetAnimationDuration(kDefaultAnimationDuration);
    animating_layout->SetTargetLayoutManager(std::make_unique<FlexLayout>())
        ->SetOrientation(LayoutOrientation::kHorizontal);
  }

  void SetAnimationDuration(base::TimeDelta animation_duration) {
    layout()->SetAnimationDuration(animation_duration);
  }

  FlexLayout* target_layout() {
    return static_cast<FlexLayout*>(layout()->target_layout_manager());
  }
};

BEGIN_METADATA(SimulatedToolbarElement)
END_METADATA

// Simulates an avatar button on the Chrome toolbar, with a fixed-size icon and
// a label that can animate in and out.
class SimulatedAvatarButton : public SimulatedToolbarElement {
  METADATA_HEADER(SimulatedAvatarButton, SimulatedToolbarElement)

 public:
  SimulatedAvatarButton() {
    AddChildView(std::make_unique<StaticSizedView>(kIconSize));
    auto* const status =
        AddChildView(std::make_unique<StaticSizedView>(kLabelSize));
    status->SetVisible(false);
    layout()->SetDefaultFadeMode(
        AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  }

  ~SimulatedAvatarButton() override = default;

  void FadeLabelIn() {
    layout()->FadeIn(label());
    showing_label_ = true;
  }

  void FadeLabelOut() {
    layout()->FadeOut(label());
    showing_label_ = false;
  }

  // Verifies that the label appears (or does not appear) directly to the right
  // of the avatar icon, and fills available remaining space in this view. If
  // the view is not animating, ensures that the label appears (or does not
  // appear) at the exact size and position it should.
  void EnsureLayout() const {
    EXPECT_EQ(gfx::Rect(gfx::Point(), kIconSize), icon()->bounds());
    if (layout()->is_animating()) {
      if (label()->GetVisible()) {
        EXPECT_EQ(0, label()->y());
        EXPECT_EQ(kIconDimension, label()->height());
        // TODO(dfried): eliminate potential for rounding error here. Currently
        // it is possible for the left side of the label to round up a pixel
        // when it is shrinking down due to how interpolation works, so we need
        // to account for that in determining if the current state is valid.
        EXPECT_GE(label()->x(), kIconDimension);
        EXPECT_LE(label()->x(), kIconDimension + 1);
        EXPECT_EQ(label()->width(), width() - label()->x());
      }
    } else if (showing_label_) {
      EXPECT_TRUE(label()->GetVisible());
      EXPECT_EQ(gfx::Rect(gfx::Point(kIconDimension, 0), kLabelSize),
                label()->bounds());
    } else {
      EXPECT_FALSE(label()->GetVisible());
    }
  }

 private:
  View* icon() { return children()[0]; }
  const View* icon() const { return children()[0]; }
  View* label() { return children()[1]; }
  const View* label() const { return children()[1]; }
  bool showing_label_ = false;
};

BEGIN_METADATA(SimulatedAvatarButton)
END_METADATA

// Simulates extensions buttons in the new toolbar extensions view, with a fixed
// button on the right and buttons to the left that can animate in and out and
// be hidden if there is insufficient space.
class SimulatedExtensionsContainer : public SimulatedToolbarElement {
  METADATA_HEADER(SimulatedExtensionsContainer, SimulatedToolbarElement)

 public:
  SimulatedExtensionsContainer() {
    auto* const main_button =
        AddChildView(std::make_unique<StaticSizedView>(kIconSize));
    main_button->SetProperty(kFlexBehaviorKey, FlexSpecification());
    layout()->SetDefaultFadeMode(
        AnimatingLayoutManager::FadeInOutMode::kSlideFromTrailingEdge);
    target_layout()->SetDefault(
        kFlexBehaviorKey,
        FlexSpecification(LayoutOrientation::kHorizontal,
                          MinimumFlexSizeRule::kPreferredSnapToZero));
  }

  ~SimulatedExtensionsContainer() override = default;

  void AddIcons(std::vector<bool> visibility) {
    int insertion_point = children().size() - 1;
    for (bool visible : visibility)
      AddIconAt(insertion_point++, visible);
  }

  void AddIconAt(int position, bool initially_visible) {
    DCHECK_GE(position, 0);
    DCHECK_LT(position, static_cast<int>(children().size()));
    auto new_child = std::make_unique<StaticSizedView>(kIconSize);
    new_child->SetVisible(initially_visible);
    if (initially_visible)
      visible_views_.insert(new_child.get());
    AddChildViewAt(std::move(new_child), position);
  }

  void RemoveIconAt(int position) {
    DCHECK_GE(position, 0);
    DCHECK_LT(position, static_cast<int>(children().size()) - 1);
    visible_views_.erase(RemoveChildViewT<View>(children()[position]).get());
  }

  void SetIconVisibility(int position, bool visible) {
    DCHECK_GE(position, 0);
    DCHECK_LT(position, static_cast<int>(children().size()) - 1);
    auto* const button = children()[position].get();
    if (visible) {
      layout()->FadeIn(button);
      visible_views_.insert(button);
    } else {
      layout()->FadeOut(button);
      visible_views_.erase(button);
    }
  }

  void MoveIcon(size_t from, size_t to) {
    DCHECK_NE(from, to);
    DCHECK_LT(from, children().size() - 1);
    DCHECK_LT(to, children().size() - 1);
    ReorderChildView(children()[from], to);
  }

  // Ensures that the extension icons appear with the size and placement and
  // visibility expected, and that the final "extensions menu" button always
  // appears and is flush with the right edge of the view:
  //
  //   |[pinned][pinned][pinned][menu]|   (unpinned extensions not visible)
  //
  // While animating, icons need only be the correct size and in the correct
  // region of the view, and visible if they are going to be visible in the
  // final layout (icons which are fading out may also be visible).
  //
  // If |expected_num_icons| is specified:
  // - while animating, serves as a lower bound on the number of icons displayed
  // - while not animating, must match the number of visible icons exactly
  void EnsureLayout(std::optional<int> expected_num_icons) const {
    if (layout()->is_animating()) {
      // For animating layouts, we ensure that icons are the correct size and
      // appear between the left edge of the container and exactly overlapping
      // the "extensions menu" icon (the final icon in the container).
      const int available_width = width() - kIconDimension;
      DCHECK_GE(available_width, 0);
      int num_visible = 0;
      for (int i = 0; i < static_cast<int>(children().size()) - 1; ++i) {
        const View* const child = children()[i];
        if (child->GetVisible()) {
          ++num_visible;
          EXPECT_GE(child->x(), 0) << " icon " << i;
          EXPECT_LE(child->x(), available_width) << " icon " << i;
          EXPECT_EQ(kIconSize, child->size()) << " icon " << i;
        }
      }
      if (expected_num_icons.has_value())
        EXPECT_GE(num_visible, expected_num_icons.value());
    } else {
      // Calculate how many icons *should* be visible given the available space.
      SizeBounds available_size = parent()->GetAvailableSize(this);
      int num_visible = visible_views_.size();
      if (available_size.width().is_bounded()) {
        num_visible = std::min(
            num_visible,
            (available_size.width().value() - kIconDimension) / kIconDimension);
      }
      DCHECK_LT(num_visible, static_cast<int>(children().size()));
      if (expected_num_icons.has_value())
        EXPECT_EQ(expected_num_icons.value(), num_visible);
      // Verify that the correct icons are visible and are in the correct place
      // with the correct size.
      int x = 0;
      for (int i = 0; i < static_cast<int>(children().size()) - 1; ++i) {
        const View* const child = children()[i];
        if (base::Contains(visible_views_, child)) {
          if (num_visible > 0) {
            --num_visible;
            EXPECT_TRUE(child->GetVisible()) << " icon " << i;
            EXPECT_EQ(gfx::Rect(gfx::Point(x, 0), kIconSize), child->bounds())
                << " icon " << i;
            x += kIconDimension;
          } else {
            // This is a pinned extension that overflowed the available space
            // and therefore should be hidden.
            EXPECT_FALSE(child->GetVisible())
                << " icon " << i
                << " should have been hidden; available size is "
                << available_size.ToString();
          }
        } else {
          // This icon is explicitly hidden.
          EXPECT_FALSE(child->GetVisible()) << " icon " << i;
        }
      }
    }
    EXPECT_TRUE(main_button()->GetVisible());
    EXPECT_EQ(kIconSize, main_button()->size());
    EXPECT_EQ(width(), main_button()->bounds().right());
  }

 private:
  const View* main_button() const { return children()[children().size() - 1]; }

  std::set<raw_ptr<const View, SetExperimental>> visible_views_;
};

BEGIN_METADATA(SimulatedExtensionsContainer)
END_METADATA

// Simulates a toolbar with buttons on either side, a "location bar", and mock
// versions of the extensions container and avatar button.
class SimulatedToolbar : public View {
  METADATA_HEADER(SimulatedToolbar, View)

 public:
  SimulatedToolbar() {
    AddChildView(std::make_unique<StaticSizedView>(kIconSize));
    auto* const bar =
        AddChildView(std::make_unique<StaticSizedView>(kBarPreferredSize));
    bar->set_minimum_size(kBarMinimumSize);
    extensions_ =
        AddChildView(std::make_unique<SimulatedExtensionsContainer>());
    avatar_ = AddChildView(std::make_unique<SimulatedAvatarButton>());
    AddChildView(std::make_unique<StaticSizedView>(kIconSize));

    SetLayoutManager(std::make_unique<FlexLayout>())
        ->SetOrientation(LayoutOrientation::kHorizontal);
    avatar_->SetProperty(
        kFlexBehaviorKey,
        FlexSpecification(avatar_->layout()->GetDefaultFlexRule())
            .WithOrder(1));
    bar->SetProperty(kFlexBehaviorKey,
                     FlexSpecification(LayoutOrientation::kHorizontal,
                                       MinimumFlexSizeRule::kScaleToMinimum,
                                       MaximumFlexSizeRule::kUnbounded)
                         .WithOrder(2));
    extensions_->SetProperty(
        kFlexBehaviorKey,
        FlexSpecification(extensions_->layout()->GetDefaultFlexRule())
            .WithOrder(3));
  }

  SimulatedExtensionsContainer* extensions() { return extensions_; }
  const SimulatedExtensionsContainer* extensions() const { return extensions_; }
  SimulatedAvatarButton* avatar() { return avatar_; }
  const SimulatedAvatarButton* avatar() const { return avatar_; }
  View* location() { return children()[1]; }
  const View* location() const { return children()[1]; }

  // Ensures the layout of the toolbar which contains:
  // - one dummy back/forward/home type button of fixed size
  // - a location bar with flexible width
  // - a simulated extensions container
  // - a simulated avatar button
  // - one dummy "wrench menu" type button of fixed size
  //
  // All child views must be laid out end-to-end adjacent to each other, in the
  // right order, and at the correct size. Furthermore, EnsureLayout() is called
  // on the child views that support it.
  //
  // The parameter |expected_num_extension_icons| is passed to
  // SimulatedExtensionsContainer::EnsureLayout().
  void EnsureLayout(std::optional<int> expected_num_extension_icons) const {
    EXPECT_EQ(kIconDimension, height());
    EXPECT_EQ(gfx::Rect(gfx::Point(), kIconSize), children()[0]->bounds());
    EXPECT_EQ(gfx::Point(kIconDimension, 0), location()->origin());
    EXPECT_EQ(kIconDimension, location()->height());
    EXPECT_GE(location()->width(), kBarMinimumWidth);
    EXPECT_EQ(gfx::Point(location()->bounds().right(), 0),
              extensions_->origin());
    extensions_->EnsureLayout(expected_num_extension_icons);
    EXPECT_EQ(gfx::Point(extensions_->bounds().right(), 0), avatar_->origin());
    avatar_->EnsureLayout();
    EXPECT_EQ(gfx::Rect(gfx::Point(avatar_->bounds().right(), 0), kIconSize),
              children()[4]->bounds());
    if (location()->width() == kBarMinimumWidth)
      EXPECT_LE(width(), children()[4]->bounds().right());
    else
      EXPECT_EQ(width(), children()[4]->bounds().right());
  }

 private:
  raw_ptr<SimulatedExtensionsContainer> extensions_;
  raw_ptr<SimulatedAvatarButton> avatar_;
};

BEGIN_METADATA(SimulatedToolbar)
END_METADATA

}  // anonymous namespace

// Test suite. Sets up the mock toolbar and ties animation of all child elements
// together so they can be controlled for testing. Use the utility methods here
// as much as possible rather than calling methods on the individual Views.
class CompositeLayoutTest : public testing::Test {
 public:
  void SetUp() override {
    // In case the user is running these tests manually on a machine where
    // animation is disabled for accessibility or visual reasons (e.g. on a
    // Windows system via Chrome Remote Desktop), force animation on for the
    // purposes of testing these layout configurations.
    animation_lock_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

    toolbar_ = std::make_unique<SimulatedToolbar>();
    toolbar_->SetSize(kDefaultToolbarSize);
    extensions_test_api_ = std::make_unique<gfx::AnimationContainerTestApi>(
        extensions()->layout()->GetAnimationContainerForTesting());
    avatar_test_api_ = std::make_unique<gfx::AnimationContainerTestApi>(
        avatar()->layout()->GetAnimationContainerForTesting());
  }

  SimulatedAvatarButton* avatar() { return toolbar_->avatar(); }
  const SimulatedAvatarButton* avatar() const { return toolbar_->avatar(); }
  SimulatedExtensionsContainer* extensions() { return toolbar_->extensions(); }
  const SimulatedExtensionsContainer* extensions() const {
    return toolbar_->extensions();
  }
  SimulatedToolbar* toolbar() { return toolbar_.get(); }
  const SimulatedToolbar* toolbar() const { return toolbar_.get(); }

  void SetWidth(int width) {
    toolbar()->SetSize(gfx::Size(width, kIconDimension));
  }

  void ChangeWidth(int delta) {
    toolbar()->SetSize(
        gfx::Size(std::max(0, toolbar()->width() + delta), kIconDimension));
  }

  void AdvanceAnimations(int ms) {
    const auto delta = base::Milliseconds(ms);
    if (avatar()->layout()->is_animating())
      avatar_test_api_->IncrementTime(delta);
    if (extensions()->layout()->is_animating())
      extensions_test_api_->IncrementTime(delta);
    views::test::RunScheduledLayout(toolbar());
  }

  void ResetAnimation() {
    avatar()->layout()->ResetLayout();
    extensions()->layout()->ResetLayout();
    views::test::RunScheduledLayout(toolbar());
  }

  bool IsAnimating() const {
    return avatar()->layout()->is_animating() ||
           extensions()->layout()->is_animating();
  }

  void FinishAnimations() {
    // Advance the animation an unreasonable amount of times and fail if that
    // doesn't actually cause the animation to complete. It's possible that one
    // animation could lead to another so basing our limit only on the current
    // animation durations is not necessarily reliable.
    for (int i = 0; i < 100; ++i) {
      if (!IsAnimating())
        return;
      // Advance by a small but significant step (1/10 of a second).
      AdvanceAnimations(100);
    }
    GTEST_FAIL()
        << "Animations did not complete in a reasonable amount of time.";
  }

  // Ensures the toolbar layout and all child view layouts are as expected.
  // If |expected_num_extension_icons| is specified, then exactly that many (or
  // at minimum that many if animating) simulated extension icons must be
  // visible.
  void EnsureLayout(
      std::optional<int> expected_num_extension_icons = std::nullopt) {
    toolbar_->EnsureLayout(expected_num_extension_icons);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<gfx::AnimationContainerTestApi> extensions_test_api_;
  std::unique_ptr<gfx::AnimationContainerTestApi> avatar_test_api_;
  std::unique_ptr<SimulatedToolbar> toolbar_;
  gfx::AnimationTestApi::RenderModeResetter animation_lock_;
};

// ------------
// Basic tests.

TEST_F(CompositeLayoutTest, InitialLayout) {
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, SmallResize) {
  toolbar()->SetSize(gfx::Size(300, kIconDimension));
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ShrinkLocationBar) {
  toolbar()->SetSize(
      gfx::Size(4 * kIconDimension + kBarMinimumWidth, kIconDimension));
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ShrinkLocationBarTooSmall) {
  toolbar()->SetSize(
      gfx::Size(4 * kIconDimension + kBarMinimumWidth - 20, kIconDimension));
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ProfileAnimates) {
  avatar()->FadeLabelIn();
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
  avatar()->FadeLabelOut();
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ProfileAnimationInterrupted) {
  avatar()->FadeLabelIn();
  EXPECT_TRUE(IsAnimating());
  AdvanceAnimations(500);
  EnsureLayout();
  avatar()->FadeLabelOut();
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ProfileAnimationInterruptedImmediately) {
  avatar()->FadeLabelIn();
  avatar()->FadeLabelOut();
  EXPECT_FALSE(IsAnimating());
  EnsureLayout();
}

// ----------------------------------------------------------
// Tests which add/remove extension icons from the container.

TEST_F(CompositeLayoutTest, ExtensionsAnimateOnAdd) {
  extensions()->AddIconAt(0, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
  extensions()->AddIconAt(0, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  extensions()->AddIconAt(2, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionsAnimateOnAddMultiple) {
  extensions()->AddIconAt(0, true);
  extensions()->AddIconAt(0, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
  extensions()->AddIconAt(1, true);
  extensions()->AddIconAt(3, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionsAnimateOnAddMultipleStaggered) {
  extensions()->AddIconAt(0, true);
  EXPECT_TRUE(IsAnimating());
  AdvanceAnimations(200);
  EnsureLayout();
  EXPECT_TRUE(IsAnimating());
  extensions()->AddIconAt(0, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
  extensions()->AddIconAt(1, true);
  EXPECT_TRUE(IsAnimating());
  AdvanceAnimations(200);
  EnsureLayout();
  EXPECT_TRUE(IsAnimating());
  extensions()->AddIconAt(3, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionRemovedDuringAnimation) {
  extensions()->AddIconAt(0, true);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->AddIconAt(0, true);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->AddIconAt(2, true);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->RemoveIconAt(1);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->AddIconAt(0, true);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->RemoveIconAt(2);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->RemoveIconAt(0);
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionRemovedImmediately) {
  extensions()->AddIconAt(0, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
  extensions()->AddIconAt(1, true);
  extensions()->RemoveIconAt(1);
  EXPECT_FALSE(IsAnimating());
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionRemovedImmediatelyDuringAnimation) {
  extensions()->AddIconAt(0, true);
  EXPECT_TRUE(IsAnimating());
  AdvanceAnimations(500);
  EnsureLayout();
  extensions()->AddIconAt(1, true);
  extensions()->RemoveIconAt(1);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

// -----------------------------------------------
// Tests which show/hide existing extension icons.

TEST_F(CompositeLayoutTest, ExtensionsAnimateOnShow) {
  extensions()->AddIcons({false, true, false});
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
  extensions()->SetIconVisibility(0, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  extensions()->AddIconAt(2, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionsAnimateOnShowMultiple) {
  extensions()->AddIcons({true, false, true, false});
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
  extensions()->SetIconVisibility(1, true);
  extensions()->SetIconVisibility(3, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionsAnimateOnShowMultipleStaggered) {
  extensions()->AddIcons({false, false, true, false});
  EXPECT_TRUE(IsAnimating());
  AdvanceAnimations(200);
  EnsureLayout();
  EXPECT_TRUE(IsAnimating());
  extensions()->SetIconVisibility(0, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
  extensions()->SetIconVisibility(1, true);
  EXPECT_TRUE(IsAnimating());
  AdvanceAnimations(200);
  EnsureLayout();
  EXPECT_TRUE(IsAnimating());
  extensions()->SetIconVisibility(3, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionHiddenDuringAnimation) {
  extensions()->AddIcons({false, false, true, false});
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->SetIconVisibility(1, true);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->SetIconVisibility(3, true);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->SetIconVisibility(2, false);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->SetIconVisibility(0, true);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->SetIconVisibility(3, false);
  AdvanceAnimations(200);
  EnsureLayout();
  extensions()->SetIconVisibility(0, false);
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionHiddenImmediately) {
  extensions()->AddIcons({true, false});
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
  extensions()->SetIconVisibility(1, true);
  extensions()->SetIconVisibility(1, false);
  EXPECT_FALSE(IsAnimating());
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionHiddenImmediatelyDuringAnimation) {
  extensions()->AddIcons({true, false});
  EXPECT_TRUE(IsAnimating());
  AdvanceAnimations(500);
  EnsureLayout();
  extensions()->SetIconVisibility(1, true);
  extensions()->SetIconVisibility(1, false);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

// ----------------------------------
// Tests where child views are moved.

TEST_F(CompositeLayoutTest, ExtensionOrderChanged) {
  extensions()->AddIcons({true, true, true});
  FinishAnimations();
  EnsureLayout();
  extensions()->MoveIcon(1, 2);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionHiddenAndPoppedOutImmediate) {
  extensions()->AddIcons({true, true, true});
  FinishAnimations();
  EnsureLayout();
  extensions()->SetIconVisibility(1, false);
  extensions()->MoveIcon(1, 2);
  extensions()->SetIconVisibility(2, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionHiddenAndPoppedOutDelayed) {
  extensions()->AddIcons({true, true, true});
  FinishAnimations();
  EnsureLayout();
  extensions()->SetIconVisibility(1, false);
  extensions()->MoveIcon(1, 2);
  AdvanceAnimations(500);
  extensions()->SetIconVisibility(2, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionPinned) {
  extensions()->AddIcons({true, true, false, false});
  FinishAnimations();
  EnsureLayout();
  extensions()->MoveIcon(3, 0);
  extensions()->SetIconVisibility(0, true);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, VisibleExtensionMoved) {
  extensions()->AddIcons({true, true, false, false});
  FinishAnimations();
  EnsureLayout();
  extensions()->MoveIcon(1, 0);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, InvisibleExtensionMoved) {
  extensions()->AddIcons({true, true, false, false});
  FinishAnimations();
  EnsureLayout();
  extensions()->MoveIcon(2, 3);
  EXPECT_FALSE(IsAnimating());
  EnsureLayout();
}

// -----------------------------------------------
// Tests combining two different animating views.

TEST_F(CompositeLayoutTest, ExtensionsAndAvatarAnimateSimultaneously) {
  // Expand both views.
  extensions()->AddIcons({true, true});
  avatar()->FadeLabelIn();
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EXPECT_FALSE(IsAnimating());
  EnsureLayout();
  // Expand one, contract the other.
  extensions()->AddIcons({true});
  avatar()->FadeLabelOut();
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout();
  EXPECT_FALSE(IsAnimating());
  // Then vice-versa.
  extensions()->SetIconVisibility(1, false);
  avatar()->FadeLabelIn();
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EXPECT_FALSE(IsAnimating());
  EnsureLayout();
}

TEST_F(CompositeLayoutTest, ExtensionsAndAvatarAnimateStaggered) {
  // Expand both views.
  extensions()->AddIcons({true, true});
  AdvanceAnimations(500);
  EnsureLayout();
  avatar()->FadeLabelIn();
  FinishAnimations();
  EnsureLayout();
  // Expand one, contract the other.
  extensions()->AddIcons({true});
  AdvanceAnimations(500);
  EnsureLayout();
  avatar()->FadeLabelOut();
  FinishAnimations();
  EnsureLayout();
  // Then vice-versa.
  extensions()->SetIconVisibility(1, false);
  AdvanceAnimations(500);
  avatar()->FadeLabelIn();
  FinishAnimations();
  EnsureLayout();
}

// -----------------------
// Tests in limited space.

TEST_F(CompositeLayoutTest, ExtensionsNotShownWhenSpaceConstrained) {
  // At the minimum size for the toolbar, no icons should be displayed.
  toolbar()->SizeToPreferredSize();
  extensions()->AddIcons({true, true});
  EXPECT_FALSE(IsAnimating());
  EnsureLayout(0);

  // Increase the size gradually, exposing pinned icons.
  // We don't really care if the individual icons animate out or not; as long as
  // the correct number are displayed at each step.
  ChangeWidth(kIconDimension / 2);
  EXPECT_FALSE(IsAnimating());
  EnsureLayout(0);
  ChangeWidth(kIconDimension / 2);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout(1);
  ChangeWidth(kIconDimension / 2);
  EXPECT_FALSE(IsAnimating());
  EnsureLayout(1);
  ChangeWidth(kIconDimension / 2);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout(2);
}

TEST_F(CompositeLayoutTest, SomeExtensionsNotShownWhenSpaceConstrained) {
  // Provide room for one of two icons.
  SetWidth(toolbar()->GetPreferredSize({}).width() + kIconDimension);
  extensions()->AddIcons({true, true});
  FinishAnimations();
  EnsureLayout(1);

  // Increase the size gradually, exposing pinned icons.
  // We don't really care if the individual icons animate out or not; as long as
  // the correct number are displayed at each step.
  ChangeWidth(kIconDimension / 2);
  EXPECT_FALSE(IsAnimating());
  EnsureLayout(1);
  ChangeWidth(kIconDimension / 2);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout(2);
}

TEST_F(CompositeLayoutTest, ExtensionsShownSnapsWhenSpaceShrinks) {
  // Provide room for both icons.
  SetWidth(toolbar()->GetPreferredSize({}).width() + 2 * kIconDimension);
  extensions()->AddIcons({true, true});
  FinishAnimations();
  EnsureLayout(2);

  ChangeWidth(-kIconDimension);
  EXPECT_FALSE(IsAnimating());
  EnsureLayout(1);

  ChangeWidth(-kIconDimension);
  EXPECT_FALSE(IsAnimating());
  EnsureLayout(0);
}

TEST_F(CompositeLayoutTest,
       ExtensionsShowingAnimationRedirectsDueToSmallerAvailableSpace) {
  // Provide room for both icons.
  SetWidth(toolbar()->GetPreferredSize({}).width() + 2 * kIconDimension);
  extensions()->AddIcons({true, true});
  AdvanceAnimations(400);

  // The icons are fading in, but not enough that the animation would be reset
  // by changing the toolbar width by one icon width.
  ChangeWidth(-kIconDimension);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout(1);
}

TEST_F(CompositeLayoutTest,
       ExtensionsShowingAnimationCancelsDueToSmallerAvailableSpace) {
  // Provide room for both icons.
  SetWidth(toolbar()->GetPreferredSize({}).width() + 2 * kIconDimension);
  extensions()->AddIcons({true, true});
  AdvanceAnimations(800);

  // The icons are fading in, far enough that the animation is reset by changing
  // the toolbar width by one icon width.
  ChangeWidth(-kIconDimension);
  EXPECT_FALSE(IsAnimating());
  EnsureLayout(1);
}

TEST_F(CompositeLayoutTest,
       ExtensionsShowingAnimationRedirectsDueToLargerAvailableSpace) {
  // Provide room for one of two icons.
  SetWidth(toolbar()->GetPreferredSize({}).width() + kIconDimension);
  extensions()->AddIcons({true, true});
  AdvanceAnimations(400);

  // Make room for the second icon; the animation should continue.
  ChangeWidth(kIconDimension);
  EXPECT_TRUE(IsAnimating());
  FinishAnimations();
  EnsureLayout(2);
}

TEST_F(CompositeLayoutTest, ExtensionsHiddenWhenAvatarExpands) {
  extensions()->AddIcons({true, true, true, true, true});
  ResetAnimation();
  EnsureLayout(5);
  toolbar()->SizeToPreferredSize();
  EXPECT_FALSE(IsAnimating());

  avatar()->FadeLabelIn();

  // Halfway through, the label will have displaced 35 pixels, or two icons.
  AdvanceAnimations(500);
  EnsureLayout(3);

  // At its largest, 70 pixels and four icons.
  FinishAnimations();
  EnsureLayout(1);
}

TEST_F(CompositeLayoutTest, ExtensionsShownWhenAvatarCollapses) {
  extensions()->AddIcons({true});
  avatar()->FadeLabelIn();
  ResetAnimation();
  toolbar()->SizeToPreferredSize();
  // These should all be hidden.
  extensions()->AddIcons({true, true, true, true});
  EnsureLayout(1);

  avatar()->FadeLabelOut();

  // Halfway through, the label will cede back 35 pixels - enough to display an
  // additional icon.
  AdvanceAnimations(500);
  EnsureLayout(2);

  // Finish everything - this will include icons revealed at the very end. Since
  // 70 pixels total are ceded back, three of the four newly-added icons can be
  // shown.
  FinishAnimations();
  EnsureLayout(4);
}

TEST_F(CompositeLayoutTest, ExtensionsHideAndShowWhenAvatarAnimates) {
  extensions()->AddIcons({true, true, true, true, true});
  ResetAnimation();
  EnsureLayout(5);
  toolbar()->SizeToPreferredSize();
  EXPECT_FALSE(IsAnimating());

  avatar()->FadeLabelIn();

  // Halfway through, the label will have displaced 35 pixels, or two icons.
  AdvanceAnimations(500);
  EnsureLayout(3);

  // Interrupt most of the way through.
  AdvanceAnimations(200);
  EnsureLayout(2);

  // Fade the label out and make sure all of the extensions reappeared.
  avatar()->FadeLabelOut();
  FinishAnimations();
  EnsureLayout(5);
}

TEST_F(CompositeLayoutTest, ExtensionsShowAndHideWhenAvatarAnimates) {
  extensions()->AddIcons({true});
  avatar()->FadeLabelIn();
  ResetAnimation();
  toolbar()->SizeToPreferredSize();
  // These should all be hidden.
  extensions()->AddIcons({true, true, true, true});
  ResetAnimation();

  // Halfway through, the label will have ceded 35 pixels, or one icon.
  avatar()->FadeLabelOut();
  AdvanceAnimations(500);
  EnsureLayout(2);

  // Interrupt most of the way through.
  AdvanceAnimations(200);
  EnsureLayout(3);

  // Fade the label back in and make sure all of the extensions re-hide.
  avatar()->FadeLabelIn();
  FinishAnimations();
  EnsureLayout(1);
}

TEST_F(CompositeLayoutTest, MultipleAnimationAndLayoutChanges) {
  extensions()->AddIcons({true});
  avatar()->FadeLabelIn();
  ResetAnimation();
  toolbar()->SizeToPreferredSize();
  // These should all be hidden.
  extensions()->AddIcons({true, true, true});
  ResetAnimation();

  // Halfway through, the label will have ceded 35 pixels, or one icon.
  avatar()->FadeLabelOut();
  AdvanceAnimations(500);
  EnsureLayout(2);

  // Interrupt most of the way through to add a random icon.
  AdvanceAnimations(100);
  extensions()->AddIconAt(2, true);
  AdvanceAnimations(100);
  EnsureLayout(3);

  extensions()->SetIconVisibility(1, false);
  extensions()->SetIconVisibility(3, false);
  FinishAnimations();
  EnsureLayout(3);

  // Fade the label back in and make sure all of the extensions re-hide.
  avatar()->FadeLabelIn();
  FinishAnimations();
  EnsureLayout(1);
}

}  // namespace views
