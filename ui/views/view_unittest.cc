// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/display_item_list.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/test_layers.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/scoped_target_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/paint_info.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/window/dialog_delegate.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace {

// Returns true if |ancestor| is an ancestor of |layer|.
bool LayerIsAncestor(const ui::Layer* ancestor, const ui::Layer* layer) {
  while (layer && layer != ancestor)
    layer = layer->parent();
  return layer == ancestor;
}

// Convenience functions for walking a View tree.
const views::View* FirstView(const views::View* view) {
  const views::View* v = view;
  while (!v->children().empty())
    v = v->children().front();
  return v;
}

const views::View* NextView(const views::View* view) {
  const views::View* v = view;
  const views::View* parent = v->parent();
  if (!parent)
    return nullptr;
  const auto next = std::next(parent->FindChild(v));
  return (next == parent->children().cend()) ? parent : FirstView(*next);
}

// Convenience functions for walking a Layer tree.
const ui::Layer* FirstLayer(const ui::Layer* layer) {
  const ui::Layer* l = layer;
  while (!l->children().empty())
    l = l->children().front();
  return l;
}

const ui::Layer* NextLayer(const ui::Layer* layer) {
  const ui::Layer* parent = layer->parent();
  if (!parent)
    return nullptr;
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>> children =
      parent->children();
  const auto i = base::ranges::find(children, layer) + 1;
  return (i == children.cend()) ? parent : FirstLayer(*i);
}

// Given the root nodes of a View tree and a Layer tree, makes sure the two
// trees are in sync.
bool ViewAndLayerTreeAreConsistent(const views::View* view,
                                   const ui::Layer* layer) {
  const views::View* v = FirstView(view);
  const ui::Layer* l = FirstLayer(layer);
  while (v && l) {
    // Find the view with a layer.
    while (v && !v->layer())
      v = NextView(v);
    EXPECT_TRUE(v);
    if (!v)
      return false;

    // Check if the View tree and the Layer tree are in sync.
    EXPECT_EQ(l, v->layer());
    if (v->layer() != l)
      return false;

    // Check if the visibility states of the View and the Layer are in sync.
    EXPECT_EQ(l->IsVisible(), v->IsDrawn());
    if (v->IsDrawn() != l->IsVisible()) {
      for (const views::View* vv = v; vv; vv = vv->parent())
        LOG(ERROR) << "V: " << vv << " " << vv->GetVisible() << " "
                   << vv->IsDrawn() << " " << vv->layer();
      for (const ui::Layer* ll = l; ll; ll = ll->parent())
        LOG(ERROR) << "L: " << ll << " " << ll->IsVisible();
      return false;
    }

    // Check if the size of the View and the Layer are in sync.
    EXPECT_EQ(l->bounds(), v->bounds());
    if (v->bounds() != l->bounds())
      return false;

    if (v == view || l == layer)
      return v == view && l == layer;

    v = NextView(v);
    l = NextLayer(l);
  }

  return false;
}

// Constructs a View tree with the specified depth.
void ConstructTree(views::View* view, int depth) {
  if (depth == 0)
    return;
  int count = base::RandInt(1, 5);
  for (int i = 0; i < count; i++) {
    views::View* v = new views::View;
    view->AddChildView(v);
    if (base::RandDouble() > 0.5)
      v->SetPaintToLayer();
    if (base::RandDouble() < 0.2)
      v->SetVisible(false);

    ConstructTree(v, depth - 1);
  }
}

void ScrambleTree(views::View* view) {
  if (view->children().empty())
    return;

  for (views::View* child : view->children())
    ScrambleTree(child);

  size_t count = view->children().size();
  if (count > 1) {
    const uint64_t max = count - 1;
    size_t a = static_cast<size_t>(base::RandGenerator(max));
    size_t b = static_cast<size_t>(base::RandGenerator(max));

    if (a != b) {
      views::View* view_a = view->children()[a];
      views::View* view_b = view->children()[b];
      view->ReorderChildView(view_a, b);
      view->ReorderChildView(view_b, a);
    }
  }

  if (!view->layer() && base::RandDouble() < 0.1)
    view->SetPaintToLayer();

  if (base::RandDouble() < 0.1)
    view->SetVisible(!view->GetVisible());
}

}  // namespace

namespace views {

using ViewTest = ViewsTestBase;

// A derived class for testing purpose.
class TestView : public View {
  METADATA_HEADER(TestView, View)

 public:
  TestView() : a11y_ignore_missing_widget_(GetViewAccessibility()) {}
  ~TestView() override {
    if (destruction_callback_) {
      std::move(destruction_callback_).Run();
    }
  }

  // Reset all test state.
  void Reset() {
    did_change_bounds_ = false;
    did_layout_ = false;
    last_mouse_event_type_ = ui::EventType::kUnknown;
    location_.SetPoint(0, 0);
    received_mouse_enter_ = false;
    received_mouse_exit_ = false;
    did_paint_ = false;
    accelerator_count_map_.clear();
    destruction_callback_.Reset();
  }

  // Exposed as public for testing.
  void DoFocus() { views::View::Focus(); }

  void DoBlur() { views::View::Blur(); }

  void Layout(PassKey) override {
    did_layout_ = true;
    LayoutSuperclass<View>(this);
  }

  void SetDestructionCallback(base::OnceClosure destruction_callback) {
    destruction_callback_ = std::move(destruction_callback);
  }

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  void OnPaint(gfx::Canvas* canvas) override;
  void OnDidSchedulePaint(const gfx::Rect& rect) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  void OnThemeChanged() override;

  void OnAccessibilityEvent(ax::mojom::Event event_type) override;

  // OnBoundsChanged.
  bool did_change_bounds_ = false;
  gfx::Rect new_bounds_;

  // Layout.
  bool did_layout_ = false;

  // MouseEvent.
  ui::EventType last_mouse_event_type_ = ui::EventType::kUnknown;
  gfx::Point location_;
  bool received_mouse_enter_ = false;
  bool received_mouse_exit_ = false;
  bool delete_on_pressed_ = false;

  // Painting.
  std::vector<gfx::Rect> scheduled_paint_rects_;
  bool did_paint_ = false;

  // Accelerators.
  std::map<ui::Accelerator, int> accelerator_count_map_;

  // Native theme.
  raw_ptr<const ui::NativeTheme> native_theme_ = nullptr;

  // Accessibility events
  ax::mojom::Event last_a11y_event_ = ax::mojom::Event::kNone;

  base::OnceClosure destruction_callback_;

  IgnoreMissingWidgetForTestingScopedSetter a11y_ignore_missing_widget_;
};

void TestView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  did_change_bounds_ = true;
  new_bounds_ = bounds();
}

bool TestView::OnMousePressed(const ui::MouseEvent& event) {
  last_mouse_event_type_ = event.type();
  location_.SetPoint(event.x(), event.y());
  if (delete_on_pressed_) {
    delete this;
  }
  return true;
}

bool TestView::OnMouseDragged(const ui::MouseEvent& event) {
  last_mouse_event_type_ = event.type();
  location_.SetPoint(event.x(), event.y());
  return true;
}

void TestView::OnMouseReleased(const ui::MouseEvent& event) {
  last_mouse_event_type_ = event.type();
  location_.SetPoint(event.x(), event.y());
}

void TestView::OnMouseEntered(const ui::MouseEvent& event) {
  received_mouse_enter_ = true;
}

void TestView::OnMouseExited(const ui::MouseEvent& event) {
  received_mouse_exit_ = true;
}

void TestView::OnPaint(gfx::Canvas* canvas) {
  did_paint_ = true;
}

void TestView::OnDidSchedulePaint(const gfx::Rect& rect) {
  scheduled_paint_rects_.push_back(rect);
  View::OnDidSchedulePaint(rect);
}

bool TestView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  accelerator_count_map_[accelerator]++;
  return true;
}

void TestView::OnThemeChanged() {
  View::OnThemeChanged();
  native_theme_ = GetNativeTheme();
}

void TestView::OnAccessibilityEvent(ax::mojom::Event event_type) {
  last_a11y_event_ = event_type;
}

BEGIN_METADATA(TestView)
END_METADATA

class A11yTestView : public TestView {
  METADATA_HEADER(A11yTestView, TestView)

 public:
  // Convenience constructor to test `ViewAccessibility::SetProperties`
  explicit A11yTestView(
      std::optional<ax::mojom::Role> role = std::nullopt,
      std::optional<std::u16string> name = std::nullopt,
      std::optional<std::u16string> description = std::nullopt,
      std::optional<std::u16string> role_description = std::nullopt,
      std::optional<ax::mojom::NameFrom> name_from = std::nullopt,
      std::optional<ax::mojom::DescriptionFrom> description_from =
          std::nullopt) {
    if (role) {
      GetViewAccessibility().SetRole(*role);
    }
    if (name && name_from) {
      GetViewAccessibility().SetName(*name, *name_from);
    } else if (name) {
      GetViewAccessibility().SetName(*name);
    }
    if (description && description_from) {
      GetViewAccessibility().SetDescription(*description, *description_from);
    } else if (description) {
      GetViewAccessibility().SetDescription(*description);
    }
    if (role_description) {
      GetViewAccessibility().SetRoleDescription(*role_description);
    }
  }

  ~A11yTestView() override = default;

  // Overridden from views::View:
  void AdjustAccessibleName(std::u16string& new_name,
                            ax::mojom::NameFrom& name_from) override {
    if (name_prefix_.has_value()) {
      new_name.insert(0, name_prefix_.value());
    }

    if (name_from_.has_value()) {
      name_from = name_from_.value();
    }
  }

  void SetAccessibleNamePrefix(std::optional<std::u16string> name_prefix) {
    name_prefix_ = std::move(name_prefix);
  }

  void SetAccessibleNameFrom(std::optional<ax::mojom::NameFrom> name_from) {
    name_from_ = std::move(name_from);
  }

 private:
  std::optional<std::u16string> name_prefix_;
  std::optional<ax::mojom::NameFrom> name_from_;
};

BEGIN_METADATA(A11yTestView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// Metadata
////////////////////////////////////////////////////////////////////////////////

TEST_F(ViewTest, MetadataTest) {
  auto test_view = std::make_unique<TestView>();
  test::TestViewMetadata(test_view.get());
}

////////////////////////////////////////////////////////////////////////////////
// Layout
////////////////////////////////////////////////////////////////////////////////

TEST_F(ViewTest, LayoutCalledInvalidateAndOriginChanges) {
  TestView parent;
  TestView* child = new TestView;
  gfx::Rect parent_rect(0, 0, 100, 100);
  parent.SetBoundsRect(parent_rect);

  parent.Reset();
  // |AddChildView| invalidates parent's layout.
  parent.AddChildView(child);
  // Change rect so that only rect's origin is affected.
  parent.SetBoundsRect(parent_rect + gfx::Vector2d(10, 0));

  EXPECT_TRUE(parent.did_layout_);

  // After child layout is invalidated, parent and child must be laid out
  // during parent->BoundsChanged(...) call.
  parent.Reset();
  child->Reset();

  child->InvalidateLayout();
  parent.SetBoundsRect(parent_rect + gfx::Vector2d(20, 0));
  EXPECT_TRUE(parent.did_layout_);
  EXPECT_TRUE(child->did_layout_);
}

// Tests that SizeToPreferredSize will trigger a Layout if the size has changed
// or if layout is marked invalid.
TEST_F(ViewTest, SizeToPreferredSizeInducesLayout) {
  TestView example_view;
  example_view.SetPreferredSize(gfx::Size(101, 102));
  example_view.SizeToPreferredSize();
  EXPECT_TRUE(example_view.did_layout_);

  example_view.Reset();
  example_view.SizeToPreferredSize();
  EXPECT_FALSE(example_view.did_layout_);

  example_view.InvalidateLayout();
  example_view.SizeToPreferredSize();
  EXPECT_TRUE(example_view.did_layout_);
}

namespace {

// A view that provides direct and indirect ways to trigger
// `LayoutSuperclass<>()`.
class SuperclassLayoutTestView : public TestView {
 public:
  int layout_count() const { return layout_count_; }

  void AttemptSuperclassLayout() {
    ++layout_count_;
    LayoutSuperclass<TestView>(this);
  }

  void Layout(PassKey) override { AttemptSuperclassLayout(); }

 private:
  int layout_count_ = 0;
};

}  // namespace

// Verifies that LayoutSuperclass<>() can only be invoked while layout is
// occurring.
TEST_F(ViewTest, CannotLayoutSuperclassOutsideLayout) {
  // Construction should not automatically attempt layout.
  SuperclassLayoutTestView view;
  EXPECT_EQ(0, view.layout_count());

  // Triggering layout through the standard method should attempt superclass
  // layout, which should succeed.
  view.InvalidateLayout();
  test::RunScheduledLayout(&view);
  EXPECT_EQ(1, view.layout_count());

  // Attempting superclass layout outside that flow should checkfail.
  EXPECT_CHECK_DEATH(view.AttemptSuperclassLayout());
}

////////////////////////////////////////////////////////////////////////////////
// Accessibility Property Setters
////////////////////////////////////////////////////////////////////////////////

TEST_F(ViewTest, ViewAccessibilityReadyToNotifyEvents) {
  TestView v;
  v.GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  EXPECT_EQ(v.GetViewAccessibility().ready_to_notify_events_, false);

  // Setting the accessible name when `ready_to_notify_events_` is false
  // shouldn't result in no event being fired.
  v.last_a11y_event_ = ax::mojom::Event::kNone;
  v.SetAccessibleName(u"Name");
  EXPECT_EQ(v.last_a11y_event_, ax::mojom::Event::kNone);
  EXPECT_EQ(v.GetViewAccessibility().ready_to_notify_events_, false);

  // Setting the accessible name when `ready_to_notify_events_` is true
  // should result in an event being fired.
  v.last_a11y_event_ = ax::mojom::Event::kNone;
  v.GetViewAccessibility().ready_to_notify_events_ = true;
  v.SetAccessibleName(u"New Name");
  EXPECT_EQ(v.last_a11y_event_, ax::mojom::Event::kTextChanged);
  EXPECT_EQ(v.GetViewAccessibility().ready_to_notify_events_, true);
}

TEST_F(ViewTest, ReadyToSendAccessibilityEvents) {
  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());
  auto view1 = std::make_unique<TestView>();
  view1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  view1->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  auto view2 = std::make_unique<TestView>();
  view2->SetBoundsRect(gfx::Rect(100, 100, 100, 100));
  view2->GetViewAccessibility().SetRole(ax::mojom::Role::kLink);

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  // The root should always be connected to the widget and so is always ready to
  // send events.
  root->GetViewAccessibility().SetName(u"Root",
                                       ax::mojom::NameFrom::kAttribute);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kTextChanged, root), 1);

  // No events should be sent if the view is not connected to a RootView.
  auto* added_view_2 = view1->AddChildView(std::move(view2));
  added_view_2->GetViewAccessibility().SetName(u"Child",
                                               ax::mojom::NameFrom::kAttribute);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kTextChanged, added_view_2),
            0);

  // Events should be sent if the view is connected to a RootView.
  auto* added_view_1 = root->AddChildView(std::move(view1));
  added_view_1->GetViewAccessibility().SetName(u"Descendant_1",
                                               ax::mojom::NameFrom::kAttribute);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kTextChanged, added_view_1),
            1);
  added_view_2->GetViewAccessibility().SetName(u"Descendant_2",
                                               ax::mojom::NameFrom::kAttribute);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kTextChanged, added_view_2),
            1);
}

TEST_F(ViewTest, SetAccessibilityPropertiesRoleNameDescription) {
  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());
  auto v = std::make_unique<A11yTestView>(ax::mojom::Role::kButton, u"Name",
                                          u"Description");
  ui::AXNodeData data = ui::AXNodeData();
  v->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kButton);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(v->GetViewAccessibility().GetCachedName(), u"Name");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Name");
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kAttribute);
  EXPECT_EQ(v->GetViewAccessibility().GetCachedDescription(), u"Description");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"Description");
  EXPECT_EQ(static_cast<ax::mojom::DescriptionFrom>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kDescriptionFrom)),
            ax::mojom::DescriptionFrom::kAriaDescription);

  // There should not be any accessibility events fired when properties are
  // set within `ViewAccessibility::SetProperties` since this will be done in
  // constructors before the view is added to the tree. For the above
  // properties, the only event type is `kTextChanged`.
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kTextChanged, v.get()), 0);

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* added_view = root->AddChildView(std::move(v));

  // Setting the accessible name after being added to the tree should result in
  // an event being fired.
  added_view->GetViewAccessibility().SetName(u"New Name");
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kTextChanged, added_view), 1);
}

TEST_F(ViewTest, SetAccessibilityPropertiesRoleNameDescriptionDetailed) {
  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());
  auto v = std::make_unique<A11yTestView>(
      ax::mojom::Role::kButton, u"Name", u"Description",
      /*role_description*/ u"", ax::mojom::NameFrom::kContents,
      ax::mojom::DescriptionFrom::kTitle);
  ui::AXNodeData data = ui::AXNodeData();
  v->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kButton);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(v->GetViewAccessibility().GetCachedName(), u"Name");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Name");
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kContents);
  EXPECT_EQ(v->GetViewAccessibility().GetCachedDescription(), u"Description");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"Description");
  EXPECT_EQ(static_cast<ax::mojom::DescriptionFrom>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kDescriptionFrom)),
            ax::mojom::DescriptionFrom::kTitle);

  // There should not be any accessibility events fired when properties are
  // set within `ViewAccessibility::SetProperties` before the view is added to
  // the tree. For the above properties, the only event type is `kTextChanged`.
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kTextChanged, v.get()), 0);

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* added_view = root->AddChildView(std::move(v));

  // Setting the accessible name after initialization should result in an event
  // being fired.
  added_view->GetViewAccessibility().SetName(u"New Name");
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kTextChanged, added_view), 1);
}

TEST_F(ViewTest, SetAccessibilityPropertiesRoleRolenameNameDescription) {
  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());
  auto v = std::make_unique<A11yTestView>(ax::mojom::Role::kButton, u"Name",
                                          u"Description", u"Super Button");
  ui::AXNodeData data = ui::AXNodeData();
  v->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kButton);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kRoleDescription),
      u"Super Button");
  EXPECT_EQ(v->GetViewAccessibility().GetCachedName(), u"Name");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Name");
  EXPECT_EQ(v->GetViewAccessibility().GetCachedDescription(), u"Description");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"Description");

  // There should not be any accessibility events fired when properties are
  // set within `ViewAccessibility::SetProperties` before the view is added to
  // the tree. For the above properties, the only event type is `kTextChanged`.
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kTextChanged, v.get()), 0);

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* added_view = root->AddChildView(std::move(v));

  // Setting the accessible name after initialization should result in an event
  // being fired.
  added_view->GetViewAccessibility().SetName(u"New Name");
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kTextChanged, added_view), 1);
}

TEST_F(ViewTest, SetAccessibilityPropertiesRoleAndRoleDescription) {
  A11yTestView v(ax::mojom::Role::kButton,
                 /*name*/ std::nullopt,
                 /*description*/ std::nullopt, u"Super Button");
  ui::AXNodeData data = ui::AXNodeData();
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedRole(), ax::mojom::Role::kButton);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kRoleDescription),
      u"Super Button");
}

TEST_F(ViewTest, SetAccessibilityPropertiesNameExplicitlyEmpty) {
  A11yTestView v(ax::mojom::Role::kNone,
                 /*name*/ u"",
                 /*description*/ u"",
                 /*role_description*/ u"",
                 ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  ui::AXNodeData data = ui::AXNodeData();
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedRole(), ax::mojom::Role::kNone);
  EXPECT_EQ(data.role, ax::mojom::Role::kNone);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedName(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

TEST_F(ViewTest, SetAccessibleRole) {
  TestView v;
  ui::AXNodeData data = ui::AXNodeData();
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kUnknown);
  EXPECT_EQ(data.role, ax::mojom::Role::kUnknown);

  data = ui::AXNodeData();
  v.GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedRole(), ax::mojom::Role::kButton);
}

TEST_F(ViewTest, SetAccessibleNameToStringWithRoleAlreadySet) {
  auto v = std::make_unique<TestView>();
  v->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* added_view = root->AddChildView(std::move(v));

  ui::AXNodeData data = ui::AXNodeData();
  added_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(added_view->GetViewAccessibility().GetCachedName(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");

  added_view->last_a11y_event_ = ax::mojom::Event::kNone;
  data = ui::AXNodeData();

  added_view->GetViewAccessibility().SetName(u"Name");
  added_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(added_view->GetViewAccessibility().GetCachedName(), u"Name");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Name");
  EXPECT_EQ(added_view->last_a11y_event_, ax::mojom::Event::kTextChanged);
}

TEST_F(ViewTest, AdjustAccessibleNameStringWithRoleAlreadySet) {
  auto v = std::make_unique<A11yTestView>(ax::mojom::Role::kButton);
  v->SetAccessibleNamePrefix(u"Prefix: ");

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* added_view = root->AddChildView(std::move(v));

  ui::AXNodeData data;
  added_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(added_view->GetViewAccessibility().GetCachedName(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");

  added_view->last_a11y_event_ = ax::mojom::Event::kNone;
  data = ui::AXNodeData();

  added_view->GetViewAccessibility().SetName(u"Name");
  added_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(added_view->GetViewAccessibility().GetCachedName(),
            u"Prefix: Name");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Prefix: Name");
  EXPECT_EQ(added_view->last_a11y_event_, ax::mojom::Event::kTextChanged);
}

TEST_F(ViewTest, SetAccessibleNameToLabelWithRoleAlreadySet) {
  TestView label;
  label.GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  label.SetAccessibleName(u"Label's Name");

  auto v = std::make_unique<TestView>();
  v->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* added_view = root->AddChildView(std::move(v));

  ui::AXNodeData data = ui::AXNodeData();
  added_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(added_view->GetViewAccessibility().GetCachedName(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kNone);
  EXPECT_FALSE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds));

  added_view->last_a11y_event_ = ax::mojom::Event::kNone;
  data = ui::AXNodeData();

  added_view->GetViewAccessibility().SetName(label);
  added_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(added_view->GetViewAccessibility().GetCachedName(),
            u"Label's Name");
  EXPECT_TRUE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds));
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kRelatedElement);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Label's Name");
  EXPECT_EQ(added_view->last_a11y_event_, ax::mojom::Event::kTextChanged);
}

TEST_F(ViewTest, AdjustAccessibleNameFrom) {
  auto v = std::make_unique<A11yTestView>(ax::mojom::Role::kTextField);
  v->SetAccessibleNameFrom(ax::mojom::NameFrom::kPlaceholder);

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* added_view = root->AddChildView(std::move(v));

  ui::AXNodeData data = ui::AXNodeData();
  added_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(added_view->GetViewAccessibility().GetCachedName(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");

  added_view->last_a11y_event_ = ax::mojom::Event::kNone;
  data = ui::AXNodeData();

  added_view->GetViewAccessibility().SetName(u"Name");
  added_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(added_view->GetViewAccessibility().GetCachedName(), u"Name");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Name");
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kPlaceholder);
  EXPECT_EQ(added_view->last_a11y_event_, ax::mojom::Event::kTextChanged);
}

TEST_F(ViewTest, AdjustAccessibleNameFromLabelWithRoleAlreadySet) {
  TestView label;
  label.GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  label.SetAccessibleName(u"Label's Name");

  auto v = std::make_unique<A11yTestView>(ax::mojom::Role::kButton);
  v->SetAccessibleNamePrefix(u"Prefix: ");

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* added_view = root->AddChildView(std::move(v));

  ui::AXNodeData data = ui::AXNodeData();
  added_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(added_view->GetViewAccessibility().GetCachedName(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kNone);
  EXPECT_FALSE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds));

  added_view->last_a11y_event_ = ax::mojom::Event::kNone;
  data = ui::AXNodeData();

  added_view->SetAccessibleName(&label);
  added_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(added_view->GetViewAccessibility().GetCachedName(),
            u"Prefix: Label's Name");
  EXPECT_TRUE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds));
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kRelatedElement);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Prefix: Label's Name");
  EXPECT_EQ(added_view->last_a11y_event_, ax::mojom::Event::kTextChanged);
}

TEST_F(ViewTest, SetAccessibleNameExplicitlyEmpty) {
  TestView v;
  ui::AXNodeData data = ui::AXNodeData();
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedName(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kNone);

  data = ui::AXNodeData();
  v.SetAccessibleName(u"", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedName(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

TEST_F(ViewTest, SetAccessibleNameExplicitlyEmptyToRemoveName) {
  TestView v;
  ui::AXNodeData data = ui::AXNodeData();
  v.GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  v.SetAccessibleName(u"Name");
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedName(), u"Name");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Name");

  data = ui::AXNodeData();
  v.SetAccessibleName(u"", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedName(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");
  EXPECT_EQ(static_cast<ax::mojom::NameFrom>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)),
            ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

TEST_F(ViewTest, SetAccessibleDescriptionToString) {
  TestView v;
  ui::AXNodeData data = ui::AXNodeData();
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedDescription(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"");

  data = ui::AXNodeData();
  v.GetViewAccessibility().SetDescription(u"Description");
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedDescription(), u"Description");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"Description");
}

TEST_F(ViewTest, SetAccessibleDescriptionToLabel) {
  TestView label;
  label.GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  label.SetAccessibleName(u"Label's Name");

  TestView v;
  v.GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  ui::AXNodeData data = ui::AXNodeData();
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedDescription(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"");
  EXPECT_EQ(static_cast<ax::mojom::DescriptionFrom>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kDescriptionFrom)),
            ax::mojom::DescriptionFrom::kNone);
  EXPECT_FALSE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kDescribedbyIds));

  data = ui::AXNodeData();
  v.GetViewAccessibility().SetDescription(label);
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedDescription(), u"Label's Name");
  EXPECT_TRUE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kDescribedbyIds));
  EXPECT_EQ(static_cast<ax::mojom::DescriptionFrom>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kDescriptionFrom)),
            ax::mojom::DescriptionFrom::kRelatedElement);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"Label's Name");
}

TEST_F(ViewTest, SetAccessibleDescriptionExplicitlyEmpty) {
  TestView v;
  ui::AXNodeData data = ui::AXNodeData();
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedDescription(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"");
  EXPECT_EQ(static_cast<ax::mojom::DescriptionFrom>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kDescriptionFrom)),
            ax::mojom::DescriptionFrom::kNone);

  data = ui::AXNodeData();
  v.GetViewAccessibility().SetDescription(
      u"", ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedDescription(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"");
  EXPECT_EQ(static_cast<ax::mojom::DescriptionFrom>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kDescriptionFrom)),
            ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
}

TEST_F(ViewTest, SetAccessibleDescriptionExplicitlyEmptyToRemoveDescription) {
  TestView v;
  ui::AXNodeData data = ui::AXNodeData();
  v.GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  v.GetViewAccessibility().SetDescription(u"Description");
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedDescription(), u"Description");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"Description");

  data = ui::AXNodeData();
  v.GetViewAccessibility().SetDescription(
      u"", ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  v.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedDescription(), u"");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"");
  EXPECT_EQ(static_cast<ax::mojom::DescriptionFrom>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kDescriptionFrom)),
            ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
}

TEST_F(ViewTest, SetIsLeafUnpruneSubtreeWithLeafView) {
  TestView v;
  ui::AXNodeData data = ui::AXNodeData();
  v.GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  EXPECT_EQ(v.GetViewAccessibility().GetCachedRole(), ax::mojom::Role::kButton);

  v.AddChildView(std::make_unique<TestView>());
  auto v2 = v.children()[0];
  v2->AddChildView(std::make_unique<TestView>());
  auto v3 = v2->children()[0];

  v2->GetViewAccessibility().SetIsLeaf(true);
  EXPECT_EQ(v2->GetViewAccessibility().ViewAccessibility::IsLeaf(), true);
  EXPECT_EQ(v2->GetViewAccessibility().GetIsPruned(), false);
  EXPECT_EQ(v3->GetViewAccessibility().ViewAccessibility::IsLeaf(), false);
  EXPECT_EQ(v3->GetViewAccessibility().GetIsPruned(), true);

  // When we set the parent view to be a leaf, the child view should be pruned.
  v.GetViewAccessibility().SetIsLeaf(true);
  EXPECT_EQ(v.GetViewAccessibility().ViewAccessibility::IsLeaf(), true);
  EXPECT_EQ(v2->GetViewAccessibility().GetIsPruned(), true);
  EXPECT_EQ(v3->GetViewAccessibility().GetIsPruned(), true);

  // If we unset the parent as a leaf, we should unprune the child, but it
  // should remain as leaf since we explcitly set it as so.
  v.GetViewAccessibility().SetIsLeaf(false);
  EXPECT_EQ(v.GetViewAccessibility().ViewAccessibility::IsLeaf(), false);
  EXPECT_EQ(v2->GetViewAccessibility().ViewAccessibility::IsLeaf(), true);
  EXPECT_EQ(v2->GetViewAccessibility().GetIsPruned(), false);
  EXPECT_EQ(v3->GetViewAccessibility().GetIsPruned(), true);
}

////////////////////////////////////////////////////////////////////////////////
// OnBoundsChanged
////////////////////////////////////////////////////////////////////////////////

TEST_F(ViewTest, OnBoundsChangedFiresA11yEvent) {
  auto view = std::make_unique<TestView>();

  // No events will be sent if the view is not connected to a RootView.
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* v = root->AddChildView(std::move(view));

  // Should change when scaled or moved.
  gfx::Rect initial(0, 0, 200, 200);
  gfx::Rect scaled(0, 0, 250, 250);
  gfx::Rect moved(100, 100, 250, 250);

  v->last_a11y_event_ = ax::mojom::Event::kNone;
  v->SetBoundsRect(initial);
  EXPECT_EQ(v->last_a11y_event_, ax::mojom::Event::kLocationChanged);

  v->last_a11y_event_ = ax::mojom::Event::kNone;
  v->SetBoundsRect(scaled);
  EXPECT_EQ(v->last_a11y_event_, ax::mojom::Event::kLocationChanged);

  v->last_a11y_event_ = ax::mojom::Event::kNone;
  v->SetBoundsRect(moved);
  EXPECT_EQ(v->last_a11y_event_, ax::mojom::Event::kLocationChanged);
}

TEST_F(ViewTest, OnBoundsChanged) {
  TestView v;

  gfx::Rect prev_rect(0, 0, 200, 200);
  gfx::Rect new_rect(100, 100, 250, 250);

  v.SetBoundsRect(prev_rect);
  v.Reset();
  v.SetBoundsRect(new_rect);

  EXPECT_TRUE(v.did_change_bounds_);
  EXPECT_EQ(v.new_bounds_, new_rect);
  EXPECT_EQ(v.bounds(), new_rect);
}

TEST_F(ViewTest, TransformFiresA11yEvent) {
  auto view = std::make_unique<TestView>();

  // No events will be sent if the view is not connected to a RootView.
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* v = root->AddChildView(std::move(view));

  v->SetPaintToLayer();

  gfx::Rect bounds(0, 0, 200, 200);
  v->last_a11y_event_ = ax::mojom::Event::kNone;
  v->SetBoundsRect(bounds);
  EXPECT_EQ(v->last_a11y_event_, ax::mojom::Event::kLocationChanged);

  gfx::Transform transform;
  transform.Translate(gfx::Vector2dF(10, 10));
  v->last_a11y_event_ = ax::mojom::Event::kNone;
  v->layer()->SetTransform(transform);
  EXPECT_EQ(v->last_a11y_event_, ax::mojom::Event::kLocationChanged);
}

////////////////////////////////////////////////////////////////////////////////
// OnStateChanged
////////////////////////////////////////////////////////////////////////////////

TEST_F(ViewTest, OnStateChangedFiresA11yEvent) {
  auto view = std::make_unique<TestView>();

  // No events will be sent if the view is not connected to a RootView.
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  auto* v = root->AddChildView(std::move(view));

  v->last_a11y_event_ = ax::mojom::Event::kNone;
  v->SetEnabled(false);
  EXPECT_EQ(v->last_a11y_event_, ax::mojom::Event::kStateChanged);

  v->last_a11y_event_ = ax::mojom::Event::kNone;
  v->SetEnabled(true);
  EXPECT_EQ(v->last_a11y_event_, ax::mojom::Event::kStateChanged);
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent
////////////////////////////////////////////////////////////////////////////////

TEST_F(ViewTest, MouseEvent) {
  auto view1 = std::make_unique<TestView>();
  view1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));

  auto view2 = std::make_unique<TestView>();
  view2->SetBoundsRect(gfx::Rect(100, 100, 100, 100));

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  TestView* v1 = root->AddChildView(std::move(view1));
  TestView* v2 = v1->AddChildView(std::move(view2));

  v1->Reset();
  v2->Reset();

  gfx::Point p1(110, 120);
  ui::MouseEvent pressed(ui::EventType::kMousePressed, p1, p1,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(pressed);
  EXPECT_EQ(v2->last_mouse_event_type_, ui::EventType::kMousePressed);
  EXPECT_EQ(v2->location_.x(), 10);
  EXPECT_EQ(v2->location_.y(), 20);
  // Make sure v1 did not receive the event
  EXPECT_EQ(v1->last_mouse_event_type_, ui::EventType::kUnknown);

  // Drag event out of bounds. Should still go to v2
  v1->Reset();
  v2->Reset();
  gfx::Point p2(50, 40);
  ui::MouseEvent dragged(ui::EventType::kMouseDragged, p2, p2,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  root->OnMouseDragged(dragged);
  EXPECT_EQ(v2->last_mouse_event_type_, ui::EventType::kMouseDragged);
  EXPECT_EQ(v2->location_.x(), -50);
  EXPECT_EQ(v2->location_.y(), -60);
  // Make sure v1 did not receive the event
  EXPECT_EQ(v1->last_mouse_event_type_, ui::EventType::kUnknown);

  // Releasted event out of bounds. Should still go to v2
  v1->Reset();
  v2->Reset();
  ui::MouseEvent released(ui::EventType::kMouseReleased, gfx::Point(),
                          gfx::Point(), ui::EventTimeForNow(), 0, 0);
  root->OnMouseDragged(released);
  EXPECT_EQ(v2->last_mouse_event_type_, ui::EventType::kMouseReleased);
  EXPECT_EQ(v2->location_.x(), -100);
  EXPECT_EQ(v2->location_.y(), -100);
  // Make sure v1 did not receive the event
  EXPECT_EQ(v1->last_mouse_event_type_, ui::EventType::kUnknown);
}

// Confirm that a view can be deleted as part of processing a mouse press.
TEST_F(ViewTest, DeleteOnPressed) {
  auto view1 = std::make_unique<TestView>();
  view1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));

  auto view2 = std::make_unique<TestView>();
  view2->SetBoundsRect(gfx::Rect(100, 100, 100, 100));

  view1->Reset();
  view2->Reset();

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();

  TestView* v1 = root->AddChildView(std::move(view1));
  TestView* v2 = v1->AddChildView(std::move(view2));

  v2->delete_on_pressed_ = true;
  gfx::Point point(110, 120);
  ui::MouseEvent pressed(ui::EventType::kMousePressed, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(pressed);
  EXPECT_TRUE(v1->children().empty());
}

// Detect the return value of OnMouseDragged
TEST_F(ViewTest, DetectReturnFormDrag) {
  auto view1 = std::make_unique<TestView>();
  view1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));

  auto view2 = std::make_unique<TestView>();
  view2->SetBoundsRect(gfx::Rect(100, 100, 100, 100));

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  auto* root = AsViewClass<internal::RootView>(widget->GetRootView());

  TestView* v1 = root->AddChildView(std::move(view1));
  TestView* v2 = v1->AddChildView(std::move(view2));

  v1->Reset();
  v2->Reset();
  gfx::Point p1(110, 120);
  ui::MouseEvent pressed(ui::EventType::kMousePressed, p1, p1,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(pressed);

  v1->Reset();
  v2->Reset();
  gfx::Point p2(50, 40);
  ui::MouseEvent dragged(ui::EventType::kMouseDragged, p2, p2,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  EXPECT_TRUE(root->OnMouseDragged(dragged));

  v1->Reset();
  v2->Reset();
  ui::MouseEvent released(ui::EventType::kMouseReleased, gfx::Point(),
                          gfx::Point(), ui::EventTimeForNow(), 0, 0);
  EXPECT_TRUE(root->OnMouseDragged(released));
}

////////////////////////////////////////////////////////////////////////////////
// Painting
////////////////////////////////////////////////////////////////////////////////

namespace {

// Helper class to create a Widget with standard parameters that is closed when
// the helper class goes out of scope.
class ScopedTestPaintWidget {
 public:
  explicit ScopedTestPaintWidget(Widget::InitParams params)
      : widget_(std::make_unique<Widget>()) {
    widget_->Init(std::move(params));
    widget_->GetRootView()->SetBounds(0, 0, 25, 26);
  }

  ScopedTestPaintWidget(const ScopedTestPaintWidget&) = delete;
  ScopedTestPaintWidget& operator=(const ScopedTestPaintWidget&) = delete;

  ~ScopedTestPaintWidget() = default;

  Widget* operator->() { return widget_.get(); }

 private:
  std::unique_ptr<Widget> widget_;
};

}  // namespace

TEST_F(ViewTest, PaintEmptyView) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  // |v1| is empty.
  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 0, 1);
  root_view->AddChildView(v1);

  // |v11| is a child of an empty |v1|.
  TestView* v11 = new TestView;
  v11->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v11);

  // |v2| is not.
  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  root_view->AddChildView(v2);

  // Paint "everything".
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      first_paint.size()));

  // The empty view has nothing to paint so it doesn't try build a cache, nor do
  // its children which would be clipped by its (empty) self.
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v11->did_paint_);
  EXPECT_TRUE(v2->did_paint_);
}

TEST_F(ViewTest, PaintWithMovedViewUsesCache) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();
  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect pixel_rect = gfx::Rect(1, 1);
  float device_scale_factor = 1.f;
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->PaintFromPaintRoot(
      ui::PaintContext(list.get(), device_scale_factor, pixel_rect, false));
  EXPECT_TRUE(v1->did_paint_);
  v1->Reset();
  // The visual rects for (clip, drawing, transform) should be in layer space.
  gfx::Rect expected_visual_rect_in_layer_space(10, 11, 12, 13);
  int item_index = 3;
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index));

  // If invalidation doesn't intersect v1, we paint with the cache.
  list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->PaintFromPaintRoot(
      ui::PaintContext(list.get(), device_scale_factor, pixel_rect, false));
  EXPECT_FALSE(v1->did_paint_);
  v1->Reset();

  // If invalidation does intersect v1, we don't paint with the cache.
  list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->PaintFromPaintRoot(
      ui::PaintContext(list.get(), device_scale_factor, v1->bounds(), false));
  EXPECT_TRUE(v1->did_paint_);
  v1->Reset();

  // Moving the view should still use the cache when the invalidation doesn't
  // intersect v1.
  list = base::MakeRefCounted<cc::DisplayItemList>();
  v1->SetX(9);
  root_view->PaintFromPaintRoot(
      ui::PaintContext(list.get(), device_scale_factor, pixel_rect, false));
  EXPECT_FALSE(v1->did_paint_);
  v1->Reset();
  item_index = 3;
  expected_visual_rect_in_layer_space.SetRect(9, 11, 12, 13);
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index));

  // Moving the view should not use the cache when painting without
  // invalidation.
  list = base::MakeRefCounted<cc::DisplayItemList>();
  v1->SetX(8);
  root_view->PaintFromPaintRoot(ui::PaintContext(
      ui::PaintContext(list.get(), device_scale_factor, pixel_rect, false),
      ui::PaintContext::CLONE_WITHOUT_INVALIDATION));
  EXPECT_TRUE(v1->did_paint_);
  v1->Reset();
  item_index = 3;
  expected_visual_rect_in_layer_space.SetRect(8, 11, 12, 13);
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index));
}

TEST_F(ViewTest, PaintWithMovedViewUsesCacheInRTL) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();
  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect pixel_rect = gfx::Rect(1, 1);
  float device_scale_factor = 1.f;
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->PaintFromPaintRoot(
      ui::PaintContext(list.get(), device_scale_factor, pixel_rect, false));
  EXPECT_TRUE(v1->did_paint_);
  v1->Reset();
  // The visual rects for (clip, drawing, transform) should be in layer space.
  // x: 25 - 10(x) - 12(width) = 3
  gfx::Rect expected_visual_rect_in_layer_space(3, 11, 12, 13);
  int item_index = 3;
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index));

  // If invalidation doesn't intersect v1, we paint with the cache.
  list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->PaintFromPaintRoot(
      ui::PaintContext(list.get(), device_scale_factor, pixel_rect, false));
  EXPECT_FALSE(v1->did_paint_);
  v1->Reset();

  // If invalidation does intersect v1, we don't paint with the cache.
  list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->PaintFromPaintRoot(
      ui::PaintContext(list.get(), device_scale_factor, v1->bounds(), false));
  EXPECT_TRUE(v1->did_paint_);
  v1->Reset();

  // Moving the view should still use the cache when the invalidation doesn't
  // intersect v1.
  list = base::MakeRefCounted<cc::DisplayItemList>();
  v1->SetX(9);
  root_view->PaintFromPaintRoot(
      ui::PaintContext(list.get(), device_scale_factor, pixel_rect, false));
  EXPECT_FALSE(v1->did_paint_);
  v1->Reset();
  item_index = 3;
  // x: 25 - 9(x) - 12(width) = 4
  expected_visual_rect_in_layer_space.SetRect(4, 11, 12, 13);
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index));

  // Moving the view should not use the cache when painting without
  // invalidation.
  list = base::MakeRefCounted<cc::DisplayItemList>();
  v1->SetX(8);
  root_view->PaintFromPaintRoot(ui::PaintContext(
      ui::PaintContext(list.get(), device_scale_factor, pixel_rect, false),
      ui::PaintContext::CLONE_WITHOUT_INVALIDATION));
  EXPECT_TRUE(v1->did_paint_);
  v1->Reset();
  item_index = 3;
  // x: 25 - 8(x) - 12(width) = 5
  expected_visual_rect_in_layer_space.SetRect(5, 11, 12, 13);
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index++));
  EXPECT_EQ(expected_visual_rect_in_layer_space,
            list->VisualRectForTesting(item_index));
}

TEST_F(ViewTest, PaintWithUnknownInvalidation) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v2);

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->PaintFromPaintRoot(
      ui::PaintContext(list.get(), 1.f, first_paint, false));
  v1->Reset();
  v2->Reset();

  gfx::Rect paint_area(1, 1);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  // With a known invalidation, v1 and v2 are not painted.
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->PaintFromPaintRoot(
      ui::PaintContext(list.get(), 1.f, paint_area, false));
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);

  // With unknown invalidation, v1 and v2 are painted.
  root_view->PaintFromPaintRoot(
      ui::PaintContext(ui::PaintContext(list.get(), 1.f, paint_area, false),
                       ui::PaintContext::CLONE_WITHOUT_INVALIDATION));
  EXPECT_TRUE(v1->did_paint_);
  EXPECT_TRUE(v2->did_paint_);
}

TEST_F(ViewTest, PaintContainsChildren) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v2);

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();

  gfx::Rect paint_area(25, 26);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_TRUE(v1->did_paint_);
  EXPECT_TRUE(v2->did_paint_);
}

TEST_F(ViewTest, PaintContainsChildrenInRTL) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v2);

  // Verify where the layers actually appear.
  v1->SetPaintToLayer();
  // x: 25 - 10(x) - 12(width) = 3
  EXPECT_EQ(gfx::Rect(3, 11, 12, 13), v1->layer()->bounds());
  v1->DestroyLayer();

  v2->SetPaintToLayer();
  // x: 25 - 10(parent x) - 3(x) - 6(width) = 6
  EXPECT_EQ(gfx::Rect(6, 15, 6, 5), v2->layer()->bounds());
  v2->DestroyLayer();

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();

  gfx::Rect paint_area(25, 26);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_TRUE(v1->did_paint_);
  EXPECT_TRUE(v2->did_paint_);
}

TEST_F(ViewTest, PaintIntersectsChildren) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v2);

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();

  gfx::Rect paint_area(9, 10, 5, 6);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_TRUE(v1->did_paint_);
  EXPECT_TRUE(v2->did_paint_);
}

TEST_F(ViewTest, PaintIntersectsChildrenInRTL) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v2);

  // Verify where the layers actually appear.
  v1->SetPaintToLayer();
  // x: 25 - 10(x) - 12(width) = 3
  EXPECT_EQ(gfx::Rect(3, 11, 12, 13), v1->layer()->bounds());
  v1->DestroyLayer();

  v2->SetPaintToLayer();
  // x: 25 - 10(parent x) - 3(x) - 6(width) = 6
  EXPECT_EQ(gfx::Rect(6, 15, 6, 5), v2->layer()->bounds());
  v2->DestroyLayer();

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();

  gfx::Rect paint_area(2, 10, 5, 6);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_TRUE(v1->did_paint_);
  EXPECT_TRUE(v2->did_paint_);
}

TEST_F(ViewTest, PaintIntersectsChildButNotGrandChild) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v2);

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();

  gfx::Rect paint_area(9, 10, 2, 3);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_TRUE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
}

TEST_F(ViewTest, PaintIntersectsChildButNotGrandChildInRTL) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v2);

  // Verify where the layers actually appear.
  v1->SetPaintToLayer();
  // x: 25 - 10(x) - 12(width) = 3
  EXPECT_EQ(gfx::Rect(3, 11, 12, 13), v1->layer()->bounds());
  v1->DestroyLayer();

  v2->SetPaintToLayer();
  // x: 25 - 10(parent x) - 3(x) - 6(width) = 6
  EXPECT_EQ(gfx::Rect(6, 15, 6, 5), v2->layer()->bounds());
  v2->DestroyLayer();

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();

  gfx::Rect paint_area(2, 10, 2, 3);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_TRUE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
}

TEST_F(ViewTest, PaintIntersectsNoChildren) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v2);

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();

  gfx::Rect paint_area(9, 10, 2, 1);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
}

TEST_F(ViewTest, PaintIntersectsNoChildrenInRTL) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v2);

  // Verify where the layers actually appear.
  v1->SetPaintToLayer();
  // x: 25 - 10(x) - 12(width) = 3
  EXPECT_EQ(gfx::Rect(3, 11, 12, 13), v1->layer()->bounds());
  v1->DestroyLayer();

  v2->SetPaintToLayer();
  // x: 25 - 10(parent x) - 3(x) - 6(width) = 6
  EXPECT_EQ(gfx::Rect(6, 15, 6, 5), v2->layer()->bounds());
  v2->DestroyLayer();

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();

  gfx::Rect paint_area(2, 10, 2, 1);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
}

TEST_F(ViewTest, PaintIntersectsOneChild) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  root_view->AddChildView(v2);

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();

  // Intersects with the second child only.
  gfx::Rect paint_area(3, 3, 1, 2);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_TRUE(v2->did_paint_);

  // Intersects with the first child only.
  paint_area = gfx::Rect(20, 10, 1, 2);

  v1->Reset();
  v2->Reset();
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_TRUE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
}

TEST_F(ViewTest, PaintIntersectsOneChildInRTL) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  root_view->AddChildView(v2);

  // Verify where the layers actually appear.
  v1->SetPaintToLayer();
  // x: 25 - 10(x) - 12(width) = 3
  EXPECT_EQ(gfx::Rect(3, 11, 12, 13), v1->layer()->bounds());
  v1->DestroyLayer();

  v2->SetPaintToLayer();
  // x: 25 - 3(x) - 6(width) = 16
  EXPECT_EQ(gfx::Rect(16, 4, 6, 5), v2->layer()->bounds());
  v2->DestroyLayer();

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();

  // Intersects with the first child only.
  gfx::Rect paint_area(3, 10, 1, 2);
  gfx::Rect root_area(root_view->size());
  list = base::MakeRefCounted<cc::DisplayItemList>();

  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_TRUE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);

  // Intersects with the second child only.
  paint_area = gfx::Rect(21, 3, 1, 2);

  v1->Reset();
  v2->Reset();
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), root_view->size()));
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_TRUE(v2->did_paint_);
}

TEST_F(ViewTest, PaintInPromotedToLayer) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = new TestView;
  v1->SetPaintToLayer();
  v1->SetBounds(10, 11, 12, 13);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(3, 4, 6, 5);
  v1->AddChildView(v2);

  {
    // Paint everything once, since it has to build its cache. Then we can test
    // invalidation.
    gfx::Rect first_paint(1, 1);
    auto list = base::MakeRefCounted<cc::DisplayItemList>();
    v1->Paint(PaintInfo::CreateRootPaintInfo(
        ui::PaintContext(list.get(), 1.f, first_paint, false), v1->size()));
    v1->Reset();
    v2->Reset();
  }

  {
    gfx::Rect paint_area(25, 26);
    gfx::Rect view_area(root_view->size());
    auto list = base::MakeRefCounted<cc::DisplayItemList>();

    // The promoted views are not painted as they are separate paint roots.
    root_view->Paint(PaintInfo::CreateRootPaintInfo(
        ui::PaintContext(list.get(), 1.f, paint_area, false),
        root_view->size()));
    EXPECT_FALSE(v1->did_paint_);
    EXPECT_FALSE(v2->did_paint_);
  }

  {
    gfx::Rect paint_area(1, 1);
    gfx::Rect view_area(v1->size());
    auto list = base::MakeRefCounted<cc::DisplayItemList>();

    // The |v1| view is painted. If it used its offset incorrect, it would think
    // its at (10,11) instead of at (0,0) since it is the paint root.
    v1->Paint(PaintInfo::CreateRootPaintInfo(
        ui::PaintContext(list.get(), 1.f, paint_area, false), v1->size()));
    EXPECT_TRUE(v1->did_paint_);
    EXPECT_FALSE(v2->did_paint_);
  }

  v1->Reset();

  {
    gfx::Rect paint_area(3, 3, 1, 2);
    gfx::Rect view_area(v1->size());
    auto list = base::MakeRefCounted<cc::DisplayItemList>();

    // The |v2| view is painted also. If it used its offset incorrect, it would
    // think its at (13,15) instead of at (3,4) since |v1| is the paint root.
    v1->Paint(PaintInfo::CreateRootPaintInfo(
        ui::PaintContext(list.get(), 1.f, paint_area, false), v1->size()));
    EXPECT_TRUE(v1->did_paint_);
    EXPECT_TRUE(v2->did_paint_);
  }
}

// A derived class for testing paint.
class TestPaintView : public TestView {
  METADATA_HEADER(TestPaintView, TestView)

 public:
  TestPaintView() = default;
  ~TestPaintView() override = default;

  void OnPaint(gfx::Canvas* canvas) override {
    did_paint_ = true;
    // Get the bounds from the canvas this view paints to.
    EXPECT_TRUE(canvas->GetClipBounds(&canvas_bounds_));
  }

  gfx::Rect canvas_bounds() const { return canvas_bounds_; }

 private:
  gfx::Rect canvas_bounds_;
};

BEGIN_METADATA(TestPaintView)
END_METADATA

TEST_F(ViewTest, PaintLocalBounds) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();
  // Make |root_view|'s bounds larger so |v1|'s visible bounds is not clipped by
  // |root_view|.
  root_view->SetBounds(0, 0, 200, 200);

  TestPaintView* v1 = new TestPaintView;
  v1->SetPaintToLayer();

  // Set bounds for |v1| such that it has an offset to its parent and only part
  // of it is visible. The visible bounds does not intersect with |root_view|'s
  // bounds.
  v1->SetBounds(0, -1000, 100, 1100);
  root_view->AddChildView(v1);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 1100), v1->GetLocalBounds());
  EXPECT_EQ(gfx::Rect(0, 1000, 100, 100), v1->GetVisibleBounds());

  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  ui::PaintContext context(list.get(), 1.f, gfx::Rect(), false);

  v1->Paint(PaintInfo::CreateRootPaintInfo(context, gfx::Size()));
  EXPECT_TRUE(v1->did_paint_);

  // Check that the canvas produced by |v1| for paint contains all of |v1|'s
  // visible bounds.
  EXPECT_TRUE(v1->canvas_bounds().Contains(v1->GetVisibleBounds()));
}

namespace {

gfx::Transform RotationCounterclockwise() {
  return gfx::Transform::Make270degRotation();
}

gfx::Transform RotationClockwise() {
  return gfx::Transform::Make90degRotation();
}

}  // namespace

// Tests the correctness of the rect-based targeting algorithm implemented in
// View::GetEventHandlerForRect(). See http://goo.gl/3Jp2BD for a description
// of rect-based targeting.
TEST_F(ViewTest, GetEventHandlerForRect) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(params));
  View* root_view = widget->GetRootView();
  root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));

  // Have this hierarchy of views (the coordinates here are all in
  // the root view's coordinate space):
  // v1 (0, 0, 100, 100)
  // v2 (150, 0, 250, 100)
  // v3 (0, 200, 150, 100)
  //     v31 (10, 210, 80, 80)
  //     v32 (110, 210, 30, 80)
  // v4 (300, 200, 100, 100)
  //     v41 (310, 210, 80, 80)
  //         v411 (370, 275, 10, 5)
  // v5 (450, 197, 30, 36)
  //     v51 (450, 200, 30, 30)

  // The coordinates used for SetBounds are in parent coordinates.

  TestView* v1 = new TestView;
  v1->SetBounds(0, 0, 100, 100);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(150, 0, 250, 100);
  root_view->AddChildView(v2);

  TestView* v3 = new TestView;
  v3->SetBounds(0, 200, 150, 100);
  root_view->AddChildView(v3);

  TestView* v4 = new TestView;
  v4->SetBounds(300, 200, 100, 100);
  root_view->AddChildView(v4);

  TestView* v31 = new TestView;
  v31->SetBounds(10, 10, 80, 80);
  v3->AddChildView(v31);

  TestView* v32 = new TestView;
  v32->SetBounds(110, 10, 30, 80);
  v3->AddChildView(v32);

  TestView* v41 = new TestView;
  v41->SetBounds(10, 10, 80, 80);
  v4->AddChildView(v41);

  TestView* v411 = new TestView;
  v411->SetBounds(60, 65, 10, 5);
  v41->AddChildView(v411);

  TestView* v5 = new TestView;
  v5->SetBounds(450, 197, 30, 36);
  root_view->AddChildView(v5);

  TestView* v51 = new TestView;
  v51->SetBounds(0, 3, 30, 30);
  v5->AddChildView(v51);

  // |touch_rect| does not intersect any descendant view of |root_view|.
  gfx::Rect touch_rect(105, 105, 30, 45);
  View* result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(root_view, result_view);
  result_view = nullptr;

  // Covers |v1| by at least 60%.
  touch_rect.SetRect(15, 15, 100, 100);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v1, result_view);
  result_view = nullptr;

  // Intersects |v1| but does not cover it by at least 60%. The center
  // of |touch_rect| is within |v1|.
  touch_rect.SetRect(50, 50, 5, 10);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v1, result_view);
  result_view = nullptr;

  // Intersects |v1| but does not cover it by at least 60%. The center
  // of |touch_rect| is not within |v1|.
  touch_rect.SetRect(95, 96, 21, 22);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(root_view, result_view);
  result_view = nullptr;

  // Intersects |v1| and |v2|, but only covers |v2| by at least 60%.
  touch_rect.SetRect(95, 10, 300, 120);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v2, result_view);
  result_view = nullptr;

  // Covers both |v1| and |v2| by at least 60%, but the center point
  // of |touch_rect| is closer to the center point of |v2|.
  touch_rect.SetRect(20, 20, 400, 100);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v2, result_view);
  result_view = nullptr;

  // Covers both |v1| and |v2| by at least 60%, but the center point
  // of |touch_rect| is closer to the center point of |v1|.
  touch_rect.SetRect(-700, -15, 1050, 110);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v1, result_view);
  result_view = nullptr;

  // A mouse click within |v1| will target |v1|.
  touch_rect.SetRect(15, 15, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v1, result_view);
  result_view = nullptr;

  // Intersects |v3| and |v31| by at least 60% and the center point
  // of |touch_rect| is closer to the center point of |v31|.
  touch_rect.SetRect(0, 200, 110, 100);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v31, result_view);
  result_view = nullptr;

  // Intersects |v3| and |v31|, but neither by at least 60%. The
  // center point of |touch_rect| lies within |v31|.
  touch_rect.SetRect(80, 280, 15, 15);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v31, result_view);
  result_view = nullptr;

  // Covers |v3|, |v31|, and |v32| all by at least 60%, and the
  // center point of |touch_rect| is closest to the center point
  // of |v32|.
  touch_rect.SetRect(0, 200, 200, 100);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v32, result_view);
  result_view = nullptr;

  // Intersects all of |v3|, |v31|, and |v32|, but only covers
  // |v31| and |v32| by at least 60%. The center point of
  // |touch_rect| is closest to the center point of |v32|.
  touch_rect.SetRect(30, 225, 180, 115);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v32, result_view);
  result_view = nullptr;

  // A mouse click at the corner of |v3| will target |v3|.
  touch_rect.SetRect(0, 200, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v3, result_view);
  result_view = nullptr;

  // A mouse click within |v32| will target |v32|.
  touch_rect.SetRect(112, 211, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v32, result_view);
  result_view = nullptr;

  // Covers all of |v4|, |v41|, and |v411| by at least 60%.
  // The center point of |touch_rect| is equally close to
  // the center points of |v4| and |v41|.
  touch_rect.SetRect(310, 210, 80, 80);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  // |v411| is the deepest view that is completely contained by |touch_rect|.
  EXPECT_EQ(v411, result_view);
  result_view = nullptr;

  // Intersects all of |v4|, |v41|, and |v411| but only covers
  // |v411| by at least 60%.
  touch_rect.SetRect(370, 275, 7, 5);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v411, result_view);
  result_view = nullptr;

  // Intersects |v4| and |v41| but covers neither by at least 60%.
  // The center point of |touch_rect| is equally close to the center
  // points of |v4| and |v41|.
  touch_rect.SetRect(345, 245, 7, 7);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v41, result_view);
  result_view = nullptr;

  // Intersects all of |v4|, |v41|, and |v411| and covers none of
  // them by at least 60%. The center point of |touch_rect| lies
  // within |v411|.
  touch_rect.SetRect(368, 272, 4, 6);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v411, result_view);
  result_view = nullptr;

  // Intersects all of |v4|, |v41|, and |v411| and covers none of
  // them by at least 60%. The center point of |touch_rect| lies
  // within |v41|.
  touch_rect.SetRect(365, 270, 7, 7);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v41, result_view);
  result_view = nullptr;

  // Intersects all of |v4|, |v41|, and |v411| and covers none of
  // them by at least 60%. The center point of |touch_rect| lies
  // within |v4|.
  touch_rect.SetRect(205, 275, 200, 2);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v4, result_view);
  result_view = nullptr;

  // Intersects all of |v4|, |v41|, and |v411| but only covers
  // |v41| by at least 60%.
  touch_rect.SetRect(310, 210, 61, 66);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v41, result_view);
  result_view = nullptr;

  // A mouse click within |v411| will target |v411|.
  touch_rect.SetRect(372, 275, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v411, result_view);
  result_view = nullptr;

  // A mouse click within |v41| will target |v41|.
  touch_rect.SetRect(350, 215, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v41, result_view);
  result_view = nullptr;

  // Covers |v3|, |v4|, and all of their descendants by at
  // least 60%. The center point of |touch_rect| is closest
  // to the center point of |v32|.
  touch_rect.SetRect(0, 200, 400, 100);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v32, result_view);
  result_view = nullptr;

  // Intersects all of |v2|, |v3|, |v32|, |v4|, |v41|, and |v411|.
  // Covers |v2|, |v32|, |v4|, |v41|, and |v411| by at least 60%.
  touch_rect.SetRect(110, 15, 375, 450);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  // Target is |v411| as it is the deepest view touched by at least 60% of the
  // rect.
  EXPECT_EQ(v411, result_view);
  result_view = nullptr;

  // Covers all views (except |v5| and |v51|) by at least 60%. The
  // center point of |touch_rect| is equally close to the center
  // points of |v2| and |v32|.
  touch_rect.SetRect(0, 0, 400, 300);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  // |v32| is the deepest view that is contained by the rest.
  EXPECT_EQ(v32, result_view);
  result_view = nullptr;

  // Covers |v5| and |v51| by at least 60%, and the center point of
  // the touch is located within both views. Since both views share
  // the same center point, the child view should be selected.
  touch_rect.SetRect(440, 190, 40, 40);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v51, result_view);
  result_view = nullptr;

  // Covers |v5| and |v51| by at least 60%, but the center point of
  // the touch is not located within either view. Since both views
  // share the same center point, the child view should be selected.
  touch_rect.SetRect(455, 187, 60, 60);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v51, result_view);
  result_view = nullptr;

  // Covers neither |v5| nor |v51| by at least 60%, but the center
  // of the touch is located within |v51|.
  touch_rect.SetRect(450, 197, 10, 10);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v51, result_view);
  result_view = nullptr;

  // Covers neither |v5| nor |v51| by at least 60% but intersects both.
  // The center point is located outside of both views.
  touch_rect.SetRect(433, 180, 24, 24);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(root_view, result_view);
  result_view = nullptr;

  // Only intersects |v5| but does not cover it by at least 60%. The
  // center point of the touch region is located within |v5|.
  touch_rect.SetRect(449, 196, 3, 3);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v5, result_view);
  result_view = nullptr;

  // A mouse click within |v5| (but not |v51|) should target |v5|.
  touch_rect.SetRect(462, 199, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v5, result_view);
  result_view = nullptr;

  // A mouse click |v5| and |v51| should target the child view.
  touch_rect.SetRect(452, 226, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v51, result_view);
  result_view = nullptr;

  // A mouse click on the center of |v5| and |v51| should target
  // the child view.
  touch_rect.SetRect(465, 215, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v51, result_view);
  result_view = nullptr;

  widget->CloseNow();
}

// Tests that GetEventHandlerForRect() and GetTooltipHandlerForPoint() behave
// as expected when different views in the view hierarchy return false
// when GetCanProcessEventsWithinSubtree() is called.
TEST_F(ViewTest, GetCanProcessEventsWithinSubtree) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(params));
  View* root_view = widget->GetRootView();
  root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));

  // Have this hierarchy of views (the coords here are in the coordinate
  // space of the root view):
  // v (0, 0, 100, 100)
  //  - v_child (0, 0, 20, 30)
  //    - v_grandchild (5, 5, 5, 15)

  TestView* v = new TestView;
  v->SetBounds(0, 0, 100, 100);
  root_view->AddChildView(v);
  v->SetNotifyEnterExitOnChild(true);

  TestView* v_child = new TestView;
  v_child->SetBounds(0, 0, 20, 30);
  v->AddChildView(v_child);

  TestView* v_grandchild = new TestView;
  v_grandchild->SetBounds(5, 5, 5, 15);
  v_child->AddChildView(v_grandchild);

  v->Reset();
  v_child->Reset();
  v_grandchild->Reset();

  // Define rects and points within the views in the hierarchy.
  gfx::Rect rect_in_v_grandchild(7, 7, 3, 3);
  gfx::Point point_in_v_grandchild(rect_in_v_grandchild.origin());
  gfx::Rect rect_in_v_child(12, 3, 5, 5);
  gfx::Point point_in_v_child(rect_in_v_child.origin());
  gfx::Rect rect_in_v(50, 50, 25, 30);
  gfx::Point point_in_v(rect_in_v.origin());

  // When all three views return true when GetCanProcessEventsWithinSubtree()
  // is called, targeting should behave as expected.

  View* result_view = root_view->GetEventHandlerForRect(rect_in_v_grandchild);
  EXPECT_EQ(v_grandchild, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_grandchild);
  EXPECT_EQ(v_grandchild, result_view);
  result_view = nullptr;

  result_view = root_view->GetEventHandlerForRect(rect_in_v_child);
  EXPECT_EQ(v_child, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_child);
  EXPECT_EQ(v_child, result_view);
  result_view = nullptr;

  result_view = root_view->GetEventHandlerForRect(rect_in_v);
  EXPECT_EQ(v, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v);
  EXPECT_EQ(v, result_view);
  result_view = nullptr;

  // When |v_grandchild| returns false when GetCanProcessEventsWithinSubtree()
  // is called, then |v_grandchild| cannot be returned as a target.

  v_grandchild->SetCanProcessEventsWithinSubtree(false);

  result_view = root_view->GetEventHandlerForRect(rect_in_v_grandchild);
  EXPECT_EQ(v_child, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_grandchild);
  EXPECT_EQ(v_child, result_view);
  result_view = nullptr;

  result_view = root_view->GetEventHandlerForRect(rect_in_v_child);
  EXPECT_EQ(v_child, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_child);
  EXPECT_EQ(v_child, result_view);
  result_view = nullptr;

  result_view = root_view->GetEventHandlerForRect(rect_in_v);
  EXPECT_EQ(v, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v);
  EXPECT_EQ(v, result_view);

  // When |v_grandchild| returns false when GetCanProcessEventsWithinSubtree()
  // is called, then NULL should be returned as a target if we call
  // GetTooltipHandlerForPoint() with |v_grandchild| as the root of the
  // views tree. Note that the location must be in the coordinate space
  // of the root view (|v_grandchild| in this case), so use (1, 1).

  result_view = v_grandchild;
  result_view = v_grandchild->GetTooltipHandlerForPoint(gfx::Point(1, 1));
  EXPECT_EQ(nullptr, result_view);
  result_view = nullptr;

  // When |v_child| returns false when GetCanProcessEventsWithinSubtree()
  // is called, then neither |v_child| nor |v_grandchild| can be returned
  // as a target (|v| should be returned as the target for each case).

  v_grandchild->Reset();
  v_child->SetCanProcessEventsWithinSubtree(false);

  result_view = root_view->GetEventHandlerForRect(rect_in_v_grandchild);
  EXPECT_EQ(v, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_grandchild);
  EXPECT_EQ(v, result_view);
  result_view = nullptr;

  result_view = root_view->GetEventHandlerForRect(rect_in_v_child);
  EXPECT_EQ(v, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_child);
  EXPECT_EQ(v, result_view);
  result_view = nullptr;

  result_view = root_view->GetEventHandlerForRect(rect_in_v);
  EXPECT_EQ(v, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v);
  EXPECT_EQ(v, result_view);
  result_view = nullptr;

  // When |v| returns false when GetCanProcessEventsWithinSubtree()
  // is called, then none of |v|, |v_child|, and |v_grandchild| can be returned
  // as a target (|root_view| should be returned as the target for each case).

  v_child->Reset();
  v->SetCanProcessEventsWithinSubtree(false);

  result_view = root_view->GetEventHandlerForRect(rect_in_v_grandchild);
  EXPECT_EQ(root_view, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_grandchild);
  EXPECT_EQ(root_view, result_view);
  result_view = nullptr;

  result_view = root_view->GetEventHandlerForRect(rect_in_v_child);
  EXPECT_EQ(root_view, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_child);
  EXPECT_EQ(root_view, result_view);
  result_view = nullptr;

  result_view = root_view->GetEventHandlerForRect(rect_in_v);
  EXPECT_EQ(root_view, result_view);
  result_view = nullptr;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v);
  EXPECT_EQ(root_view, result_view);

  widget->CloseNow();
}

TEST_F(ViewTest, NotifyEnterExitOnChild) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(params));
  View* root_view = widget->GetRootView();
  root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));

  // Have this hierarchy of views (the coords here are in root coord):
  // v1 (0, 0, 100, 100)
  //  - v11 (0, 0, 20, 30)
  //    - v111 (5, 5, 5, 15)
  //  - v12 (50, 10, 30, 90)
  //    - v121 (60, 20, 10, 10)
  // v2 (105, 0, 100, 100)
  //  - v21 (120, 10, 50, 20)

  TestView* v1 = new TestView;
  v1->SetBounds(0, 0, 100, 100);
  root_view->AddChildView(v1);
  v1->SetNotifyEnterExitOnChild(true);

  TestView* v11 = new TestView;
  v11->SetBounds(0, 0, 20, 30);
  v1->AddChildView(v11);

  TestView* v111 = new TestView;
  v111->SetBounds(5, 5, 5, 15);
  v11->AddChildView(v111);

  TestView* v12 = new TestView;
  v12->SetBounds(50, 10, 30, 90);
  v1->AddChildView(v12);

  TestView* v121 = new TestView;
  v121->SetBounds(10, 10, 10, 10);
  v12->AddChildView(v121);

  TestView* v2 = new TestView;
  v2->SetBounds(105, 0, 100, 100);
  root_view->AddChildView(v2);

  TestView* v21 = new TestView;
  v21->SetBounds(15, 10, 50, 20);
  v2->AddChildView(v21);

  v1->Reset();
  v11->Reset();
  v111->Reset();
  v12->Reset();
  v121->Reset();
  v2->Reset();
  v21->Reset();

  // Move the mouse in v111.
  gfx::Point p1(6, 6);
  ui::MouseEvent move1(ui::EventType::kMouseMoved, p1, p1,
                       ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(move1);
  EXPECT_TRUE(v111->received_mouse_enter_);
  EXPECT_EQ(v11->last_mouse_event_type_, ui::EventType::kUnknown);
  EXPECT_TRUE(v1->received_mouse_enter_);

  v111->Reset();
  v1->Reset();

  // Now, move into v121.
  gfx::Point p2(65, 21);
  ui::MouseEvent move2(ui::EventType::kMouseMoved, p2, p2,
                       ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(move2);
  EXPECT_TRUE(v111->received_mouse_exit_);
  EXPECT_TRUE(v121->received_mouse_enter_);
  EXPECT_EQ(v1->last_mouse_event_type_, ui::EventType::kUnknown);

  v111->Reset();
  v121->Reset();

  // Now, move into v11.
  gfx::Point p3(1, 1);
  ui::MouseEvent move3(ui::EventType::kMouseMoved, p3, p3,
                       ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(move3);
  EXPECT_TRUE(v121->received_mouse_exit_);
  EXPECT_TRUE(v11->received_mouse_enter_);
  EXPECT_EQ(v1->last_mouse_event_type_, ui::EventType::kUnknown);

  v121->Reset();
  v11->Reset();

  // Move to v21.
  gfx::Point p4(121, 15);
  ui::MouseEvent move4(ui::EventType::kMouseMoved, p4, p4,
                       ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(move4);
  EXPECT_TRUE(v21->received_mouse_enter_);
  EXPECT_EQ(v2->last_mouse_event_type_, ui::EventType::kUnknown);
  EXPECT_TRUE(v11->received_mouse_exit_);
  EXPECT_TRUE(v1->received_mouse_exit_);

  v21->Reset();
  v11->Reset();
  v1->Reset();

  // Move to v1.
  gfx::Point p5(21, 0);
  ui::MouseEvent move5(ui::EventType::kMouseMoved, p5, p5,
                       ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(move5);
  EXPECT_TRUE(v21->received_mouse_exit_);
  EXPECT_TRUE(v1->received_mouse_enter_);

  v21->Reset();
  v1->Reset();

  // Now, move into v11.
  gfx::Point p6(15, 15);
  ui::MouseEvent mouse6(ui::EventType::kMouseMoved, p6, p6,
                        ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(mouse6);
  EXPECT_TRUE(v11->received_mouse_enter_);
  EXPECT_EQ(v1->last_mouse_event_type_, ui::EventType::kUnknown);

  v11->Reset();
  v1->Reset();

  // Move back into v1. Although |v1| had already received an ENTER for mouse6,
  // and the mouse remains inside |v1| the whole time, it receives another ENTER
  // when the mouse leaves v11.
  gfx::Point p7(21, 0);
  ui::MouseEvent mouse7(ui::EventType::kMouseMoved, p7, p7,
                        ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(mouse7);
  EXPECT_TRUE(v11->received_mouse_exit_);
  EXPECT_FALSE(v1->received_mouse_enter_);

  widget->CloseNow();
}

TEST_F(ViewTest, Textfield) {
  const std::u16string kText =
      u"Reality is that which, when you stop believing it, doesn't go away.";
  const std::u16string kExtraText = u"Pretty deep, Philip!";

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(0, 0, 100, 100);
  widget->Init(std::move(params));
  View* root_view = widget->GetRootView();

  Textfield* textfield = new Textfield();
  root_view->AddChildView(textfield);

  // Test setting, appending text.
  textfield->SetText(kText);
  EXPECT_EQ(kText, textfield->GetText());
  textfield->AppendText(kExtraText);
  EXPECT_EQ(kText + kExtraText, textfield->GetText());
  textfield->SetText(std::u16string());
  EXPECT_TRUE(textfield->GetText().empty());

  // Test selection related methods.
  textfield->SetText(kText);
  EXPECT_TRUE(textfield->GetSelectedText().empty());
  textfield->SelectAll(false);
  EXPECT_EQ(kText, textfield->GetText());
  textfield->ClearSelection();
  EXPECT_TRUE(textfield->GetSelectedText().empty());

  widget->CloseNow();
}

// Tests that the Textfield view respond appropiately to cut/copy/paste.
TEST_F(ViewTest, TextfieldCutCopyPaste) {
  const std::u16string kNormalText = u"Normal";
  const std::u16string kReadOnlyText = u"Read only";
  const std::u16string kPasswordText = u"Password! ** Secret stuff **";

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(0, 0, 100, 100);
  widget->Init(std::move(params));
  View* root_view = widget->GetRootView();

  Textfield* normal = new Textfield();
  Textfield* read_only = new Textfield();
  read_only->SetReadOnly(true);
  Textfield* password = new Textfield();
  password->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  root_view->AddChildView(normal);
  root_view->AddChildView(read_only);
  root_view->AddChildView(password);

  normal->SetText(kNormalText);
  read_only->SetText(kReadOnlyText);
  password->SetText(kPasswordText);

  //
  // Test cut.
  //

  normal->SelectAll(false);
  normal->ExecuteCommand(Textfield::kCut, 0);
  std::u16string result;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &result);
  EXPECT_EQ(kNormalText, result);
  normal->SetText(kNormalText);  // Let's revert to the original content.

  read_only->SelectAll(false);
  read_only->ExecuteCommand(Textfield::kCut, 0);
  result.clear();
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &result);
  // Cut should have failed, so the clipboard content should not have changed.
  EXPECT_EQ(kNormalText, result);

  password->SelectAll(false);
  password->ExecuteCommand(Textfield::kCut, 0);
  result.clear();
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &result);
  // Cut should have failed, so the clipboard content should not have changed.
  EXPECT_EQ(kNormalText, result);

  //
  // Test copy.
  //

  // Start with |read_only| to observe a change in clipboard text.
  read_only->SelectAll(false);
  read_only->ExecuteCommand(Textfield::kCopy, 0);
  result.clear();
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &result);
  EXPECT_EQ(kReadOnlyText, result);

  normal->SelectAll(false);
  normal->ExecuteCommand(Textfield::kCopy, 0);
  result.clear();
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &result);
  EXPECT_EQ(kNormalText, result);

  password->SelectAll(false);
  password->ExecuteCommand(Textfield::kCopy, 0);
  result.clear();
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &result);
  // Text cannot be copied from an obscured field; the clipboard won't change.
  EXPECT_EQ(kNormalText, result);

  //
  // Test paste.
  //

  // Attempting to paste kNormalText in a read-only text-field should fail.
  read_only->SelectAll(false);
  read_only->ExecuteCommand(Textfield::kPaste, 0);
  EXPECT_EQ(kReadOnlyText, read_only->GetText());

  password->SelectAll(false);
  password->ExecuteCommand(Textfield::kPaste, 0);
  EXPECT_EQ(kNormalText, password->GetText());

  // Copy from |read_only| to observe a change in the normal textfield text.
  read_only->SelectAll(false);
  read_only->ExecuteCommand(Textfield::kCopy, 0);
  normal->SelectAll(false);
  normal->ExecuteCommand(Textfield::kPaste, 0);
  EXPECT_EQ(kReadOnlyText, normal->GetText());
  widget->CloseNow();
}

class ViewPaintOptimizationTest : public ViewsTestBase {
 public:
  ViewPaintOptimizationTest() = default;

  ViewPaintOptimizationTest(const ViewPaintOptimizationTest&) = delete;
  ViewPaintOptimizationTest& operator=(const ViewPaintOptimizationTest&) =
      delete;

  ~ViewPaintOptimizationTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        views::features::kEnableViewPaintOptimization);
    ViewTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that only Views where SchedulePaint was invoked get repainted.
TEST_F(ViewPaintOptimizationTest, PaintDirtyViewsOnly) {
  ScopedTestPaintWidget widget(CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  TestView* v1 = root_view->AddChildView(std::make_unique<TestView>());
  v1->SetBounds(10, 11, 12, 13);

  TestView* v2 = root_view->AddChildView(std::make_unique<TestView>());
  v2->SetBounds(3, 4, 6, 5);

  TestView* v21 = v2->AddChildView(std::make_unique<TestView>());
  v21->SetBounds(2, 3, 4, 5);

  // Paint everything once, since it has to build its cache. Then we can test
  // invalidation.
  gfx::Rect first_paint(1, 1);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, first_paint, false),
      root_view->size()));
  v1->Reset();
  v2->Reset();
  v21->Reset();

  gfx::Rect paint_area(10, 11, 12, 13);
  list = base::MakeRefCounted<cc::DisplayItemList>();

  // Schedule a paint on v2 which marks it invalidated.
  v2->SchedulePaint();
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_FALSE(v2->did_paint_);
  EXPECT_FALSE(v21->did_paint_);

  // Paint with an unknown invalidation. The invalidation is irrelevant since
  // repainting a view only depends on whether the view had a scheduled paint.
  gfx::Rect empty_rect;
  EXPECT_TRUE(empty_rect.IsEmpty());

  root_view->Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, paint_area, false), empty_rect.size()));

  // Only v2 should be repainted.
  EXPECT_FALSE(v1->did_paint_);
  EXPECT_TRUE(v2->did_paint_);
  EXPECT_FALSE(v21->did_paint_);
}

////////////////////////////////////////////////////////////////////////////////
// Accelerators
////////////////////////////////////////////////////////////////////////////////

namespace {

// A Widget with a TestView in the view hierarchy. Used for accelerator tests.
class TestViewWidget {
 public:
  TestViewWidget(Widget::InitParams create_params,
                 ui::Accelerator* initial_accelerator,
                 bool show_after_init = true) {
    auto view = std::make_unique<TestView>();
    view->Reset();

    // Register a keyboard accelerator before the view is added to a window.
    if (initial_accelerator) {
      view->AddAccelerator(*initial_accelerator);
      EXPECT_EQ(view->accelerator_count_map_[*initial_accelerator], 0);
    }

    widget_ = std::make_unique<Widget>();
    // Create a window and add the view as its child.
    Widget::InitParams params = std::move(create_params);
    params.bounds = gfx::Rect(0, 0, 100, 100);
    widget_->Init(std::move(params));
    View* root = widget_->GetRootView();
    view_ = root->AddChildView(std::move(view));
    if (show_after_init)
      widget_->Show();

    EXPECT_TRUE(widget_->GetFocusManager());
  }

  TestViewWidget(const TestViewWidget&) = delete;
  TestViewWidget& operator=(const TestViewWidget&) = delete;

  TestView* view() { return view_; }
  Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<Widget> widget_;
  raw_ptr<TestView> view_;
};

}  // namespace

// On non-ChromeOS aura there is extra logic to determine whether a view should
// handle accelerators or not (see View::CanHandleAccelerators for details).
// This test targets that extra logic, but should also work on other platforms.
TEST_F(ViewTest, HandleAccelerator) {
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  TestViewWidget test_widget(
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP),
      &return_accelerator);
  TestView* view = test_widget.view();
  Widget* widget = test_widget.widget();
  FocusManager* focus_manager = widget->GetFocusManager();

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
  // When a non-child view is not active, it shouldn't handle accelerators.
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(0, view->accelerator_count_map_[return_accelerator]);
#endif

  // TYPE_POPUP widgets default to non-activatable, so the Show() above wouldn't
  // have activated the Widget. First, allow activation.
  widget->widget_delegate()->SetCanActivate(true);

  // When a non-child view is active, it should handle accelerators.
  view->accelerator_count_map_[return_accelerator] = 0;
  widget->Activate();
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(1, view->accelerator_count_map_[return_accelerator]);

  // Add a child view associated with a child widget.
  auto child_widget = std::make_unique<Widget>();
  Widget::InitParams child_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_CONTROL);
  child_params.parent = widget->GetNativeView();
  child_widget->Init(std::move(child_params));
  TestView* child_view =
      child_widget->SetContentsView(std::make_unique<TestView>());
  child_view->Reset();
  child_view->AddAccelerator(return_accelerator);
  EXPECT_EQ(child_view->accelerator_count_map_[return_accelerator], 0);

  FocusManager* child_focus_manager = child_widget->GetFocusManager();
  ASSERT_TRUE(child_focus_manager);

  // When a child view is in focus, it should handle accelerators.
  child_view->accelerator_count_map_[return_accelerator] = 0;
  view->accelerator_count_map_[return_accelerator] = 0;
  child_focus_manager->SetFocusedView(child_view);
  EXPECT_FALSE(child_view->GetWidget()->IsActive());
  EXPECT_TRUE(child_focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(1, child_view->accelerator_count_map_[return_accelerator]);
  EXPECT_EQ(0, view->accelerator_count_map_[return_accelerator]);

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
  // When a child view is not in focus, its parent should handle accelerators.
  child_view->accelerator_count_map_[return_accelerator] = 0;
  view->accelerator_count_map_[return_accelerator] = 0;
  child_focus_manager->ClearFocus();
  EXPECT_FALSE(child_view->GetWidget()->IsActive());
  EXPECT_TRUE(child_focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(0, child_view->accelerator_count_map_[return_accelerator]);
  EXPECT_EQ(1, view->accelerator_count_map_[return_accelerator]);
#endif
}

// TODO(themblsha): Bring this up on non-Mac platforms. It currently fails
// because TestView::AcceleratorPressed() is not called. See
// http://crbug.com/667757.
#if BUILDFLAG(IS_MAC)
// Test that BridgedContentView correctly handles Accelerator key events when
// subject to OS event dispatch.
TEST_F(ViewTest, ActivateAcceleratorOnMac) {
  // Cmd+1 translates to "noop:" command by interpretKeyEvents.
  ui::Accelerator command_accelerator(ui::VKEY_1, ui::EF_COMMAND_DOWN);
  TestViewWidget test_widget(
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP),
      &command_accelerator);
  TestView* view = test_widget.view();

  ui::test::EventGenerator event_generator(
      test_widget.widget()->GetNativeWindow());
  // Emulate normal event dispatch through -[NSWindow sendEvent:].
  event_generator.set_target(ui::test::EventGenerator::Target::WINDOW);

  event_generator.PressKey(command_accelerator.key_code(),
                           command_accelerator.modifiers());
  event_generator.ReleaseKey(command_accelerator.key_code(),
                             command_accelerator.modifiers());
  EXPECT_EQ(view->accelerator_count_map_[command_accelerator], 1);

  // Without an _wantsKeyDownForEvent: override we'll only get a keyUp: event
  // for this accelerator.
  ui::Accelerator key_up_accelerator(ui::VKEY_TAB,
                                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  view->AddAccelerator(key_up_accelerator);
  event_generator.PressKey(key_up_accelerator.key_code(),
                           key_up_accelerator.modifiers());
  event_generator.ReleaseKey(key_up_accelerator.key_code(),
                             key_up_accelerator.modifiers());
  EXPECT_EQ(view->accelerator_count_map_[key_up_accelerator], 1);

  // We should handle this accelerator inside keyDown: as it doesn't translate
  // to any command by default.
  ui::Accelerator key_down_accelerator(
      ui::VKEY_L, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  view->AddAccelerator(key_down_accelerator);
  event_generator.PressKey(key_down_accelerator.key_code(),
                           key_down_accelerator.modifiers());
  event_generator.ReleaseKey(key_down_accelerator.key_code(),
                             key_down_accelerator.modifiers());
  EXPECT_EQ(view->accelerator_count_map_[key_down_accelerator], 1);
}
#endif  // BUILDFLAG(IS_MAC)

// TODO(crbug.com/41287573): these tests were initially commented out when
// getting aura to run. Figure out if still valuable and either nuke or fix.
#if BUILDFLAG(IS_MAC)
TEST_F(ViewTest, ActivateAccelerator) {
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  TestViewWidget test_widget(
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP),
      &return_accelerator);
  TestView* view = test_widget.view();
  FocusManager* focus_manager = test_widget.widget()->GetFocusManager();

  // Hit the return key and see if it takes effect.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 1);

  // Hit the escape key. Nothing should happen.
  ui::Accelerator escape_accelerator(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 1);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 0);

  // Now register the escape key and hit it again.
  view->AddAccelerator(escape_accelerator);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 1);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 1);

  // Remove the return key accelerator.
  view->RemoveAccelerator(return_accelerator);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 1);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 1);

  // Add it again. Hit the return key and the escape key.
  view->AddAccelerator(return_accelerator);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 2);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 1);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 2);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 2);

  // Remove all the accelerators.
  view->ResetAccelerators();
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 2);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 2);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 2);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 2);
}

TEST_F(ViewTest, HiddenViewWithAccelerator) {
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  TestViewWidget test_widget(
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP),
      &return_accelerator);
  TestView* view = test_widget.view();
  FocusManager* focus_manager = test_widget.widget()->GetFocusManager();

  view->SetVisible(false);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));

  view->SetVisible(true);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
}

TEST_F(ViewTest, ViewInHiddenWidgetWithAccelerator) {
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  TestViewWidget test_widget(
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP),
      &return_accelerator, false);
  TestView* view = test_widget.view();
  Widget* widget = test_widget.widget();
  FocusManager* focus_manager = test_widget.widget()->GetFocusManager();

  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(0, view->accelerator_count_map_[return_accelerator]);

  widget->Show();
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(1, view->accelerator_count_map_[return_accelerator]);

  widget->Hide();
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(1, view->accelerator_count_map_[return_accelerator]);
}
#endif  // BUILDFLAG(IS_MAC)

////////////////////////////////////////////////////////////////////////////////
// Native view hierachy
////////////////////////////////////////////////////////////////////////////////
class ToplevelWidgetObserverView : public View {
  METADATA_HEADER(ToplevelWidgetObserverView, View)

 public:
  ToplevelWidgetObserverView() = default;

  ToplevelWidgetObserverView(const ToplevelWidgetObserverView&) = delete;
  ToplevelWidgetObserverView& operator=(const ToplevelWidgetObserverView&) =
      delete;

  ~ToplevelWidgetObserverView() override = default;

  // View overrides:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    if (details.is_add) {
      toplevel_ = GetWidget() ? GetWidget()->GetTopLevelWidget() : nullptr;
    } else {
      toplevel_ = nullptr;
    }
  }
  void NativeViewHierarchyChanged() override {
    toplevel_ = GetWidget() ? GetWidget()->GetTopLevelWidget() : nullptr;
  }

  Widget* toplevel() { return toplevel_; }

 private:
  raw_ptr<Widget> toplevel_ = nullptr;
};

BEGIN_METADATA(ToplevelWidgetObserverView)
END_METADATA

// Test that
// a) a view can track the current top level widget by overriding
//    View::ViewHierarchyChanged() and View::NativeViewHierarchyChanged().
// b) a widget has the correct parent after reparenting.
TEST_F(ViewTest, NativeViewHierarchyChanged) {
  auto toplevel1 = std::make_unique<Widget>();
  Widget::InitParams toplevel1_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  toplevel1->Init(std::move(toplevel1_params));

  auto toplevel2 = std::make_unique<Widget>();
  Widget::InitParams toplevel2_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  toplevel2->Init(std::move(toplevel2_params));

  auto child = std::make_unique<Widget>();
  Widget::InitParams child_params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_CONTROL);
  child_params.parent = toplevel1->GetNativeView();
  child->Init(std::move(child_params));
  EXPECT_EQ(toplevel1.get(), child->parent());

  auto owning_observer_view = std::make_unique<ToplevelWidgetObserverView>();
  EXPECT_EQ(nullptr, owning_observer_view->toplevel());

  ToplevelWidgetObserverView* observer_view =
      child->SetContentsView(std::move(owning_observer_view));
  EXPECT_EQ(toplevel1.get(), observer_view->toplevel());

  Widget::ReparentNativeView(child->GetNativeView(),
                             toplevel2->GetNativeView());
  EXPECT_EQ(toplevel2.get(), observer_view->toplevel());
  EXPECT_EQ(toplevel2.get(), child->parent());

  owning_observer_view =
      observer_view->parent()->RemoveChildViewT(observer_view);
  EXPECT_EQ(nullptr, observer_view->toplevel());

  // Make |observer_view| |child|'s contents view again so that it gets deleted
  // with the widget.
  child->SetContentsView(std::move(owning_observer_view));
}

////////////////////////////////////////////////////////////////////////////////
// Transformations
////////////////////////////////////////////////////////////////////////////////

class TransformPaintView : public TestView {
  METADATA_HEADER(TransformPaintView, TestView)

 public:
  TransformPaintView() = default;

  TransformPaintView(const TransformPaintView&) = delete;
  TransformPaintView& operator=(const TransformPaintView&) = delete;

  ~TransformPaintView() override = default;

  void ClearScheduledPaintRect() { scheduled_paint_rect_ = gfx::Rect(); }

  gfx::Rect scheduled_paint_rect() const { return scheduled_paint_rect_; }

  // Overridden from View:
  void OnDidSchedulePaint(const gfx::Rect& rect) override {
    gfx::Rect xrect = ConvertRectToParent(rect);
    scheduled_paint_rect_.Union(xrect);
  }

 private:
  gfx::Rect scheduled_paint_rect_;
};

BEGIN_METADATA(TransformPaintView)
END_METADATA

TEST_F(ViewTest, TransformPaint) {
  auto view1 = std::make_unique<TransformPaintView>();
  view1->SetBoundsRect(gfx::Rect(0, 0, 500, 300));

  auto view2 = std::make_unique<TestView>();
  view2->SetBoundsRect(gfx::Rect(100, 100, 200, 100));

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  widget->Show();
  View* root = widget->GetRootView();

  TransformPaintView* v1 = root->AddChildView(std::move(view1));
  TestView* v2 = v1->AddChildView(std::move(view2));

  // At this moment, |v2| occupies (100, 100) to (300, 200) in |root|.
  v1->ClearScheduledPaintRect();
  v2->SchedulePaint();

  EXPECT_EQ(gfx::Rect(100, 100, 200, 100), v1->scheduled_paint_rect());

  // Rotate |v1| counter-clockwise.
  gfx::Transform transform = RotationCounterclockwise();
  transform.set_rc(1, 3, 500.0);
  v1->SetTransform(transform);

  // |v2| now occupies (100, 200) to (200, 400) in |root|.

  v1->ClearScheduledPaintRect();
  v2->SchedulePaint();

  EXPECT_EQ(gfx::Rect(100, 200, 100, 200), v1->scheduled_paint_rect());
}

TEST_F(ViewTest, TransformEvent) {
  auto view1 = std::make_unique<TestView>();
  view1->SetBoundsRect(gfx::Rect(0, 0, 500, 300));

  auto view2 = std::make_unique<TestView>();
  view2->SetBoundsRect(gfx::Rect(100, 100, 200, 100));

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();

  TestView* v1 = root->AddChildView(std::move(view1));
  TestView* v2 = v1->AddChildView(std::move(view2));

  // At this moment, |v2| occupies (100, 100) to (300, 200) in |root|.

  // Rotate |v1| counter-clockwise.
  gfx::Transform transform = RotationCounterclockwise();
  transform.set_rc(1, 3, 500.0);
  v1->SetTransform(transform);

  // |v2| now occupies (100, 200) to (200, 400) in |root|.
  v1->Reset();
  v2->Reset();

  gfx::Point p1(110, 210);
  ui::MouseEvent pressed(ui::EventType::kMousePressed, p1, p1,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(pressed);
  EXPECT_EQ(v1->last_mouse_event_type_, ui::EventType::kUnknown);
  EXPECT_EQ(ui::EventType::kMousePressed, v2->last_mouse_event_type_);
  EXPECT_EQ(190, v2->location_.x());
  EXPECT_EQ(10, v2->location_.y());

  ui::MouseEvent released(ui::EventType::kMouseReleased, gfx::Point(),
                          gfx::Point(), ui::EventTimeForNow(), 0, 0);
  root->OnMouseReleased(released);

  // Now rotate |v2| inside |v1| clockwise.
  transform = RotationClockwise();
  transform.set_rc(0, 3, 100.f);
  v2->SetTransform(transform);

  // Now, |v2| occupies (100, 100) to (200, 300) in |v1|, and (100, 300) to
  // (300, 400) in |root|.

  v1->Reset();
  v2->Reset();

  gfx::Point point2(110, 320);
  ui::MouseEvent p2(ui::EventType::kMousePressed, point2, point2,
                    ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                    ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(p2);
  EXPECT_EQ(v1->last_mouse_event_type_, ui::EventType::kUnknown);
  EXPECT_EQ(ui::EventType::kMousePressed, v2->last_mouse_event_type_);
  EXPECT_EQ(10, v2->location_.x());
  EXPECT_EQ(20, v2->location_.y());

  root->OnMouseReleased(released);

  v1->SetTransform(gfx::Transform());
  v2->SetTransform(gfx::Transform());

  auto view3 = std::make_unique<TestView>();
  view3->SetBoundsRect(gfx::Rect(10, 10, 20, 30));
  TestView* v3 = v2->AddChildView(std::move(view3));

  // Rotate |v3| clockwise with respect to |v2|.
  transform = RotationClockwise();
  transform.set_rc(0, 3, 30.f);
  v3->SetTransform(transform);

  // Scale |v2| with respect to |v1| along both axis.
  transform = v2->GetTransform();
  transform.set_rc(0, 0, 0.8f);
  transform.set_rc(1, 1, 0.5f);
  v2->SetTransform(transform);

  // |v3| occupies (108, 105) to (132, 115) in |root|.

  v1->Reset();
  v2->Reset();
  v3->Reset();

  gfx::Point point(112, 110);
  ui::MouseEvent p3(ui::EventType::kMousePressed, point, point,
                    ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                    ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(p3);

  EXPECT_EQ(ui::EventType::kMousePressed, v3->last_mouse_event_type_);
  EXPECT_EQ(10, v3->location_.x());
  EXPECT_EQ(25, v3->location_.y());

  root->OnMouseReleased(released);

  v1->SetTransform(gfx::Transform());
  v2->SetTransform(gfx::Transform());
  v3->SetTransform(gfx::Transform());

  v1->Reset();
  v2->Reset();
  v3->Reset();

  // Rotate |v3| clockwise with respect to |v2|, and scale it along both axis.
  transform = RotationClockwise();
  transform.set_rc(0, 3, 30.f);
  // Rotation sets some scaling transformation. Using SetScale would overwrite
  // that and pollute the rotation. So combine the scaling with the existing
  // transforamtion.
  gfx::Transform scale;
  scale.Scale(0.8f, 0.5f);
  transform.PostConcat(scale);
  v3->SetTransform(transform);

  // Translate |v2| with respect to |v1|.
  transform = v2->GetTransform();
  transform.set_rc(0, 3, 10.f);
  transform.set_rc(1, 3, 10.f);
  v2->SetTransform(transform);

  // |v3| now occupies (120, 120) to (144, 130) in |root|.

  gfx::Point point3(124, 125);
  ui::MouseEvent p4(ui::EventType::kMousePressed, point3, point3,
                    ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                    ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(p4);

  EXPECT_EQ(ui::EventType::kMousePressed, v3->last_mouse_event_type_);
  EXPECT_EQ(10, v3->location_.x());
  EXPECT_EQ(25, v3->location_.y());

  root->OnMouseReleased(released);
}

TEST_F(ViewTest, TransformVisibleBound) {
  gfx::Rect viewport_bounds(0, 0, 100, 100);

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = viewport_bounds;
  widget->Init(std::move(params));
  widget->GetRootView()->SetBoundsRect(viewport_bounds);

  View* viewport = widget->SetContentsView(std::make_unique<View>());
  View* contents = viewport->AddChildView(std::make_unique<View>());
  viewport->SetBoundsRect(viewport_bounds);
  contents->SetBoundsRect(gfx::Rect(0, 0, 100, 200));

  View* child = contents->AddChildView(std::make_unique<View>());
  child->SetBoundsRect(gfx::Rect(10, 90, 50, 50));
  EXPECT_EQ(gfx::Rect(0, 0, 50, 10), child->GetVisibleBounds());

  // Rotate |child| counter-clockwise
  gfx::Transform transform = RotationCounterclockwise();
  transform.set_rc(1, 3, 50.f);
  child->SetTransform(transform);
  EXPECT_EQ(gfx::Rect(40, 0, 10, 50), child->GetVisibleBounds());
}

TEST_F(ViewTest, WidgetObserverViewWidgetClosedViewReparented) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));
  EXPECT_TRUE(widget->GetRootView());

  View* contents_view =
      widget->GetRootView()->AddChildView(std::make_unique<View>());
  View* view_1 = contents_view->AddChildView(std::make_unique<View>());
  View* child_view_1 = view_1->AddChildView(std::make_unique<View>());

  EXPECT_TRUE(!contents_view->GetViewAccessibility().is_widget_closed_);
  EXPECT_TRUE(!view_1->GetViewAccessibility().is_widget_closed_);
  EXPECT_TRUE(!child_view_1->GetViewAccessibility().is_widget_closed_);

  widget->CloseNow();

  // Add a child view to the view that has a closed widget.
  View* child_view_2 = view_1->AddChildView(std::make_unique<View>());
  EXPECT_TRUE(child_view_2->GetViewAccessibility().is_widget_closed_);

  EXPECT_TRUE(contents_view->GetViewAccessibility().is_widget_closed_);
  EXPECT_TRUE(view_1->GetViewAccessibility().is_widget_closed_);
  EXPECT_TRUE(child_view_1->GetViewAccessibility().is_widget_closed_);

  auto widget_2 = std::make_unique<Widget>();
  params = CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                        Widget::InitParams::TYPE_WINDOW);
  widget_2->Init(std::move(params));
  EXPECT_TRUE(widget_2->GetRootView());

  // Reparent the views tree with the closed widget to a new widget.
  widget_2->GetRootView()->AddChildView(contents_view);
  EXPECT_TRUE(!contents_view->GetViewAccessibility().is_widget_closed_);
  EXPECT_TRUE(!view_1->GetViewAccessibility().is_widget_closed_);
  EXPECT_TRUE(!child_view_1->GetViewAccessibility().is_widget_closed_);
  EXPECT_TRUE(!child_view_2->GetViewAccessibility().is_widget_closed_);
}

////////////////////////////////////////////////////////////////////////////////
// OnVisibleBoundsChanged()

class VisibleBoundsView : public View {
  METADATA_HEADER(VisibleBoundsView, View)

 public:
  VisibleBoundsView() = default;

  VisibleBoundsView(const VisibleBoundsView&) = delete;
  VisibleBoundsView& operator=(const VisibleBoundsView&) = delete;

  ~VisibleBoundsView() override = default;

  bool received_notification() const { return received_notification_; }
  void set_received_notification(bool received) {
    received_notification_ = received;
  }

 private:
  // Overridden from View:
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override {
    return true;
  }
  void OnVisibleBoundsChanged() override { received_notification_ = true; }

  bool received_notification_ = false;
};

BEGIN_METADATA(VisibleBoundsView)
END_METADATA

TEST_F(ViewTest, OnVisibleBoundsChanged) {
  gfx::Rect viewport_bounds(0, 0, 100, 100);

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = viewport_bounds;
  widget->Init(std::move(params));
  widget->GetRootView()->SetBoundsRect(viewport_bounds);

  View* viewport = widget->SetContentsView(std::make_unique<View>());
  View* contents = viewport->AddChildView(std::make_unique<View>());
  viewport->SetBoundsRect(viewport_bounds);
  contents->SetBoundsRect(gfx::Rect(0, 0, 100, 200));

  // Create a view that cares about visible bounds notifications, and position
  // it just outside the visible bounds of the viewport.
  VisibleBoundsView* child =
      contents->AddChildView(std::make_unique<VisibleBoundsView>());
  child->SetBoundsRect(gfx::Rect(10, 110, 50, 50));

  // The child bound should be fully clipped.
  EXPECT_TRUE(child->GetVisibleBounds().IsEmpty());

  // Now scroll the contents, but not enough to make the child visible.
  contents->SetY(contents->y() - 1);

  // We should have received the notification since the visible bounds may have
  // changed (even though they didn't).
  EXPECT_TRUE(child->received_notification());
  EXPECT_TRUE(child->GetVisibleBounds().IsEmpty());
  child->set_received_notification(false);

  // Now scroll the contents, this time by enough to make the child visible by
  // one pixel.
  contents->SetY(contents->y() - 10);
  EXPECT_TRUE(child->received_notification());
  EXPECT_EQ(1, child->GetVisibleBounds().height());
  child->set_received_notification(false);
}

TEST_F(ViewTest, SetBoundsPaint) {
  auto top_view = std::make_unique<TestView>();
  auto child = std::make_unique<TestView>();

  top_view->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  top_view->scheduled_paint_rects_.clear();
  child->SetBoundsRect(gfx::Rect(10, 10, 20, 20));
  TestView* child_view = top_view->AddChildView(std::move(child));

  top_view->scheduled_paint_rects_.clear();
  child_view->SetBoundsRect(gfx::Rect(30, 30, 20, 20));
  EXPECT_EQ(2U, top_view->scheduled_paint_rects_.size());

  // There should be 2 rects, spanning from (10, 10) to (50, 50).
  gfx::Rect paint_rect = top_view->scheduled_paint_rects_[0];
  paint_rect.Union(top_view->scheduled_paint_rects_[1]);
  EXPECT_EQ(gfx::Rect(10, 10, 40, 40), paint_rect);
}

// Assertions around painting and focus gain/lost.
TEST_F(ViewTest, FocusBlurPaints) {
  auto parent_view = std::make_unique<TestView>();
  auto child = std::make_unique<TestView>();  // Owned by |parent_view|.

  parent_view->SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  child->SetBoundsRect(gfx::Rect(0, 0, 20, 20));
  TestView* child_view = parent_view->AddChildView(std::move(child));

  parent_view->scheduled_paint_rects_.clear();
  child_view->scheduled_paint_rects_.clear();

  // Focus change shouldn't trigger paints.
  child_view->DoFocus();

  EXPECT_TRUE(parent_view->scheduled_paint_rects_.empty());
  EXPECT_TRUE(child_view->scheduled_paint_rects_.empty());

  child_view->DoBlur();
  EXPECT_TRUE(parent_view->scheduled_paint_rects_.empty());
  EXPECT_TRUE(child_view->scheduled_paint_rects_.empty());
}

// Verifies SetBounds(same bounds) doesn't trigger a SchedulePaint().
TEST_F(ViewTest, SetBoundsSameBoundsDoesntSchedulePaint) {
  auto view = std::make_unique<TestView>();

  view->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  view->InvalidateLayout();
  view->scheduled_paint_rects_.clear();
  view->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  EXPECT_TRUE(view->scheduled_paint_rects_.empty());
}

// Verifies AddChildView() and RemoveChildView() schedule appropriate paints.
TEST_F(ViewTest, AddAndRemoveSchedulePaints) {
  gfx::Rect viewport_bounds(0, 0, 100, 100);

  // We have to put the View hierarchy into a Widget or no paints will be
  // scheduled.
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = viewport_bounds;
  widget->Init(std::move(params));
  widget->GetRootView()->SetBoundsRect(viewport_bounds);

  TestView* parent_view = widget->SetContentsView(std::make_unique<TestView>());
  parent_view->SetBoundsRect(viewport_bounds);
  parent_view->scheduled_paint_rects_.clear();

  auto owning_child_view = std::make_unique<View>();
  owning_child_view->SetBoundsRect(gfx::Rect(0, 0, 20, 20));
  View* child_view = parent_view->AddChildView(std::move(owning_child_view));
  ASSERT_EQ(1U, parent_view->scheduled_paint_rects_.size());
  EXPECT_EQ(child_view->bounds(), parent_view->scheduled_paint_rects_.front());

  parent_view->scheduled_paint_rects_.clear();
  parent_view->RemoveChildView(child_view);
  std::unique_ptr<View> child_deleter(child_view);
  ASSERT_EQ(1U, parent_view->scheduled_paint_rects_.size());
  EXPECT_EQ(child_view->bounds(), parent_view->scheduled_paint_rects_.front());
}

// Tests conversion methods with a transform.
TEST_F(ViewTest, ConversionsWithTransform) {
  TestView top_view;

  // View hierarchy used to test scale transforms.
  TestView* child = new TestView;
  TestView* child_child = new TestView;

  // View used to test a rotation transform.
  TestView* child_2 = new TestView;

  constexpr float kDefaultAllowedConversionError = 0.00001f;

  {
    top_view.AddChildView(child);
    child->AddChildView(child_child);

    top_view.SetBoundsRect(gfx::Rect(0, 0, 1000, 1000));

    child->SetBoundsRect(gfx::Rect(7, 19, 500, 500));
    gfx::Transform transform;
    transform.Scale(3.0, 4.0);
    child->SetTransform(transform);

    child_child->SetBoundsRect(gfx::Rect(17, 13, 100, 100));
    transform.MakeIdentity();
    transform.Scale(5.0, 7.0);
    child_child->SetTransform(transform);

    top_view.AddChildView(child_2);
    child_2->SetBoundsRect(gfx::Rect(700, 725, 100, 100));
    transform = RotationClockwise();
    child_2->SetTransform(transform);
  }

  // Sanity check to make sure basic transforms act as expected.
  {
    gfx::Transform transform;
    transform.Translate(110.0, -110.0);
    transform.Scale(100.0, 55.0);
    transform.Translate(1.0, 1.0);

    // convert to a 3x3 matrix.
    SkMatrix matrix = gfx::TransformToFlattenedSkMatrix(transform);

    EXPECT_EQ(210, matrix.getTranslateX());
    EXPECT_EQ(-55, matrix.getTranslateY());
    EXPECT_EQ(100, matrix.getScaleX());
    EXPECT_EQ(55, matrix.getScaleY());
    EXPECT_EQ(0, matrix.getSkewX());
    EXPECT_EQ(0, matrix.getSkewY());
  }

  {
    gfx::Transform transform;
    transform.Translate(1.0, 1.0);
    gfx::Transform t2;
    t2.Scale(100.0, 55.0);
    gfx::Transform t3;
    t3.Translate(110.0, -110.0);
    transform.PostConcat(t2);
    transform.PostConcat(t3);

    // convert to a 3x3 matrix
    SkMatrix matrix = gfx::TransformToFlattenedSkMatrix(transform);

    EXPECT_EQ(210, matrix.getTranslateX());
    EXPECT_EQ(-55, matrix.getTranslateY());
    EXPECT_EQ(100, matrix.getScaleX());
    EXPECT_EQ(55, matrix.getScaleY());
    EXPECT_EQ(0, matrix.getSkewX());
    EXPECT_EQ(0, matrix.getSkewY());
  }

  // Conversions from child->top and top->child.
  {
    // child->top
    gfx::Point point(5, 5);
    View::ConvertPointToTarget(child, &top_view, &point);
    EXPECT_EQ(22, point.x());
    EXPECT_EQ(39, point.y());

    gfx::Rect kSrc(5, 5, 10, 20);
    gfx::RectF kSrcF(kSrc);
    gfx::Rect kExpected(22, 39, 30, 80);

    gfx::Rect kActual = View::ConvertRectToTarget(child, &top_view, kSrc);
    EXPECT_EQ(kActual, kExpected);

    gfx::RectF kActualF = View::ConvertRectToTarget(child, &top_view, kSrcF);
    EXPECT_TRUE(kActualF.ApproximatelyEqual(gfx::RectF(kExpected),
                                            kDefaultAllowedConversionError,
                                            kDefaultAllowedConversionError));

    View::ConvertRectToTarget(child, &top_view, &kSrcF);
    EXPECT_TRUE(kSrcF.ApproximatelyEqual(gfx::RectF(kExpected),
                                         kDefaultAllowedConversionError,
                                         kDefaultAllowedConversionError));

    // top->child
    point.SetPoint(22, 39);
    View::ConvertPointToTarget(&top_view, child, &point);
    EXPECT_EQ(5, point.x());
    EXPECT_EQ(5, point.y());

    kSrc.SetRect(22, 39, 30, 80);
    kSrcF = gfx::RectF(kSrc);
    kExpected.SetRect(5, 5, 10, 20);

    kActual = View::ConvertRectToTarget(&top_view, child, kSrc);
    EXPECT_EQ(kActual, kExpected);

    kActualF = View::ConvertRectToTarget(&top_view, child, kSrcF);
    EXPECT_TRUE(kActualF.ApproximatelyEqual(gfx::RectF(kExpected),
                                            kDefaultAllowedConversionError,
                                            kDefaultAllowedConversionError));

    View::ConvertRectToTarget(&top_view, child, &kSrcF);
    EXPECT_TRUE(kSrcF.ApproximatelyEqual(gfx::RectF(kExpected),
                                         kDefaultAllowedConversionError,
                                         kDefaultAllowedConversionError));
  }

  // Conversions from child_child->top and top->child_child.
  {
    // child_child->top
    gfx::Point point(5, 5);
    View::ConvertPointToTarget(child_child, &top_view, &point);
    EXPECT_EQ(133, point.x());
    EXPECT_EQ(211, point.y());

    gfx::Rect kSrc(5, 5, 10, 20);
    gfx::RectF kSrcF(kSrc);
    gfx::Rect kExpected(133, 211, 150, 560);

    gfx::Rect kActual = View::ConvertRectToTarget(child_child, &top_view, kSrc);
    EXPECT_EQ(kActual, kExpected);

    gfx::RectF kActualF =
        View::ConvertRectToTarget(child_child, &top_view, kSrcF);
    EXPECT_TRUE(kActualF.ApproximatelyEqual(gfx::RectF(kExpected),
                                            kDefaultAllowedConversionError,
                                            kDefaultAllowedConversionError));

    View::ConvertRectToTarget(child_child, &top_view, &kSrcF);
    EXPECT_TRUE(kSrcF.ApproximatelyEqual(gfx::RectF(kExpected),
                                         kDefaultAllowedConversionError,
                                         kDefaultAllowedConversionError));

    // top->child_child
    point.SetPoint(133, 211);
    View::ConvertPointToTarget(&top_view, child_child, &point);
    EXPECT_EQ(5, point.x());
    EXPECT_EQ(5, point.y());

    kSrc.SetRect(133, 211, 150, 560);
    kSrcF = gfx::RectF(kSrc);
    kExpected.SetRect(5, 5, 10, 20);

    kActual = View::ConvertRectToTarget(&top_view, child_child, kSrc);
    EXPECT_EQ(kActual, kExpected);

    kActualF = View::ConvertRectToTarget(&top_view, child_child, kSrcF);
    EXPECT_TRUE(kActualF.ApproximatelyEqual(gfx::RectF(kExpected),
                                            kDefaultAllowedConversionError,
                                            kDefaultAllowedConversionError));

    View::ConvertRectToTarget(&top_view, child_child, &kSrcF);
    EXPECT_TRUE(kSrcF.ApproximatelyEqual(gfx::RectF(kExpected),
                                         kDefaultAllowedConversionError,
                                         kDefaultAllowedConversionError));
  }

  // Conversions from child_child->child and child->child_child
  {
    // child_child->child
    gfx::Point point(5, 5);
    View::ConvertPointToTarget(child_child, child, &point);
    EXPECT_EQ(42, point.x());
    EXPECT_EQ(48, point.y());

    gfx::Rect kSrc(5, 5, 10, 20);
    gfx::RectF kSrcF(kSrc);
    gfx::Rect kExpected(42, 48, 50, 140);

    gfx::Rect kActual = View::ConvertRectToTarget(child_child, child, kSrc);
    EXPECT_EQ(kActual, kExpected);

    gfx::RectF kActualF = View::ConvertRectToTarget(child_child, child, kSrcF);
    EXPECT_TRUE(kActualF.ApproximatelyEqual(gfx::RectF(kExpected),
                                            kDefaultAllowedConversionError,
                                            kDefaultAllowedConversionError));

    View::ConvertRectToTarget(child_child, child, &kSrcF);
    EXPECT_TRUE(kSrcF.ApproximatelyEqual(gfx::RectF(kExpected),
                                         kDefaultAllowedConversionError,
                                         kDefaultAllowedConversionError));

    // child->child_child
    point.SetPoint(42, 48);
    View::ConvertPointToTarget(child, child_child, &point);
    EXPECT_EQ(5, point.x());
    EXPECT_EQ(5, point.y());

    kSrc.SetRect(42, 48, 50, 140);
    kSrcF = gfx::RectF(kSrc);
    kExpected.SetRect(5, 5, 10, 20);

    kActual = View::ConvertRectToTarget(child, child_child, kSrc);
    EXPECT_EQ(kActual, kExpected);

    kActualF = View::ConvertRectToTarget(child, child_child, kSrcF);
    EXPECT_TRUE(kActualF.ApproximatelyEqual(gfx::RectF(kExpected),
                                            kDefaultAllowedConversionError,
                                            kDefaultAllowedConversionError));

    View::ConvertRectToTarget(child, child_child, &kSrcF);
    EXPECT_TRUE(kSrcF.ApproximatelyEqual(gfx::RectF(kExpected),
                                         kDefaultAllowedConversionError,
                                         kDefaultAllowedConversionError));
  }

  // Conversions from top_view to child with a value that should be negative.
  // This ensures we don't round up with negative numbers.
  {
    gfx::Point point(6, 18);
    View::ConvertPointToTarget(&top_view, child, &point);
    EXPECT_EQ(-1, point.x());
    EXPECT_EQ(-1, point.y());

    float error = 0.01f;
    gfx::RectF rect(6.0f, 18.0f, 10.0f, 39.0f);
    View::ConvertRectToTarget(&top_view, child, &rect);
    EXPECT_NEAR(-0.33f, rect.x(), error);
    EXPECT_NEAR(-0.25f, rect.y(), error);
    EXPECT_NEAR(3.33f, rect.width(), error);
    EXPECT_NEAR(9.75f, rect.height(), error);
  }

  // Rect conversions from top_view->child_2 and child_2->top_view.
  {
    // top_view->child_2
    gfx::Rect kSrc(50, 55, 20, 30);
    gfx::RectF kSrcF(kSrc);
    gfx::Rect kExpected(615, 775, 30, 20);

    gfx::Rect kActual = View::ConvertRectToTarget(child_2, &top_view, kSrc);
    EXPECT_EQ(kActual, kExpected);

    gfx::RectF kActualF = View::ConvertRectToTarget(child_2, &top_view, kSrcF);
    EXPECT_TRUE(kActualF.ApproximatelyEqual(gfx::RectF(kExpected),
                                            kDefaultAllowedConversionError,
                                            kDefaultAllowedConversionError));

    View::ConvertRectToTarget(child_2, &top_view, &kSrcF);
    EXPECT_TRUE(kSrcF.ApproximatelyEqual(gfx::RectF(kExpected),
                                         kDefaultAllowedConversionError,
                                         kDefaultAllowedConversionError));

    // child_2->top_view
    kSrc.SetRect(615, 775, 30, 20);
    kSrcF = gfx::RectF(kSrc);
    kExpected.SetRect(50, 55, 20, 30);

    kActual = View::ConvertRectToTarget(&top_view, child_2, kSrc);
    EXPECT_EQ(kActual, kExpected);

    kActualF = View::ConvertRectToTarget(&top_view, child_2, kSrcF);
    EXPECT_TRUE(kActualF.ApproximatelyEqual(gfx::RectF(kExpected),
                                            kDefaultAllowedConversionError,
                                            kDefaultAllowedConversionError));

    View::ConvertRectToTarget(&top_view, child_2, &kSrcF);
    EXPECT_TRUE(kSrcF.ApproximatelyEqual(gfx::RectF(kExpected),
                                         kDefaultAllowedConversionError,
                                         kDefaultAllowedConversionError));
  }
}

// Tests conversion methods to and from screen coordinates.
TEST_F(ViewTest, ConversionsToFromScreen) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));

  View* child = widget->GetRootView()->AddChildView(std::make_unique<View>());
  child->SetBounds(10, 10, 100, 200);
  gfx::Transform t;
  t.Scale(0.5, 0.5);
  child->SetTransform(t);

  gfx::Size size(10, 10);
  gfx::Point point_in_screen(100, 90);
  gfx::Point point_in_child(80, 60);
  gfx::Rect rect_in_screen(point_in_screen, size);
  gfx::Rect rect_in_child(point_in_child, size);

  gfx::Point point = point_in_screen;
  View::ConvertPointFromScreen(child, &point);
  EXPECT_EQ(point_in_child.ToString(), point.ToString());

  View::ConvertPointToScreen(child, &point);
  EXPECT_EQ(point_in_screen.ToString(), point.ToString());

  View::ConvertRectToScreen(child, &rect_in_child);
  EXPECT_EQ(rect_in_screen.ToString(), rect_in_child.ToString());
}

// Tests conversion methods for rectangles.
TEST_F(ViewTest, ConvertRectWithTransform) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();

  TestView* v1 = root->AddChildView(std::make_unique<TestView>());
  TestView* v2 = v1->AddChildView(std::make_unique<TestView>());

  v1->SetBoundsRect(gfx::Rect(10, 10, 500, 500));
  v2->SetBoundsRect(gfx::Rect(20, 20, 100, 200));

  // |v2| now occupies (30, 30) to (130, 230) in |widget|
  gfx::Rect rect(5, 5, 15, 40);
  EXPECT_EQ(gfx::Rect(25, 25, 15, 40), v2->ConvertRectToParent(rect));
  EXPECT_EQ(gfx::Rect(35, 35, 15, 40), v2->ConvertRectToWidget(rect));

  // Rotate |v2|
  gfx::Transform t2 = RotationCounterclockwise();
  t2.set_rc(1, 3, 100.f);
  v2->SetTransform(t2);

  // |v2| now occupies (30, 30) to (230, 130) in |widget|
  EXPECT_EQ(gfx::Rect(25, 100, 40, 15), v2->ConvertRectToParent(rect));
  EXPECT_EQ(gfx::Rect(35, 110, 40, 15), v2->ConvertRectToWidget(rect));

  // Scale down |v1|
  gfx::Transform t1;
  t1.Scale(0.5, 0.5);
  v1->SetTransform(t1);

  // The rectangle should remain the same for |v1|.
  EXPECT_EQ(gfx::Rect(25, 100, 40, 15), v2->ConvertRectToParent(rect));

  // |v2| now occupies (20, 20) to (120, 70) in |widget|
  EXPECT_EQ(gfx::Rect(22, 60, 21, 8).ToString(),
            v2->ConvertRectToWidget(rect).ToString());
}

class ObserverView : public View {
  METADATA_HEADER(ObserverView, View)

 public:
  ObserverView();

  ObserverView(const ObserverView&) = delete;
  ObserverView& operator=(const ObserverView&) = delete;

  ~ObserverView() override;

  void ForgetOldDetails();

  std::optional<ViewHierarchyChangedDetails> add_details() {
    return add_details_;
  }

  std::optional<ViewHierarchyChangedDetails> remove_details() {
    return remove_details_;
  }

 private:
  // View:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  std::optional<ViewHierarchyChangedDetails> add_details_;
  std::optional<ViewHierarchyChangedDetails> remove_details_;
};

ObserverView::ObserverView() = default;

ObserverView::~ObserverView() = default;

void ObserverView::ForgetOldDetails() {
  add_details_.reset();
  remove_details_.reset();
}

void ObserverView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  (details.is_add ? add_details_ : remove_details_).emplace(details);
}

BEGIN_METADATA(ObserverView)
END_METADATA

using ViewHierarchyChangedTest = ViewTest;

void ForgetAllOldDetails(std::vector<ObserverView*> views) {
  for (auto* view : views) {
    view->ForgetOldDetails();
  }
}

TEST_F(ViewHierarchyChangedTest, ParentReceivesAdd) {
  auto parent = std::make_unique<ObserverView>();
  auto* child = parent->AddChildView(std::make_unique<ObserverView>());

  ASSERT_TRUE(parent->add_details().has_value());
  EXPECT_FALSE(parent->remove_details().has_value());
  EXPECT_EQ(parent.get(), parent->add_details()->parent);
  EXPECT_EQ(child, parent->add_details()->child);
  EXPECT_EQ(nullptr, parent->add_details()->move_view);

  ForgetAllOldDetails({parent.get(), child});
}

TEST_F(ViewHierarchyChangedTest, ChildReceivesAdd) {
  auto parent = std::make_unique<ObserverView>();
  auto* child = parent->AddChildView(std::make_unique<ObserverView>());

  ASSERT_TRUE(child->add_details().has_value());
  EXPECT_FALSE(child->remove_details().has_value());
  EXPECT_EQ(parent.get(), child->add_details()->parent);
  EXPECT_EQ(child, child->add_details()->child);
  EXPECT_EQ(nullptr, child->add_details()->move_view);

  ForgetAllOldDetails({parent.get(), child});
}

TEST_F(ViewHierarchyChangedTest, HierarchyReceivesAdd) {
  auto child = std::make_unique<ObserverView>();
  auto* grandchild = child->AddChildView(std::make_unique<ObserverView>());

  ForgetAllOldDetails({child.get(), grandchild});

  auto parent = std::make_unique<ObserverView>();
  auto* weak_child = parent->AddChildView(std::move(child));

  for (auto* view : {parent.get(), weak_child, grandchild}) {
    ASSERT_TRUE(view->add_details().has_value());
    EXPECT_FALSE(view->remove_details().has_value());
    EXPECT_EQ(parent.get(), view->add_details()->parent);
    EXPECT_EQ(weak_child, view->add_details()->child);
    EXPECT_EQ(nullptr, view->add_details()->move_view);
  }

  ForgetAllOldDetails({parent.get(), weak_child, grandchild});
}

TEST_F(ViewHierarchyChangedTest, HierarchyReceivesRemove) {
  auto parent = std::make_unique<ObserverView>();
  auto* weak_child = parent->AddChildView(std::make_unique<ObserverView>());
  auto* grandchild = weak_child->AddChildView(std::make_unique<ObserverView>());

  ForgetAllOldDetails({parent.get(), weak_child, grandchild});

  auto child = parent->RemoveChildViewT(weak_child);

  // The parent and child both receive a remove:
  for (auto* view : {parent.get(), weak_child}) {
    EXPECT_FALSE(view->add_details().has_value());
    ASSERT_TRUE(view->remove_details().has_value());
    EXPECT_EQ(parent.get(), view->remove_details()->parent);
    EXPECT_EQ(weak_child, view->remove_details()->child);
    EXPECT_EQ(nullptr, view->remove_details()->move_view);
  }

  // The grandchild also receives a remove, but the removed view is marked as
  // the *grandchild*, not as the child.
  // TODO(ellyjones): Why does that happen? It seems like either:
  // a) The grandchild should receive no notification,
  // b) The grandchild should receive a notification with parent == parent and
  //    child == child (same as the other two views receive), or
  // c) The grandchild should receive a notification with parent == child and
  //    child == grandchild
  EXPECT_FALSE(grandchild->add_details().has_value());
  ASSERT_TRUE(grandchild->remove_details().has_value());
  EXPECT_EQ(parent.get(), grandchild->remove_details()->parent);
  EXPECT_EQ(grandchild, grandchild->remove_details()->child);
  EXPECT_EQ(nullptr, grandchild->remove_details()->move_view);

  ForgetAllOldDetails({parent.get(), weak_child, grandchild});
}

TEST_F(ViewHierarchyChangedTest, HierarchyReceivesMove) {
  auto new_parent = std::make_unique<ObserverView>();
  auto old_parent = std::make_unique<ObserverView>();
  auto* sibling = old_parent->AddChildView(std::make_unique<ObserverView>());
  auto* child = old_parent->AddChildView(std::make_unique<ObserverView>());

  ForgetAllOldDetails({new_parent.get(), old_parent.get(), sibling, child});

  new_parent->AddChildView(child);

  // The new parent receives an add:
  ASSERT_TRUE(new_parent->add_details().has_value());
  EXPECT_FALSE(new_parent->remove_details().has_value());
  EXPECT_EQ(new_parent.get(), new_parent->add_details()->parent);
  EXPECT_EQ(child, new_parent->add_details()->child);
  EXPECT_EQ(old_parent.get(), new_parent->add_details()->move_view);

  // The old parent receives a remove:
  EXPECT_FALSE(old_parent->add_details().has_value());
  ASSERT_TRUE(old_parent->remove_details().has_value());
  EXPECT_EQ(old_parent.get(), old_parent->remove_details()->parent);
  EXPECT_EQ(child, old_parent->remove_details()->child);
  EXPECT_EQ(new_parent.get(), old_parent->remove_details()->move_view);

  // The sibling is not affected and receives neither:
  EXPECT_FALSE(sibling->add_details().has_value());
  EXPECT_FALSE(sibling->remove_details().has_value());

  // The reparented child receives a remove from the old parent and an add to
  // the new parent:
  ASSERT_TRUE(child->remove_details().has_value());
  EXPECT_EQ(old_parent.get(), child->remove_details()->parent);
  EXPECT_EQ(child, child->remove_details()->child);
  EXPECT_EQ(new_parent.get(), child->remove_details()->move_view);
  ASSERT_TRUE(child->add_details().has_value());
  EXPECT_EQ(new_parent.get(), child->add_details()->parent);
  EXPECT_EQ(child, child->add_details()->child);
  EXPECT_EQ(old_parent.get(), child->add_details()->move_view);

  ForgetAllOldDetails({old_parent.get(), new_parent.get(), sibling, child});
}

class WidgetObserverView : public View {
  METADATA_HEADER(WidgetObserverView, View)

 public:
  WidgetObserverView();

  WidgetObserverView(const WidgetObserverView&) = delete;
  WidgetObserverView& operator=(const WidgetObserverView&) = delete;

  ~WidgetObserverView() override;

  void ResetTestState();

  int added_to_widget_count() { return added_to_widget_count_; }
  int removed_from_widget_count() { return removed_from_widget_count_; }

 private:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  int added_to_widget_count_ = 0;
  int removed_from_widget_count_ = 0;
};

WidgetObserverView::WidgetObserverView() {
  ResetTestState();
}

WidgetObserverView::~WidgetObserverView() = default;

void WidgetObserverView::ResetTestState() {
  added_to_widget_count_ = 0;
  removed_from_widget_count_ = 0;
}

void WidgetObserverView::AddedToWidget() {
  ++added_to_widget_count_;
}

void WidgetObserverView::RemovedFromWidget() {
  ++removed_from_widget_count_;
}

BEGIN_METADATA(WidgetObserverView)
END_METADATA

// Verifies that AddedToWidget and RemovedFromWidget are called for a view when
// it is added to hierarchy.
// The tree looks like this:
// widget
// +-- root
//
// then v1 is added to root:
//
//     v1
//     +-- v2
//
// finally v1 is removed from root.
TEST_F(ViewTest, AddedToRemovedFromWidget) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));

  View* root = widget->GetRootView();

  auto v1 = std::make_unique<WidgetObserverView>();
  auto v2 = std::make_unique<WidgetObserverView>();
  auto v3 = std::make_unique<WidgetObserverView>();

  WidgetObserverView* v2_ptr = v1->AddChildView(std::move(v2));
  EXPECT_EQ(0, v2_ptr->added_to_widget_count());
  EXPECT_EQ(0, v2_ptr->removed_from_widget_count());

  WidgetObserverView* v1_ptr = root->AddChildView(std::move(v1));
  EXPECT_EQ(1, v1_ptr->added_to_widget_count());
  EXPECT_EQ(0, v1_ptr->removed_from_widget_count());
  EXPECT_EQ(1, v2_ptr->added_to_widget_count());
  EXPECT_EQ(0, v2_ptr->removed_from_widget_count());

  v1_ptr->ResetTestState();
  v2_ptr->ResetTestState();

  WidgetObserverView* v3_ptr = v2_ptr->AddChildView(std::move(v3));
  EXPECT_EQ(0, v1_ptr->added_to_widget_count());
  EXPECT_EQ(0, v1_ptr->removed_from_widget_count());
  EXPECT_EQ(0, v2_ptr->added_to_widget_count());
  EXPECT_EQ(0, v2_ptr->removed_from_widget_count());

  v1_ptr->ResetTestState();
  v2_ptr->ResetTestState();

  v1 = root->RemoveChildViewT(v1_ptr);
  EXPECT_EQ(0, v1->added_to_widget_count());
  EXPECT_EQ(1, v1->removed_from_widget_count());
  EXPECT_EQ(0, v2_ptr->added_to_widget_count());
  EXPECT_EQ(1, v2_ptr->removed_from_widget_count());

  v2_ptr->ResetTestState();
  v2 = v1->RemoveChildViewT(v2_ptr);
  EXPECT_EQ(0, v2->removed_from_widget_count());

  // Test move between parents in a single Widget.
  v3 = v2->RemoveChildViewT(v3_ptr);
  v1->ResetTestState();
  v2->ResetTestState();
  v3->ResetTestState();

  v2_ptr = v1->AddChildView(std::move(v2));
  v1_ptr = root->AddChildView(std::move(v1));
  v3_ptr = root->AddChildView(std::move(v3));
  EXPECT_EQ(1, v1_ptr->added_to_widget_count());
  EXPECT_EQ(1, v2_ptr->added_to_widget_count());
  EXPECT_EQ(1, v3_ptr->added_to_widget_count());

  // This should not invoke added or removed to/from the widget.
  v1_ptr = v3_ptr->AddChildView(v1_ptr);
  EXPECT_EQ(1, v1_ptr->added_to_widget_count());
  EXPECT_EQ(0, v1_ptr->removed_from_widget_count());
  EXPECT_EQ(1, v2_ptr->added_to_widget_count());
  EXPECT_EQ(0, v2_ptr->removed_from_widget_count());
  EXPECT_EQ(1, v3_ptr->added_to_widget_count());
  EXPECT_EQ(0, v3_ptr->removed_from_widget_count());

  // Test move between widgets.
  auto second_widget = std::make_unique<Widget>();
  params = CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                        Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(150, 150, 650, 650);
  second_widget->Init(std::move(params));

  View* second_root = second_widget->GetRootView();

  v1_ptr->ResetTestState();
  v2_ptr->ResetTestState();
  v3_ptr->ResetTestState();

  v1_ptr =
      second_root->AddChildView(v1_ptr->parent()->RemoveChildViewT(v1_ptr));
  EXPECT_EQ(1, v1_ptr->removed_from_widget_count());
  EXPECT_EQ(1, v1_ptr->added_to_widget_count());
  EXPECT_EQ(1, v2_ptr->added_to_widget_count());
  EXPECT_EQ(1, v2_ptr->removed_from_widget_count());
  EXPECT_EQ(0, v3_ptr->added_to_widget_count());
  EXPECT_EQ(0, v3_ptr->removed_from_widget_count());
}

// Verifies if the child views added under the root are all deleted when calling
// RemoveAllChildViews.
// The tree looks like this:
// root
// +-- child1
//     +-- foo
//         +-- bar0
//         +-- bar1
//         +-- bar2
// +-- child2
// +-- child3
TEST_F(ViewTest, RemoveAllChildViews) {
  auto root = std::make_unique<View>();

  View* child1 = root->AddChildView(std::make_unique<View>());

  for (size_t i = 0; i < 2; ++i)
    root->AddChildView(std::make_unique<View>());

  View* foo = child1->AddChildView(std::make_unique<View>());

  // Add some nodes to |foo|.
  for (size_t i = 0; i < 3; ++i)
    foo->AddChildView(std::make_unique<View>());

  EXPECT_EQ(3u, root->children().size());
  EXPECT_EQ(1u, child1->children().size());
  EXPECT_EQ(3u, foo->children().size());

  // Now remove all child views from root.
  root->RemoveAllChildViews();

  EXPECT_TRUE(root->children().empty());
}

TEST_F(ViewTest, Contains) {
  auto v1 = std::make_unique<View>();

  auto* v2 = v1->AddChildView(std::make_unique<View>());
  auto* v3 = v2->AddChildView(std::make_unique<View>());

  EXPECT_FALSE(v1->Contains(nullptr));
  EXPECT_TRUE(v1->Contains(v1.get()));
  EXPECT_TRUE(v1->Contains(v2));
  EXPECT_TRUE(v1->Contains(v3));

  EXPECT_FALSE(v2->Contains(nullptr));
  EXPECT_TRUE(v2->Contains(v2));
  EXPECT_FALSE(v2->Contains(v1.get()));
  EXPECT_TRUE(v2->Contains(v3));

  EXPECT_FALSE(v3->Contains(nullptr));
  EXPECT_TRUE(v3->Contains(v3));
  EXPECT_FALSE(v3->Contains(v1.get()));
  EXPECT_FALSE(v3->Contains(v2));
}

// Verifies if GetIndexOf() returns the correct index for the specified child
// view.
// The tree looks like this:
// root
// +-- child1
//     +-- foo1
// +-- child2
TEST_F(ViewTest, GetIndexOf) {
  auto root = std::make_unique<View>();

  auto* child1 = root->AddChildView(std::make_unique<View>());

  auto* child2 = root->AddChildView(std::make_unique<View>());

  auto* foo1 = child1->AddChildView(std::make_unique<View>());

  EXPECT_FALSE(root->GetIndexOf(nullptr).has_value());
  EXPECT_FALSE(root->GetIndexOf(root.get()).has_value());
  EXPECT_EQ(0u, root->GetIndexOf(child1));
  EXPECT_EQ(1u, root->GetIndexOf(child2));
  EXPECT_FALSE(root->GetIndexOf(foo1).has_value());

  EXPECT_FALSE(child1->GetIndexOf(nullptr).has_value());
  EXPECT_FALSE(child1->GetIndexOf(root.get()).has_value());
  EXPECT_FALSE(child1->GetIndexOf(child1).has_value());
  EXPECT_FALSE(child1->GetIndexOf(child2).has_value());
  EXPECT_EQ(0u, child1->GetIndexOf(foo1));

  EXPECT_FALSE(child2->GetIndexOf(nullptr).has_value());
  EXPECT_FALSE(child2->GetIndexOf(root.get()).has_value());
  EXPECT_FALSE(child2->GetIndexOf(child2).has_value());
  EXPECT_FALSE(child2->GetIndexOf(child1).has_value());
  EXPECT_FALSE(child2->GetIndexOf(foo1).has_value());
}

// Verifies that the child views can be reordered correctly.
TEST_F(ViewTest, ReorderChildren) {
  auto root = std::make_unique<View>();

  auto* child = root->AddChildView(std::make_unique<View>());

  auto* foo1 = child->AddChildView(std::make_unique<View>());
  auto* foo2 = child->AddChildView(std::make_unique<View>());
  auto* foo3 = child->AddChildView(std::make_unique<View>());
  foo1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  foo2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  foo3->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  ASSERT_EQ(0u, child->GetIndexOf(foo1));
  ASSERT_EQ(1u, child->GetIndexOf(foo2));
  ASSERT_EQ(2u, child->GetIndexOf(foo3));
  ASSERT_EQ(foo2, foo1->GetNextFocusableView());
  ASSERT_EQ(foo3, foo2->GetNextFocusableView());
  ASSERT_EQ(nullptr, foo3->GetNextFocusableView());

  // Move |foo2| at the end.
  child->ReorderChildView(foo2, child->children().size());
  ASSERT_EQ(0u, child->GetIndexOf(foo1));
  ASSERT_EQ(1u, child->GetIndexOf(foo3));
  ASSERT_EQ(2u, child->GetIndexOf(foo2));
  ASSERT_EQ(foo3, foo1->GetNextFocusableView());
  ASSERT_EQ(foo2, foo3->GetNextFocusableView());
  ASSERT_EQ(nullptr, foo2->GetNextFocusableView());

  // Move |foo1| at the end.
  child->ReorderChildView(foo1, child->children().size());
  ASSERT_EQ(0u, child->GetIndexOf(foo3));
  ASSERT_EQ(1u, child->GetIndexOf(foo2));
  ASSERT_EQ(2u, child->GetIndexOf(foo1));
  ASSERT_EQ(nullptr, foo1->GetNextFocusableView());
  ASSERT_EQ(foo2, foo1->GetPreviousFocusableView());
  ASSERT_EQ(foo2, foo3->GetNextFocusableView());
  ASSERT_EQ(foo1, foo2->GetNextFocusableView());

  // Move |foo2| to the front.
  child->ReorderChildView(foo2, 0);
  ASSERT_EQ(0u, child->GetIndexOf(foo2));
  ASSERT_EQ(1u, child->GetIndexOf(foo3));
  ASSERT_EQ(2u, child->GetIndexOf(foo1));
  ASSERT_EQ(nullptr, foo1->GetNextFocusableView());
  ASSERT_EQ(foo3, foo1->GetPreviousFocusableView());
  ASSERT_EQ(foo3, foo2->GetNextFocusableView());
  ASSERT_EQ(foo1, foo3->GetNextFocusableView());
}

// Verifies that GetViewByID returns the correctly child view from the specified
// ID.
// The tree looks like this:
// v1
// +-- v2
//     +-- v3
//     +-- v4
TEST_F(ViewTest, GetViewByID) {
  View v1;
  const int kV1ID = 1;
  v1.SetID(kV1ID);

  View v2;
  const int kV2ID = 2;
  v2.SetID(kV2ID);

  View v3;
  const int kV3ID = 3;
  v3.SetID(kV3ID);

  View v4;
  const int kV4ID = 4;
  v4.SetID(kV4ID);

  const int kV5ID = 5;

  v1.AddChildView(&v2);
  v2.AddChildView(&v3);
  v2.AddChildView(&v4);

  EXPECT_EQ(&v1, v1.GetViewByID(kV1ID));
  EXPECT_EQ(&v2, v1.GetViewByID(kV2ID));
  EXPECT_EQ(&v4, v1.GetViewByID(kV4ID));

  EXPECT_EQ(nullptr, v1.GetViewByID(kV5ID));  // No V5 exists.
  EXPECT_EQ(nullptr,
            v2.GetViewByID(kV1ID));  // It can get only from child views.

  const int kGroup = 1;
  v3.SetGroup(kGroup);
  v4.SetGroup(kGroup);

  View::Views views;
  v1.GetViewsInGroup(kGroup, &views);
  EXPECT_EQ(2U, views.size());
  EXPECT_TRUE(base::Contains(views, &v3));
  EXPECT_TRUE(base::Contains(views, &v4));
}

TEST_F(ViewTest, AddExistingChild) {
  auto v1 = std::make_unique<View>();

  auto* v2 = v1->AddChildView(std::make_unique<View>());
  auto* v3 = v1->AddChildView(std::make_unique<View>());
  EXPECT_EQ(0u, v1->GetIndexOf(v2));
  EXPECT_EQ(1u, v1->GetIndexOf(v3));

  // Check that there's no change in order when adding at same index.
  v1->AddChildViewAt(v2, 0);
  EXPECT_EQ(0u, v1->GetIndexOf(v2));
  EXPECT_EQ(1u, v1->GetIndexOf(v3));
  v1->AddChildViewAt(v3, 1);
  EXPECT_EQ(0u, v1->GetIndexOf(v2));
  EXPECT_EQ(1u, v1->GetIndexOf(v3));

  // Add it at a different index and check for change in order.
  v1->AddChildViewAt(v2, 1);
  EXPECT_EQ(1u, v1->GetIndexOf(v2));
  EXPECT_EQ(0u, v1->GetIndexOf(v3));
  v1->AddChildViewAt(v2, 0);
  EXPECT_EQ(0u, v1->GetIndexOf(v2));
  EXPECT_EQ(1u, v1->GetIndexOf(v3));

  // Check that calling AddChildView() moves to the end.
  v1->AddChildView(v2);
  EXPECT_EQ(1u, v1->GetIndexOf(v2));
  EXPECT_EQ(0u, v1->GetIndexOf(v3));
  v1->AddChildView(v3);
  EXPECT_EQ(0u, v1->GetIndexOf(v2));
  EXPECT_EQ(1u, v1->GetIndexOf(v3));
}

TEST_F(ViewTest, UseMirroredLayoutDisableMirroring) {
  base::i18n::SetICUDefaultLocale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());

  View parent, child1, child2;
  parent.SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));

  child1.SetPreferredSize(gfx::Size(10, 10));
  child2.SetPreferredSize(gfx::Size(10, 10));

  parent.AddChildView(&child1);
  parent.AddChildView(&child2);
  parent.SizeToPreferredSize();

  EXPECT_EQ(child1.GetNextFocusableView(), &child2);
  EXPECT_GT(child1.GetMirroredX(), child2.GetMirroredX());
  EXPECT_LT(child1.x(), child2.x());
  EXPECT_NE(parent.GetMirroredXInView(5), 5);

  parent.SetMirrored(false);

  EXPECT_EQ(child1.GetNextFocusableView(), &child2);
  EXPECT_GT(child2.GetMirroredX(), child1.GetMirroredX());
  EXPECT_LT(child1.x(), child2.x());
  EXPECT_EQ(parent.GetMirroredXInView(5), 5);
}

TEST_F(ViewTest, UseMirroredLayoutEnableMirroring) {
  base::i18n::SetICUDefaultLocale("en");
  ASSERT_FALSE(base::i18n::IsRTL());

  View parent, child1, child2;
  parent.SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));

  child1.SetPreferredSize(gfx::Size(10, 10));
  child2.SetPreferredSize(gfx::Size(10, 10));

  parent.AddChildView(&child1);
  parent.AddChildView(&child2);
  parent.SizeToPreferredSize();

  EXPECT_EQ(child1.GetNextFocusableView(), &child2);
  EXPECT_LT(child1.GetMirroredX(), child2.GetMirroredX());
  EXPECT_LT(child1.x(), child2.x());
  EXPECT_NE(parent.GetMirroredXInView(5), 15);

  parent.SetMirrored(true);

  EXPECT_EQ(child1.GetNextFocusableView(), &child2);
  EXPECT_LT(child2.GetMirroredX(), child1.GetMirroredX());
  EXPECT_LT(child1.x(), child2.x());
  EXPECT_EQ(parent.GetMirroredXInView(5), 15);
}

////////////////////////////////////////////////////////////////////////////////
// FocusManager
////////////////////////////////////////////////////////////////////////////////

// A widget that always claims to be active, regardless of its real activation
// status.
class ActiveWidget : public Widget {
 public:
  ActiveWidget() = default;

  ActiveWidget(const ActiveWidget&) = delete;
  ActiveWidget& operator=(const ActiveWidget&) = delete;

  ~ActiveWidget() override = default;

  bool IsActive() const override { return true; }
};

TEST_F(ViewTest, AdvanceFocusIfNecessaryForUnfocusableView) {
  // Create a widget with two views and give the first one focus.
  auto widget = std::make_unique<ActiveWidget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(params));

  View* view1 = widget->GetRootView()->AddChildView(std::make_unique<View>());
  view1->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  View* view2 = widget->GetRootView()->AddChildView(std::make_unique<View>());
  view2->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  FocusManager* focus_manager = widget->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  focus_manager->SetFocusedView(view1);
  EXPECT_EQ(view1, focus_manager->GetFocusedView());

  // Disable the focused view and check if the next view gets focused.
  view1->SetEnabled(false);
  EXPECT_EQ(view2, focus_manager->GetFocusedView());

  // Re-enable and re-focus.
  view1->SetEnabled(true);
  focus_manager->SetFocusedView(view1);
  EXPECT_EQ(view1, focus_manager->GetFocusedView());

  // Hide the focused view and check it the next view gets focused.
  view1->SetVisible(false);
  EXPECT_EQ(view2, focus_manager->GetFocusedView());

  // Re-show and re-focus.
  view1->SetVisible(true);
  focus_manager->SetFocusedView(view1);
  EXPECT_EQ(view1, focus_manager->GetFocusedView());

  // Set the focused view as not focusable and check if the next view gets
  // focused.
  view1->SetFocusBehavior(View::FocusBehavior::NEVER);
  EXPECT_EQ(view2, focus_manager->GetFocusedView());
}

////////////////////////////////////////////////////////////////////////////////
// Layers
////////////////////////////////////////////////////////////////////////////////

namespace {

// Test implementation of LayerAnimator.
class TestLayerAnimator : public ui::LayerAnimator {
 public:
  TestLayerAnimator();

  TestLayerAnimator(const TestLayerAnimator&) = delete;
  TestLayerAnimator& operator=(const TestLayerAnimator&) = delete;

  const gfx::Rect& last_bounds() const { return last_bounds_; }

  // LayerAnimator.
  void SetBounds(const gfx::Rect& bounds) override;

 protected:
  ~TestLayerAnimator() override = default;

 private:
  gfx::Rect last_bounds_;
};

TestLayerAnimator::TestLayerAnimator()
    : ui::LayerAnimator(base::Milliseconds(0)) {}

void TestLayerAnimator::SetBounds(const gfx::Rect& bounds) {
  last_bounds_ = bounds;
}

class TestingLayerViewObserver : public ViewObserver {
 public:
  explicit TestingLayerViewObserver(View* view) : view_(view) {
    view_->AddObserver(this);
  }

  TestingLayerViewObserver(const TestingLayerViewObserver&) = delete;
  TestingLayerViewObserver& operator=(const TestingLayerViewObserver&) = delete;

  ~TestingLayerViewObserver() override { view_->RemoveObserver(this); }

  gfx::Rect GetLastLayerBoundsAndReset() {
    gfx::Rect value = last_layer_bounds_;
    last_layer_bounds_ = gfx::Rect();
    return value;
  }

  gfx::Rect GetLastClipRectAndReset() {
    gfx::Rect value = last_clip_rect_;
    last_clip_rect_ = gfx::Rect();
    return value;
  }

 private:
  // ViewObserver:
  void OnViewLayerBoundsSet(View* view) override {
    last_layer_bounds_ = view->layer()->bounds();
  }

  void OnViewLayerClipRectChanged(View* view) override {
    last_clip_rect_ = view->layer()->clip_rect();
  }

  gfx::Rect last_layer_bounds_;
  gfx::Rect last_clip_rect_;
  raw_ptr<View> view_;
};

}  // namespace

class ViewLayerTest : public ViewsTestBase {
 public:
  ViewLayerTest() = default;

  ~ViewLayerTest() override = default;

  // Returns the Layer used by the RootView.
  ui::Layer* GetRootLayer() { return widget()->GetLayer(); }

  void SetUp() override {
    SetUpPixelCanvas();
    ViewTest::SetUp();
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(50, 50, 200, 200);
    widget_->Init(std::move(params));
    widget_->Show();
    widget_->GetRootView()->SetBounds(0, 0, 200, 200);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  Widget* widget() { return widget_.get(); }

  virtual void SetUpPixelCanvas() {
    scoped_feature_list_.InitAndDisableFeature(
        ::features::kEnablePixelCanvasRecording);
  }

 protected:
  // Accessors to View internals.
  void SchedulePaintOnParent(View* view) { view->SchedulePaintOnParent(); }

 private:
  std::unique_ptr<Widget> widget_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ViewLayerTest, LayerCreationAndDestruction) {
  View view;
  EXPECT_EQ(nullptr, view.layer());

  view.SetPaintToLayer();
  EXPECT_NE(nullptr, view.layer());

  view.DestroyLayer();
  EXPECT_EQ(nullptr, view.layer());
}

TEST_F(ViewLayerTest, SetTransformCreatesAndDestroysLayer) {
  View view;
  EXPECT_EQ(nullptr, view.layer());

  // Set an arbitrary non-identity transform, which should cause a layer to be
  // created.
  gfx::Transform transform;
  transform.Translate(1.0, 1.0);
  view.SetTransform(transform);
  EXPECT_NE(nullptr, view.layer());

  // Set the identity transform, which should destroy the layer.
  view.SetTransform(gfx::Transform());
  EXPECT_EQ(nullptr, view.layer());
}

// Verify that setting an identity transform after SetPaintToLayer() has been
// called doesn't destroy the layer.
TEST_F(ViewLayerTest, IdentityTransformDoesntOverrideSetPaintToLayer) {
  View view;
  EXPECT_EQ(nullptr, view.layer());

  view.SetPaintToLayer();
  EXPECT_NE(nullptr, view.layer());

  gfx::Transform transform;
  transform.Translate(1.0, 1.0);
  view.SetTransform(transform);
  EXPECT_NE(nullptr, view.layer());

  view.SetTransform(transform);
  EXPECT_NE(nullptr, view.layer());
}

// Verify that calling DestroyLayer() while a non-identity transform is present
// doesn't destroy the layer.
TEST_F(ViewLayerTest, DestroyLayerDoesntOverrideTransform) {
  View view;
  EXPECT_EQ(nullptr, view.layer());

  view.SetPaintToLayer();
  EXPECT_NE(nullptr, view.layer());

  gfx::Transform transform;
  transform.Translate(1.0, 1.0);
  view.SetTransform(transform);
  EXPECT_NE(nullptr, view.layer());

  view.DestroyLayer();
  EXPECT_NE(nullptr, view.layer());
}

TEST_F(ViewLayerTest, LayerToggling) {
  // Because we lazily create textures the calls to DrawTree are necessary to
  // ensure we trigger creation of textures.
  ui::Layer* root_layer = widget()->GetLayer();
  View* content_view = widget()->SetContentsView(std::make_unique<View>());

  // Create v1, give it a bounds and verify everything is set up correctly.
  View* v1 = new View;
  v1->SetPaintToLayer();
  EXPECT_TRUE(v1->layer() != nullptr);
  v1->SetBoundsRect(gfx::Rect(20, 30, 140, 150));
  content_view->AddChildView(v1);
  ASSERT_TRUE(v1->layer() != nullptr);
  EXPECT_EQ(root_layer, v1->layer()->parent());
  EXPECT_EQ(gfx::Rect(20, 30, 140, 150), v1->layer()->bounds());

  // Create v2 as a child of v1 and do basic assertion testing.
  View* v2 = new View;
  TestingLayerViewObserver v2_observer(v2);
  v1->AddChildView(v2);
  EXPECT_TRUE(v2->layer() == nullptr);
  v2->SetBoundsRect(gfx::Rect(10, 20, 30, 40));
  v2->SetPaintToLayer();
  ASSERT_TRUE(v2->layer() != nullptr);
  EXPECT_EQ(v1->layer(), v2->layer()->parent());
  EXPECT_EQ(gfx::Rect(10, 20, 30, 40), v2->layer()->bounds());
  EXPECT_EQ(v2->layer()->bounds(), v2_observer.GetLastLayerBoundsAndReset());

  // Turn off v1s layer. v2 should still have a layer but its parent should have
  // changed.
  v1->DestroyLayer();
  EXPECT_TRUE(v1->layer() == nullptr);
  EXPECT_TRUE(v2->layer() != nullptr);
  EXPECT_EQ(root_layer, v2->layer()->parent());
  ASSERT_EQ(1u, root_layer->children().size());
  EXPECT_EQ(root_layer->children()[0], v2->layer());
  // The bounds of the layer should have changed to be relative to the root view
  // now.
  EXPECT_EQ(gfx::Rect(30, 50, 30, 40), v2->layer()->bounds());
  EXPECT_EQ(v2->layer()->bounds(), v2_observer.GetLastLayerBoundsAndReset());

  // Make v1 have a layer again and verify v2s layer is wired up correctly.
  gfx::Transform transform;
  transform.Scale(2.0, 2.0);
  v1->SetTransform(transform);
  EXPECT_TRUE(v1->layer() != nullptr);
  EXPECT_TRUE(v2->layer() != nullptr);
  EXPECT_EQ(root_layer, v1->layer()->parent());
  EXPECT_EQ(v1->layer(), v2->layer()->parent());
  ASSERT_EQ(1u, root_layer->children().size());
  EXPECT_EQ(root_layer->children()[0], v1->layer());
  ASSERT_EQ(1u, v1->layer()->children().size());
  EXPECT_EQ(v1->layer()->children()[0], v2->layer());
  EXPECT_EQ(gfx::Rect(10, 20, 30, 40), v2->layer()->bounds());
  EXPECT_EQ(v2->layer()->bounds(), v2_observer.GetLastLayerBoundsAndReset());
}

// Verifies turning on a layer wires up children correctly.
TEST_F(ViewLayerTest, NestedLayerToggling) {
  View* content_view = widget()->SetContentsView(std::make_unique<View>());

  // Create v1, give it a bounds and verify everything is set up correctly.
  View* v1 = content_view->AddChildView(std::make_unique<View>());
  v1->SetBoundsRect(gfx::Rect(20, 30, 140, 150));

  View* v2 = v1->AddChildView(std::make_unique<View>());
  v2->SetBoundsRect(gfx::Rect(10, 10, 100, 100));

  View* v3 = v2->AddChildView(std::make_unique<View>());
  TestingLayerViewObserver v3_observer(v3);
  v3->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  v3->SetPaintToLayer();
  ASSERT_TRUE(v3->layer() != nullptr);
  EXPECT_EQ(v3->layer()->bounds(), v3_observer.GetLastLayerBoundsAndReset());

  // At this point we have v1-v2-v3. v3 has a layer, v1 and v2 don't.

  v1->SetPaintToLayer();
  EXPECT_EQ(v1->layer(), v3->layer()->parent());
  EXPECT_EQ(v3->layer()->bounds(), v3_observer.GetLastLayerBoundsAndReset());
}

TEST_F(ViewLayerTest, LayerAnimator) {
  View* content_view = widget()->SetContentsView(std::make_unique<View>());

  View* v1 = content_view->AddChildView(std::make_unique<View>());
  v1->SetPaintToLayer();
  EXPECT_TRUE(v1->layer() != nullptr);

  TestLayerAnimator* animator = new TestLayerAnimator();
  v1->layer()->SetAnimator(animator);

  gfx::Rect bounds(1, 2, 3, 4);
  v1->SetBoundsRect(bounds);
  EXPECT_EQ(bounds, animator->last_bounds());
  // TestLayerAnimator doesn't update the layer.
  EXPECT_NE(bounds, v1->layer()->bounds());
}

// Verifies the bounds of a layer are updated if the bounds of ancestor that
// doesn't have a layer change.
TEST_F(ViewLayerTest, BoundsChangeWithLayer) {
  View* content_view = widget()->SetContentsView(std::make_unique<View>());

  View* v1 = content_view->AddChildView(std::make_unique<View>());
  v1->SetBoundsRect(gfx::Rect(20, 30, 140, 150));

  View* v2 = v1->AddChildView(std::make_unique<View>());
  TestingLayerViewObserver v2_observer(v2);
  v2->SetBoundsRect(gfx::Rect(10, 11, 40, 50));
  v2->SetPaintToLayer();
  ASSERT_TRUE(v2->layer() != nullptr);
  EXPECT_EQ(gfx::Rect(30, 41, 40, 50), v2->layer()->bounds());
  EXPECT_EQ(v2->layer()->bounds(), v2_observer.GetLastLayerBoundsAndReset());

  v1->SetPosition(gfx::Point(25, 36));
  EXPECT_EQ(gfx::Rect(35, 47, 40, 50), v2->layer()->bounds());
  EXPECT_EQ(v2->layer()->bounds(), v2_observer.GetLastLayerBoundsAndReset());

  v2->SetPosition(gfx::Point(11, 12));
  EXPECT_EQ(gfx::Rect(36, 48, 40, 50), v2->layer()->bounds());
  EXPECT_EQ(v2->layer()->bounds(), v2_observer.GetLastLayerBoundsAndReset());

  // Bounds of the layer should change even if the view is not invisible.
  v1->SetVisible(false);
  v1->SetPosition(gfx::Point(20, 30));
  EXPECT_EQ(gfx::Rect(31, 42, 40, 50), v2->layer()->bounds());
  EXPECT_EQ(v2->layer()->bounds(), v2_observer.GetLastLayerBoundsAndReset());

  v2->SetVisible(false);
  v2->SetBoundsRect(gfx::Rect(10, 11, 20, 30));
  EXPECT_EQ(gfx::Rect(30, 41, 20, 30), v2->layer()->bounds());
  EXPECT_EQ(v2->layer()->bounds(), v2_observer.GetLastLayerBoundsAndReset());
}

// Verifies the view observer is triggered when the clip rect of view's layer is
// updated.
TEST_F(ViewLayerTest, LayerClipRectChanged) {
  View* content_view = widget()->SetContentsView(std::make_unique<View>());

  View* v1 = content_view->AddChildView(std::make_unique<View>());
  v1->SetPaintToLayer();

  auto* v1_layer = v1->layer();
  ASSERT_TRUE(v1_layer != nullptr);

  TestingLayerViewObserver v1_observer(v1);
  v1_layer->SetClipRect(gfx::Rect(10, 10, 20, 20));
  EXPECT_EQ(v1_layer->clip_rect(), gfx::Rect(10, 10, 20, 20));
  EXPECT_EQ(v1_layer->clip_rect(), v1_observer.GetLastClipRectAndReset());

  v1_layer->SetClipRect(gfx::Rect(20, 20, 40, 40));
  EXPECT_EQ(v1_layer->clip_rect(), gfx::Rect(20, 20, 40, 40));
  EXPECT_EQ(v1_layer->clip_rect(), v1_observer.GetLastClipRectAndReset());
}

// Make sure layers are positioned correctly in RTL.
TEST_F(ViewLayerTest, BoundInRTL) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");
  View* view = widget()->SetContentsView(std::make_unique<View>());

  int content_width = view->width();

  // |v1| is initially not attached to anything. So its layer will have the same
  // bounds as the view.
  View* v1 = new View;
  v1->SetPaintToLayer();
  v1->SetBounds(10, 10, 20, 10);
  EXPECT_EQ(gfx::Rect(10, 10, 20, 10), v1->layer()->bounds());

  // Once |v1| is attached to the widget, its layer will get RTL-appropriate
  // bounds.
  view->AddChildView(v1);
  EXPECT_EQ(gfx::Rect(content_width - 30, 10, 20, 10), v1->layer()->bounds());
  gfx::Rect l1bounds = v1->layer()->bounds();

  // Now attach a View to the widget first, then create a layer for it. Make
  // sure the bounds are correct.
  View* v2 = new View;
  v2->SetBounds(50, 10, 30, 10);
  EXPECT_FALSE(v2->layer());
  view->AddChildView(v2);
  v2->SetPaintToLayer();
  EXPECT_EQ(gfx::Rect(content_width - 80, 10, 30, 10), v2->layer()->bounds());
  gfx::Rect l2bounds = v2->layer()->bounds();

  view->SetPaintToLayer();
  EXPECT_EQ(l1bounds, v1->layer()->bounds());
  EXPECT_EQ(l2bounds, v2->layer()->bounds());

  // Move one of the views. Make sure the layer is positioned correctly
  // afterwards.
  v1->SetBounds(v1->x() - 5, v1->y(), v1->width(), v1->height());
  l1bounds.set_x(l1bounds.x() + 5);
  EXPECT_EQ(l1bounds, v1->layer()->bounds());

  view->DestroyLayer();
  EXPECT_EQ(l1bounds, v1->layer()->bounds());
  EXPECT_EQ(l2bounds, v2->layer()->bounds());

  // Move a view again.
  v2->SetBounds(v2->x() + 5, v2->y(), v2->width(), v2->height());
  l2bounds.set_x(l2bounds.x() - 5);
  EXPECT_EQ(l2bounds, v2->layer()->bounds());
}

// Make sure that resizing a parent in RTL correctly repositions its children.
TEST_F(ViewLayerTest, ResizeParentInRTL) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");
  View* view = widget()->SetContentsView(std::make_unique<View>());

  int content_width = view->width();

  // Create a paints-to-layer view |v1|.
  View* v1 = view->AddChildView(std::make_unique<View>());
  v1->SetPaintToLayer();
  v1->SetBounds(10, 10, 20, 10);
  EXPECT_EQ(gfx::Rect(content_width - 30, 10, 20, 10), v1->layer()->bounds());

  // Attach a paints-to-layer child view to |v1|.
  View* v2 = new View;
  v2->SetPaintToLayer();
  v2->SetBounds(3, 5, 6, 4);
  EXPECT_EQ(gfx::Rect(3, 5, 6, 4), v2->layer()->bounds());
  v1->AddChildView(v2);
  // Check that |v2| now has RTL-appropriate bounds.
  EXPECT_EQ(gfx::Rect(11, 5, 6, 4), v2->layer()->bounds());

  // Attach a non-layer child view to |v1|, and give it a paints-to-layer child.
  View* v3 = new View;
  v3->SetBounds(1, 1, 18, 8);
  View* v4 = new View;
  v4->SetPaintToLayer();
  v4->SetBounds(2, 4, 6, 4);
  EXPECT_EQ(gfx::Rect(2, 4, 6, 4), v4->layer()->bounds());
  v3->AddChildView(v4);
  EXPECT_EQ(gfx::Rect(10, 4, 6, 4), v4->layer()->bounds());
  v1->AddChildView(v3);
  // Check that |v4| now has RTL-appropriate bounds.
  EXPECT_EQ(gfx::Rect(11, 5, 6, 4), v4->layer()->bounds());

  // Resize |v1|. Make sure that |v2| and |v4|'s layers have been moved
  // correctly to RTL-appropriate bounds.
  v1->SetSize(gfx::Size(30, 10));
  EXPECT_EQ(gfx::Rect(21, 5, 6, 4), v2->layer()->bounds());
  EXPECT_EQ(gfx::Rect(21, 5, 6, 4), v4->layer()->bounds());

  // Move and resize |v3|. Make sure that |v4|'s layer has been moved correctly
  // to RTL-appropriate bounds.
  v3->SetBounds(2, 1, 12, 8);
  EXPECT_EQ(gfx::Rect(20, 5, 6, 4), v4->layer()->bounds());
}

// Makes sure a transform persists after toggling the visibility.
TEST_F(ViewLayerTest, ToggleVisibilityWithTransform) {
  View* view = widget()->SetContentsView(std::make_unique<View>());
  gfx::Transform transform;
  transform.Scale(2.0, 2.0);
  view->SetTransform(transform);
  EXPECT_EQ(2.0f, view->GetTransform().rc(0, 0));

  view->SetVisible(false);
  EXPECT_EQ(2.0f, view->GetTransform().rc(0, 0));

  view->SetVisible(true);
  EXPECT_EQ(2.0f, view->GetTransform().rc(0, 0));
}

// Verifies a transform persists after removing/adding a view with a transform.
TEST_F(ViewLayerTest, ResetTransformOnLayerAfterAdd) {
  View* view = widget()->SetContentsView(std::make_unique<View>());
  gfx::Transform transform;
  transform.Scale(2.0, 2.0);
  view->SetTransform(transform);
  EXPECT_EQ(2.0f, view->GetTransform().rc(0, 0));
  ASSERT_TRUE(view->layer() != nullptr);
  EXPECT_EQ(2.0f, view->layer()->transform().rc(0, 0));

  View* parent = view->parent();
  parent->RemoveChildView(view);
  parent->AddChildView(view);

  EXPECT_EQ(2.0f, view->GetTransform().rc(0, 0));
  ASSERT_TRUE(view->layer() != nullptr);
  EXPECT_EQ(2.0f, view->layer()->transform().rc(0, 0));
}

// Makes sure that layer visibility is correct after toggling View visibility.
TEST_F(ViewLayerTest, ToggleVisibilityWithLayer) {
  View* content_view = widget()->SetContentsView(std::make_unique<View>());

  // The view isn't attached to a widget or a parent view yet. But it should
  // still have a layer, but the layer should not be attached to the root
  // layer.
  View* v1 = new View;
  v1->SetPaintToLayer();
  EXPECT_TRUE(v1->layer());
  EXPECT_FALSE(
      LayerIsAncestor(widget()->GetCompositor()->root_layer(), v1->layer()));

  // Once the view is attached to a widget, its layer should be attached to the
  // root layer and visible.
  content_view->AddChildView(v1);
  EXPECT_TRUE(
      LayerIsAncestor(widget()->GetCompositor()->root_layer(), v1->layer()));
  EXPECT_TRUE(v1->layer()->IsVisible());

  v1->SetVisible(false);
  EXPECT_FALSE(v1->layer()->IsVisible());

  v1->SetVisible(true);
  EXPECT_TRUE(v1->layer()->IsVisible());

  widget()->Hide();
  EXPECT_FALSE(v1->layer()->IsVisible());

  widget()->Show();
  EXPECT_TRUE(v1->layer()->IsVisible());
}

// Tests that the layers in the subtree are orphaned after a View is removed
// from the parent.
TEST_F(ViewLayerTest, OrphanLayerAfterViewRemove) {
  View* content_view = widget()->SetContentsView(std::make_unique<View>());

  View* v1 = new View;
  content_view->AddChildView(v1);

  View* v2 = new View;
  v1->AddChildView(v2);
  v2->SetPaintToLayer();
  EXPECT_TRUE(
      LayerIsAncestor(widget()->GetCompositor()->root_layer(), v2->layer()));
  EXPECT_TRUE(v2->layer()->IsVisible());

  content_view->RemoveChildView(v1);

  EXPECT_FALSE(
      LayerIsAncestor(widget()->GetCompositor()->root_layer(), v2->layer()));

  // Reparent |v2|.
  v1->RemoveChildView(v2);
  content_view->AddChildView(v2);
  delete v1;
  v1 = nullptr;
  EXPECT_TRUE(
      LayerIsAncestor(widget()->GetCompositor()->root_layer(), v2->layer()));
  EXPECT_TRUE(v2->layer()->IsVisible());
}

class PaintTrackingView : public View {
  METADATA_HEADER(PaintTrackingView, View)

 public:
  PaintTrackingView() = default;

  PaintTrackingView(const PaintTrackingView&) = delete;
  PaintTrackingView& operator=(const PaintTrackingView&) = delete;

  bool painted() const { return painted_; }
  void set_painted(bool value) { painted_ = value; }

  void OnPaint(gfx::Canvas* canvas) override { painted_ = true; }

 private:
  bool painted_ = false;
};

BEGIN_METADATA(PaintTrackingView)
END_METADATA

// Makes sure child views with layers aren't painted when paint starts at an
// ancestor.
TEST_F(ViewLayerTest, DontPaintChildrenWithLayers) {
  PaintTrackingView* content_view =
      widget()->SetContentsView(std::make_unique<PaintTrackingView>());
  content_view->SetPaintToLayer();
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::WaitForCompositingEnded(
      GetRootLayer()->GetCompositor());
  GetRootLayer()->SchedulePaint(gfx::Rect(0, 0, 10, 10));
  content_view->set_painted(false);
  // content_view no longer has a dirty rect. Paint from the root and make sure
  // PaintTrackingView isn't painted.
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::WaitForCompositingEnded(
      GetRootLayer()->GetCompositor());
  EXPECT_FALSE(content_view->painted());

  // Make content_view have a dirty rect, paint the layers and make sure
  // PaintTrackingView is painted.
  content_view->layer()->SchedulePaint(gfx::Rect(0, 0, 10, 10));
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::WaitForCompositingEnded(
      GetRootLayer()->GetCompositor());
  EXPECT_TRUE(content_view->painted());
}

TEST_F(ViewLayerTest, NoCrashWhenParentlessViewSchedulesPaintOnParent) {
  TestView v;
  SchedulePaintOnParent(&v);
}

TEST_F(ViewLayerTest, ScheduledRectsInParentAfterSchedulingPaint) {
  TestView parent_view;
  parent_view.SetBounds(10, 10, 100, 100);

  TestView* child_view = parent_view.AddChildView(std::make_unique<TestView>());
  child_view->SetBounds(5, 6, 10, 20);

  parent_view.scheduled_paint_rects_.clear();
  SchedulePaintOnParent(child_view);
  ASSERT_EQ(1U, parent_view.scheduled_paint_rects_.size());
  EXPECT_EQ(gfx::Rect(5, 6, 10, 20),
            parent_view.scheduled_paint_rects_.front());
}

TEST_F(ViewLayerTest, ParentPaintWhenSwitchingPaintToLayerFromFalseToTrue) {
  TestView parent_view;
  parent_view.SetBounds(10, 11, 12, 13);

  TestView* child_view = parent_view.AddChildView(std::make_unique<TestView>());

  parent_view.scheduled_paint_rects_.clear();
  child_view->SetPaintToLayer();
  EXPECT_EQ(1U, parent_view.scheduled_paint_rects_.size());
}

TEST_F(ViewLayerTest, NoParentPaintWhenSwitchingPaintToLayerFromTrueToTrue) {
  TestView parent_view;
  parent_view.SetBounds(10, 11, 12, 13);

  TestView* child_view = parent_view.AddChildView(std::make_unique<TestView>());
  child_view->SetPaintToLayer();

  parent_view.scheduled_paint_rects_.clear();
  EXPECT_EQ(0U, parent_view.scheduled_paint_rects_.size());
}

// Tests that the visibility of child layers are updated correctly when a View's
// visibility changes.
TEST_F(ViewLayerTest, VisibilityChildLayers) {
  View* v1 = widget()->SetContentsView(std::make_unique<View>());
  v1->SetPaintToLayer();

  View* v2 = v1->AddChildView(std::make_unique<View>());

  View* v3 = v2->AddChildView(std::make_unique<View>());
  v3->SetVisible(false);

  View* v4 = v3->AddChildView(std::make_unique<View>());
  v4->SetPaintToLayer();

  EXPECT_TRUE(v1->layer()->IsVisible());
  EXPECT_FALSE(v4->layer()->IsVisible());

  v2->SetVisible(false);
  EXPECT_TRUE(v1->layer()->IsVisible());
  EXPECT_FALSE(v4->layer()->IsVisible());

  v2->SetVisible(true);
  EXPECT_TRUE(v1->layer()->IsVisible());
  EXPECT_FALSE(v4->layer()->IsVisible());

  v2->SetVisible(false);
  EXPECT_TRUE(v1->layer()->IsVisible());
  EXPECT_FALSE(v4->layer()->IsVisible());
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(v1, v1->layer()));

  v3->SetVisible(true);
  EXPECT_TRUE(v1->layer()->IsVisible());
  EXPECT_FALSE(v4->layer()->IsVisible());
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(v1, v1->layer()));

  // Reparent |v3| to |v1|.
  v2->RemoveChildView(v3);
  v1->AddChildView(v3);
  EXPECT_TRUE(v1->layer()->IsVisible());
  EXPECT_TRUE(v4->layer()->IsVisible());
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(v1, v1->layer()));
}

// This test creates a random View tree, and then randomly reorders child views,
// reparents views etc. Unrelated changes can appear to break this test. So
// marking this as FLAKY.
TEST_F(ViewLayerTest, DISABLED_ViewLayerTreesInSync) {
  View* content = widget()->SetContentsView(std::make_unique<View>());
  content->SetPaintToLayer();
  widget()->Show();

  ConstructTree(content, 5);
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(content, content->layer()));

  ScrambleTree(content);
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(content, content->layer()));

  ScrambleTree(content);
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(content, content->layer()));

  ScrambleTree(content);
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(content, content->layer()));
}

// Verifies when views are reordered the layer is also reordered. The widget is
// providing the parent layer.
TEST_F(ViewLayerTest, ReorderUnderWidget) {
  View* content = widget()->SetContentsView(std::make_unique<View>());
  View* c1 = content->AddChildView(std::make_unique<View>());
  c1->SetPaintToLayer();
  View* c2 = content->AddChildView(std::make_unique<View>());
  c2->SetPaintToLayer();

  ui::Layer* parent_layer = c1->layer()->parent();
  ASSERT_TRUE(parent_layer);
  ASSERT_EQ(2u, parent_layer->children().size());
  EXPECT_EQ(c1->layer(), parent_layer->children()[0]);
  EXPECT_EQ(c2->layer(), parent_layer->children()[1]);

  // Move c1 to the front. The layers should have moved too.
  content->ReorderChildView(c1, content->children().size());
  EXPECT_EQ(c1->layer(), parent_layer->children()[1]);
  EXPECT_EQ(c2->layer(), parent_layer->children()[0]);
}

// Verifies that the layer of a view can be acquired properly.
TEST_F(ViewLayerTest, AcquireLayer) {
  View* content = widget()->SetContentsView(std::make_unique<View>());
  std::unique_ptr<View> c1(new View);
  c1->SetPaintToLayer();
  EXPECT_TRUE(c1->layer());
  content->AddChildView(c1.get());

  std::unique_ptr<ui::Layer> layer(c1->AcquireLayer());
  EXPECT_EQ(layer.get(), c1->layer());

  std::unique_ptr<ui::Layer> layer2(c1->RecreateLayer());
  EXPECT_NE(c1->layer(), layer2.get());

  // Destroy view before destroying layer.
  c1.reset();
}

// Verify the z-order of the layers as a result of calling RecreateLayer().
TEST_F(ViewLayerTest, RecreateLayerZOrder) {
  std::unique_ptr<View> v(new View());
  v->SetPaintToLayer();

  View* v1 = v->AddChildView(std::make_unique<View>());
  v1->SetPaintToLayer();
  View* v2 = v->AddChildView(std::make_unique<View>());
  v2->SetPaintToLayer();

  // Test the initial z-order.
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>>& child_layers_pre =
      v->layer()->children();
  ASSERT_EQ(2u, child_layers_pre.size());
  EXPECT_EQ(v1->layer(), child_layers_pre[0]);
  EXPECT_EQ(v2->layer(), child_layers_pre[1]);

  std::unique_ptr<ui::Layer> v1_old_layer(v1->RecreateLayer());

  // Test the new layer order. We expect: |v1| |v1_old_layer| |v2|.
  // for |v1| and |v2|.
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>>& child_layers_post =
      v->layer()->children();
  ASSERT_EQ(3u, child_layers_post.size());
  EXPECT_EQ(v1->layer(), child_layers_post[0]);
  EXPECT_EQ(v1_old_layer.get(), child_layers_post[1]);
  EXPECT_EQ(v2->layer(), child_layers_post[2]);
}

// Verify the z-order of the layers as a result of calling RecreateLayer when
// the widget is the parent with the layer.
TEST_F(ViewLayerTest, RecreateLayerZOrderWidgetParent) {
  View* v = widget()->SetContentsView(std::make_unique<View>());

  View* v1 = v->AddChildView(std::make_unique<View>());
  v1->SetPaintToLayer();
  View* v2 = v->AddChildView(std::make_unique<View>());
  v2->SetPaintToLayer();

  ui::Layer* root_layer = GetRootLayer();

  // Test the initial z-order.
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>>& child_layers_pre =
      root_layer->children();
  ASSERT_EQ(2u, child_layers_pre.size());
  EXPECT_EQ(v1->layer(), child_layers_pre[0]);
  EXPECT_EQ(v2->layer(), child_layers_pre[1]);

  std::unique_ptr<ui::Layer> v1_old_layer(v1->RecreateLayer());

  // Test the new layer order. We expect: |v1| |v1_old_layer| |v2|.
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>>& child_layers_post =
      root_layer->children();
  ASSERT_EQ(3u, child_layers_post.size());
  EXPECT_EQ(v1->layer(), child_layers_post[0]);
  EXPECT_EQ(v1_old_layer.get(), child_layers_post[1]);
  EXPECT_EQ(v2->layer(), child_layers_post[2]);
}

// Verifies RecreateLayer() moves all Layers over, even those that don't have
// a View.
TEST_F(ViewLayerTest, RecreateLayerMovesNonViewChildren) {
  View v;
  v.SetPaintToLayer();
  View child;
  child.SetPaintToLayer();
  v.AddChildView(&child);
  ASSERT_TRUE(v.layer() != nullptr);
  ASSERT_EQ(1u, v.layer()->children().size());
  EXPECT_EQ(v.layer()->children()[0], child.layer());

  ui::Layer layer(ui::LAYER_NOT_DRAWN);
  v.layer()->Add(&layer);
  v.layer()->StackAtBottom(&layer);

  std::unique_ptr<ui::Layer> old_layer(v.RecreateLayer());

  // All children should be moved from old layer to new layer.
  ASSERT_TRUE(old_layer.get() != nullptr);
  EXPECT_TRUE(old_layer->children().empty());

  // And new layer should have the two children.
  ASSERT_TRUE(v.layer() != nullptr);
  ASSERT_EQ(2u, v.layer()->children().size());
  EXPECT_EQ(v.layer()->children()[0], &layer);
  EXPECT_EQ(v.layer()->children()[1], child.layer());
}

namespace {

std::string ToString(const gfx::Vector2dF& vector) {
  // Explicitly round it because linux uses banker's rounding
  // while Windows is using "away-from-zero" in printf.
  return base::StringPrintf("%0.2f %0.2f", std::round(vector.x() * 100) / 100.f,
                            std::round(vector.y() * 100) / 100.f);
}

}  // namespace

TEST_F(ViewLayerTest, SnapLayerToPixel) {
  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  View* v1 = widget()->SetContentsView(std::make_unique<View>());

  View* v11 = v1->AddChildView(std::make_unique<View>());

  const gfx::Size& size = GetRootLayer()->GetCompositor()->size();
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      1.25f, size, allocator.GetCurrentLocalSurfaceId());

  v11->SetBoundsRect(gfx::Rect(1, 1, 10, 10));
  v1->SetBoundsRect(gfx::Rect(1, 1, 10, 10));
  v11->SetPaintToLayer();

  EXPECT_EQ("0.40 0.40", ToString(v11->layer()->GetSubpixelOffset()));

  // Creating a layer in parent should update the child view's layer offset.
  v1->SetPaintToLayer();
  EXPECT_EQ("-0.20 -0.20", ToString(v1->layer()->GetSubpixelOffset()));
  EXPECT_EQ("-0.20 -0.20", ToString(v11->layer()->GetSubpixelOffset()));

  // DSF change should get propagated and update offsets.
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      1.5f, size, allocator.GetCurrentLocalSurfaceId());
  EXPECT_EQ("0.33 0.33", ToString(v1->layer()->GetSubpixelOffset()));
  EXPECT_EQ("0.33 0.33", ToString(v11->layer()->GetSubpixelOffset()));

  // Deleting parent's layer should update the child view's layer's offset.
  v1->DestroyLayer();
  EXPECT_EQ("0.00 0.00", ToString(v11->layer()->GetSubpixelOffset()));

  // Setting parent view should update the child view's layer's offset.
  v1->SetBoundsRect(gfx::Rect(2, 2, 10, 10));
  EXPECT_EQ("0.33 0.33", ToString(v11->layer()->GetSubpixelOffset()));

  // Setting integral DSF should reset the offset.
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      2.0f, size, allocator.GetCurrentLocalSurfaceId());
  EXPECT_EQ("0.00 0.00", ToString(v11->layer()->GetSubpixelOffset()));

  // DSF reset followed by DSF change should update the offset.
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      1.0f, size, allocator.GetCurrentLocalSurfaceId());
  EXPECT_EQ("0.00 0.00", ToString(v11->layer()->GetSubpixelOffset()));
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      1.5f, size, allocator.GetCurrentLocalSurfaceId());
  EXPECT_EQ("0.33 0.33", ToString(v11->layer()->GetSubpixelOffset()));
}

TEST_F(ViewLayerTest, LayerBeneathTriggersPaintToLayer) {
  View root;
  root.SetPaintToLayer();

  View* view = root.AddChildView(std::make_unique<View>());
  EXPECT_EQ(nullptr, view->layer());

  ui::Layer layer1;
  ui::Layer layer2;
  view->AddLayerToRegion(&layer1, LayerRegion::kBelow);
  EXPECT_NE(nullptr, view->layer());
  view->AddLayerToRegion(&layer2, LayerRegion::kBelow);
  EXPECT_NE(nullptr, view->layer());

  view->RemoveLayerFromRegions(&layer1);
  EXPECT_NE(nullptr, view->layer());
  view->RemoveLayerFromRegions(&layer2);
  EXPECT_EQ(nullptr, view->layer());
}

TEST_F(ViewLayerTest, LayerBeneathAddedToTree) {
  View root;
  root.SetPaintToLayer();

  ui::Layer layer;
  View* view = root.AddChildView(std::make_unique<View>());

  view->AddLayerToRegion(&layer, LayerRegion::kBelow);
  ASSERT_NE(nullptr, view->layer());
  EXPECT_TRUE(view->layer()->parent()->Contains(&layer));

  view->RemoveLayerFromRegions(&layer);
  EXPECT_EQ(nullptr, layer.parent());
}

TEST_F(ViewLayerTest, LayerBeneathAtFractionalScale) {
  constexpr float device_scale = 1.5f;

  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  const gfx::Size& size = GetRootLayer()->GetCompositor()->size();
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      device_scale, size, allocator.GetCurrentLocalSurfaceId());

  View* view = widget()->SetContentsView(std::make_unique<View>());

  ui::Layer layer;
  view->AddLayerToRegion(&layer, LayerRegion::kBelow);

  view->SetBoundsRect(gfx::Rect(1, 1, 10, 10));
  EXPECT_NE(gfx::Vector2dF(), view->layer()->GetSubpixelOffset());
  EXPECT_EQ(view->layer()->GetSubpixelOffset(), layer.GetSubpixelOffset());

  view->RemoveLayerFromRegions(&layer);
}

TEST_F(ViewLayerTest, LayerBeneathRemovedOnDestruction) {
  View root;
  root.SetPaintToLayer();

  auto layer = std::make_unique<ui::Layer>();
  View* view = root.AddChildView(std::make_unique<View>());

  // No assertions, just get coverage of deleting the layer while it is added.
  view->AddLayerToRegion(layer.get(), LayerRegion::kBelow);
  layer.reset();
  root.RemoveChildView(view);
  delete view;
}

TEST_F(ViewLayerTest, LayerBeneathVisibilityUpdated) {
  View root;
  root.SetPaintToLayer();

  ui::Layer layer;

  // Make a parent view that has no layer, and a child view that has a layer.
  View* parent = root.AddChildView(std::make_unique<View>());
  View* child = parent->AddChildView(std::make_unique<View>());
  child->AddLayerToRegion(&layer, LayerRegion::kBelow);

  EXPECT_EQ(nullptr, parent->layer());
  EXPECT_NE(nullptr, child->layer());

  // Test setting the views' visbilities in various orders.
  EXPECT_TRUE(layer.visible());
  child->SetVisible(false);
  EXPECT_FALSE(layer.visible());
  child->SetVisible(true);
  EXPECT_TRUE(layer.visible());

  parent->SetVisible(false);
  EXPECT_FALSE(layer.visible());
  parent->SetVisible(true);
  EXPECT_TRUE(layer.visible());

  parent->SetVisible(false);
  EXPECT_FALSE(layer.visible());
  child->SetVisible(false);
  EXPECT_FALSE(layer.visible());
  parent->SetVisible(true);
  EXPECT_FALSE(layer.visible());
  child->SetVisible(true);
  EXPECT_TRUE(layer.visible());

  child->RemoveLayerFromRegions(&layer);

  // Now check the visibility upon adding.
  child->SetVisible(false);
  child->AddLayerToRegion(&layer, LayerRegion::kBelow);
  EXPECT_FALSE(layer.visible());
  child->SetVisible(true);
  EXPECT_TRUE(layer.visible());

  child->RemoveLayerFromRegions(&layer);
}

TEST_F(ViewLayerTest, LayerBeneathHasCorrectBounds) {
  View root;
  root.SetBoundsRect(gfx::Rect(100, 100));
  root.SetPaintToLayer();

  View* view = root.AddChildView(std::make_unique<View>());
  view->SetBoundsRect(gfx::Rect(25, 25, 50, 50));

  // The layer's position will be changed, but its size should be respected.
  ui::Layer layer;
  layer.SetBounds(gfx::Rect(25, 25));

  // First check when |view| is already painting to a layer.
  view->SetPaintToLayer();
  view->AddLayerToRegion(&layer, LayerRegion::kBelow);
  EXPECT_NE(nullptr, layer.parent());
  EXPECT_EQ(gfx::Rect(25, 25, 25, 25), layer.bounds());

  view->RemoveLayerFromRegions(&layer);
  EXPECT_EQ(nullptr, layer.parent());
  layer.SetBounds(gfx::Rect(25, 25));

  // Next check when |view| wasn't painting to a layer.
  view->DestroyLayer();
  EXPECT_EQ(nullptr, view->layer());
  view->AddLayerToRegion(&layer, LayerRegion::kBelow);
  EXPECT_NE(nullptr, view->layer());
  EXPECT_NE(nullptr, layer.parent());
  EXPECT_EQ(gfx::Rect(25, 25, 25, 25), layer.bounds());

  // Finally check that moving |view| also moves the layer.
  view->SetBoundsRect(gfx::Rect(50, 50, 50, 50));
  EXPECT_EQ(gfx::Rect(50, 50, 25, 25), layer.bounds());

  view->RemoveLayerFromRegions(&layer);
}

TEST_F(ViewLayerTest, LayerBeneathTransformed) {
  View root;
  root.SetPaintToLayer();

  ui::Layer layer;
  View* view = root.AddChildView(std::make_unique<View>());
  view->SetPaintToLayer();
  view->AddLayerToRegion(&layer, LayerRegion::kBelow);
  EXPECT_TRUE(layer.transform().IsIdentity());

  gfx::Transform transform;
  transform.Rotate(90);
  view->SetTransform(transform);
  EXPECT_EQ(transform, layer.transform());
  view->SetTransform(gfx::Transform());
  EXPECT_TRUE(layer.transform().IsIdentity());
}

TEST_F(ViewLayerTest, UpdateChildLayerVisibilityEvenIfLayer) {
  View root;
  root.SetPaintToLayer();

  View* view = root.AddChildView(std::make_unique<View>());
  view->SetPaintToLayer();
  View* child = view->AddChildView(std::make_unique<View>());
  child->SetPaintToLayer();
  EXPECT_TRUE(child->layer()->GetAnimator()->GetTargetVisibility());

  // Makes the view invisible then destroy the layer.
  view->SetVisible(false);
  view->DestroyLayer();
  EXPECT_FALSE(child->layer()->GetAnimator()->GetTargetVisibility());

  view->SetVisible(true);
  view->SetPaintToLayer();
  EXPECT_TRUE(child->layer()->GetAnimator()->GetTargetVisibility());

  // Destroys the layer then make the view invisible.
  view->DestroyLayer();
  view->SetVisible(false);
  EXPECT_FALSE(child->layer()->GetAnimator()->GetTargetVisibility());
}

TEST_F(ViewLayerTest, LayerBeneathStackedCorrectly) {
  using ui::test::ChildLayerNamesAsString;

  View root;
  root.SetPaintToLayer();

  ui::Layer layer;
  layer.SetName("layer");

  View* v1 = root.AddChildView(std::make_unique<View>());
  View* v2 = root.AddChildView(std::make_unique<View>());
  View* v3 = root.AddChildView(std::make_unique<View>());

  // Check that |layer| is stacked correctly as we add more layers to the tree.
  v2->AddLayerToRegion(&layer, LayerRegion::kBelow);
  v2->layer()->SetName("v2");
  EXPECT_EQ(ChildLayerNamesAsString(*root.layer()), "layer v2");
  v3->SetPaintToLayer();
  v3->layer()->SetName("v3");
  EXPECT_EQ(ChildLayerNamesAsString(*root.layer()), "layer v2 v3");
  v1->SetPaintToLayer();
  v1->layer()->SetName("v1");
  EXPECT_EQ(ChildLayerNamesAsString(*root.layer()), "v1 layer v2 v3");

  v2->RemoveLayerFromRegions(&layer);
}

TEST_F(ViewLayerTest, LayerBeneathOrphanedOnRemoval) {
  View root;
  root.SetPaintToLayer();

  ui::Layer layer;
  View* view = root.AddChildView(std::make_unique<View>());
  view->AddLayerToRegion(&layer, LayerRegion::kBelow);
  EXPECT_EQ(layer.parent(), root.layer());

  // Ensure that the layer beneath is orphaned and re-parented appropriately.
  root.RemoveChildView(view);
  EXPECT_EQ(layer.parent(), nullptr);
  root.AddChildView(view);
  EXPECT_EQ(layer.parent(), root.layer());

  view->RemoveLayerFromRegions(&layer);
}

TEST_F(ViewLayerTest, LayerBeneathMovedWithView) {
  using ui::test::ChildLayerNamesAsString;

  View root;
  root.SetPaintToLayer();
  root.layer()->SetName("root");

  ui::Layer layer;
  layer.SetName("layer");

  View* v1 = root.AddChildView(std::make_unique<View>());
  View* v2 = root.AddChildView(std::make_unique<View>());
  View* v3 = v1->AddChildView(std::make_unique<View>());

  v1->SetPaintToLayer();
  v1->layer()->SetName("v1");
  v2->SetPaintToLayer();
  v2->layer()->SetName("v2");
  v3->SetPaintToLayer();
  v3->layer()->SetName("v3");

  // Verify that |layer| is stacked correctly.
  v3->AddLayerToRegion(&layer, LayerRegion::kBelow);
  EXPECT_EQ(ChildLayerNamesAsString(*v1->layer()), "layer v3");

  // Move |v3| to under |v2| and check |layer|'s stacking.
  v1->RemoveChildView(v3);
  v2->AddChildView(std::unique_ptr<View>(v3));
  EXPECT_EQ(ChildLayerNamesAsString(*v2->layer()), "layer v3");
}

namespace {

class PaintLayerView : public View {
  METADATA_HEADER(PaintLayerView, View)

 public:
  PaintLayerView() = default;

  PaintLayerView(const PaintLayerView&) = delete;
  PaintLayerView& operator=(const PaintLayerView&) = delete;

  void PaintChildren(const PaintInfo& info) override {
    last_paint_info_ = std::make_unique<PaintInfo>(info);
    View::PaintChildren(info);
  }

  std::unique_ptr<PaintInfo> GetLastPaintInfo() {
    return std::move(last_paint_info_);
  }

 private:
  std::unique_ptr<PaintInfo> last_paint_info_;
};

BEGIN_METADATA(PaintLayerView)
END_METADATA

}  // namespace

class ViewLayerPixelCanvasTest : public ViewLayerTest {
 public:
  ViewLayerPixelCanvasTest() = default;

  ViewLayerPixelCanvasTest(const ViewLayerPixelCanvasTest&) = delete;
  ViewLayerPixelCanvasTest& operator=(const ViewLayerPixelCanvasTest&) = delete;

  ~ViewLayerPixelCanvasTest() override = default;

  void SetUpPixelCanvas() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kEnablePixelCanvasRecording);
  }

  // Test if the recording rects are same with and without layer.
  void PaintRecordingSizeTest(PaintLayerView* v3,
                              const gfx::Size& expected_size) {
    v3->DestroyLayer();
    ui::Compositor* compositor = widget()->GetCompositor();
    auto list = base::MakeRefCounted<cc::DisplayItemList>();
    ui::PaintContext context(list.get(), compositor->device_scale_factor(),
                             gfx::Rect(compositor->size()), true);
    widget()->GetRootView()->PaintFromPaintRoot(context);
    EXPECT_EQ(expected_size, v3->GetLastPaintInfo()->paint_recording_size());
    v3->SetPaintToLayer();
    v3->OnPaintLayer(context);
    EXPECT_EQ(expected_size, v3->GetLastPaintInfo()->paint_recording_size());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ViewLayerPixelCanvasTest, SnapLayerToPixel) {
  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  View* v1 = widget()->SetContentsView(std::make_unique<View>());
  View* v2 = v1->AddChildView(std::make_unique<View>());
  PaintLayerView* v3 = v2->AddChildView(std::make_unique<PaintLayerView>());

  const gfx::Size& size = GetRootLayer()->GetCompositor()->size();
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      1.6f, size, allocator.GetCurrentLocalSurfaceId());

  v3->SetBoundsRect(gfx::Rect(14, 13, 13, 5));
  v2->SetBoundsRect(gfx::Rect(7, 7, 50, 50));
  v1->SetBoundsRect(gfx::Rect(9, 9, 100, 100));

  PaintRecordingSizeTest(v3, gfx::Size(21, 8));  // Enclosing Rect = (21, 8)
  EXPECT_EQ("-0.63 -0.25", ToString(v3->layer()->GetSubpixelOffset()));

  // Creating a layer in parent should update the child view's layer offset.
  v1->SetPaintToLayer();
  EXPECT_EQ("-0.25 -0.25", ToString(v1->layer()->GetSubpixelOffset()));
  EXPECT_EQ("-0.37 -0.00", ToString(v3->layer()->GetSubpixelOffset()));

  // DSF change should get propagated and update offsets.
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      1.5f, size, allocator.GetCurrentLocalSurfaceId());

  EXPECT_EQ("0.33 0.33", ToString(v1->layer()->GetSubpixelOffset()));
  EXPECT_EQ("0.33 0.67", ToString(v3->layer()->GetSubpixelOffset()));

  v1->DestroyLayer();
  PaintRecordingSizeTest(v3, gfx::Size(20, 7));  // Enclosing Rect = (20, 8)
  v1->SetPaintToLayer();

  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      1.33f, size, allocator.GetCurrentLocalSurfaceId());

  EXPECT_EQ("0.02 0.02", ToString(v1->layer()->GetSubpixelOffset()));
  EXPECT_EQ("0.05 -0.45", ToString(v3->layer()->GetSubpixelOffset()));

  v1->DestroyLayer();
  PaintRecordingSizeTest(v3, gfx::Size(17, 7));  // Enclosing Rect = (18, 7)

  // Deleting parent's layer should update the child view's layer's offset.
  EXPECT_EQ("0.08 -0.43", ToString(v3->layer()->GetSubpixelOffset()));

  // Setting parent view should update the child view's layer's offset.
  v1->SetBoundsRect(gfx::Rect(3, 3, 10, 10));
  EXPECT_EQ("0.06 -0.44", ToString(v3->layer()->GetSubpixelOffset()));

  // Setting integral DSF should reset the offset.
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      2.0f, size, allocator.GetCurrentLocalSurfaceId());
  EXPECT_EQ("0.00 0.00", ToString(v3->layer()->GetSubpixelOffset()));

  // DSF reset followed by DSF change should update the offset.
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      1.0f, size, allocator.GetCurrentLocalSurfaceId());
  EXPECT_EQ("0.00 0.00", ToString(v3->layer()->GetSubpixelOffset()));
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      1.33f, size, allocator.GetCurrentLocalSurfaceId());
  EXPECT_EQ("0.06 -0.44", ToString(v3->layer()->GetSubpixelOffset()));
}

TEST_F(ViewLayerPixelCanvasTest, LayerBeneathOnPixelCanvas) {
  constexpr float device_scale = 1.5f;

  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  const gfx::Size& size = GetRootLayer()->GetCompositor()->size();
  GetRootLayer()->GetCompositor()->SetScaleAndSize(
      device_scale, size, allocator.GetCurrentLocalSurfaceId());

  View* view = widget()->SetContentsView(std::make_unique<View>());

  ui::Layer layer;
  view->AddLayerToRegion(&layer, LayerRegion::kBelow);

  view->SetBoundsRect(gfx::Rect(1, 1, 10, 10));
  EXPECT_NE(gfx::Vector2dF(), view->layer()->GetSubpixelOffset());
  EXPECT_EQ(view->layer()->GetSubpixelOffset(), layer.GetSubpixelOffset());

  view->RemoveLayerFromRegions(&layer);
}

TEST_F(ViewTest, FocusableAssertions) {
  // View subclasses may change insets based on whether they are focusable,
  // which effects the preferred size. To avoid preferred size changing around
  // these Views need to key off the last value set to SetFocusBehavior(), not
  // whether the View is focusable right now. For this reason it's important
  // that the return value of GetFocusBehavior() depends on the last value
  // passed to SetFocusBehavior and not whether the View is focusable right now.
  TestView view;
  view.SetFocusBehavior(View::FocusBehavior::ALWAYS);
  EXPECT_EQ(View::FocusBehavior::ALWAYS, view.GetFocusBehavior());
  view.SetEnabled(false);
  EXPECT_EQ(View::FocusBehavior::ALWAYS, view.GetFocusBehavior());
  view.SetFocusBehavior(View::FocusBehavior::NEVER);
  EXPECT_EQ(View::FocusBehavior::NEVER, view.GetFocusBehavior());
  view.SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  EXPECT_EQ(View::FocusBehavior::ACCESSIBLE_ONLY, view.GetFocusBehavior());
}

////////////////////////////////////////////////////////////////////////////////
// NativeTheme
////////////////////////////////////////////////////////////////////////////////

TEST_F(ViewTest, OnThemeChanged) {
  auto test_view = std::make_unique<TestView>();
  EXPECT_FALSE(test_view->native_theme_);

  // Child view added before the widget hierarchy exists should get the
  // new native theme notification.
  TestView* test_view_child =
      test_view->AddChildView(std::make_unique<TestView>());
  EXPECT_FALSE(test_view_child->native_theme_);

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));

  TestView* test_view_ptr =
      widget->GetRootView()->AddChildView(std::move(test_view));
  EXPECT_TRUE(test_view_ptr->native_theme_);
  EXPECT_EQ(widget->GetNativeTheme(), test_view_ptr->native_theme_);
  EXPECT_TRUE(test_view_child->native_theme_);
  EXPECT_EQ(widget->GetNativeTheme(), test_view_child->native_theme_);

  // Child view added after the widget hierarchy exists should also get the
  // notification.
  TestView* test_view_child_2 =
      test_view_ptr->AddChildView(std::make_unique<TestView>());
  EXPECT_TRUE(test_view_child_2->native_theme_);
  EXPECT_EQ(widget->GetNativeTheme(), test_view_child_2->native_theme_);
}

class TestEventHandler : public ui::EventHandler {
 public:
  explicit TestEventHandler(TestView* view) : view_(view) {}
  ~TestEventHandler() override = default;

  void OnMouseEvent(ui::MouseEvent* event) override {
    // The |view_| should have received the event first.
    EXPECT_EQ(ui::EventType::kMousePressed, view_->last_mouse_event_type_);
    had_mouse_event_ = true;
  }

  raw_ptr<TestView> view_;
  bool had_mouse_event_ = false;
};

TEST_F(ViewTest, ScopedTargetHandlerReceivesEvents) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 350, 350);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();
  TestView* v = root->AddChildView(std::make_unique<TestView>());
  v->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  v->Reset();
  {
    TestEventHandler handler(v);
    ui::ScopedTargetHandler scoped_target_handler(v, &handler);
    // View's target EventHandler should be set to the |scoped_target_handler|.
    EXPECT_EQ(&scoped_target_handler,
              v->SetTargetHandler(&scoped_target_handler));

    EXPECT_EQ(v->last_mouse_event_type_, ui::EventType::kUnknown);
    gfx::Point p(10, 120);
    ui::MouseEvent pressed(ui::EventType::kMousePressed, p, p,
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    root->OnMousePressed(pressed);

    // Both the View |v| and the |handler| should have received the event.
    EXPECT_EQ(ui::EventType::kMousePressed, v->last_mouse_event_type_);
    EXPECT_TRUE(handler.had_mouse_event_);
  }

  // The View should continue receiving events after the |handler| is deleted.
  v->Reset();
  ui::MouseEvent released(ui::EventType::kMouseReleased, gfx::Point(),
                          gfx::Point(), ui::EventTimeForNow(), 0, 0);
  root->OnMouseReleased(released);
  EXPECT_EQ(ui::EventType::kMouseReleased, v->last_mouse_event_type_);
}

// See comment above test for details.
class WidgetWithCustomTheme : public Widget {
 public:
  explicit WidgetWithCustomTheme(ui::TestNativeTheme* theme) : theme_(theme) {}

  WidgetWithCustomTheme(const WidgetWithCustomTheme&) = delete;
  WidgetWithCustomTheme& operator=(const WidgetWithCustomTheme&) = delete;

  ~WidgetWithCustomTheme() override = default;

  // Widget:
  const ui::NativeTheme* GetNativeTheme() const override { return theme_; }

 private:
  raw_ptr<ui::TestNativeTheme> theme_;
};

// See comment above test for details.
class ViewThatAddsViewInOnThemeChanged : public View {
  METADATA_HEADER(ViewThatAddsViewInOnThemeChanged, View)

 public:
  ViewThatAddsViewInOnThemeChanged() { SetPaintToLayer(); }

  ViewThatAddsViewInOnThemeChanged(const ViewThatAddsViewInOnThemeChanged&) =
      delete;
  ViewThatAddsViewInOnThemeChanged& operator=(
      const ViewThatAddsViewInOnThemeChanged&) = delete;

  ~ViewThatAddsViewInOnThemeChanged() override = default;

  bool on_native_theme_changed_called() const {
    return on_native_theme_changed_called_;
  }

  // View:
  void OnThemeChanged() override {
    View::OnThemeChanged();
    on_native_theme_changed_called_ = true;
    GetWidget()->GetRootView()->AddChildView(std::make_unique<View>());
  }

 private:
  bool on_native_theme_changed_called_ = false;
};

BEGIN_METADATA(ViewThatAddsViewInOnThemeChanged)
END_METADATA

// Creates and adds a new child view to |parent| that has a layer.
void AddViewWithChildLayer(View* parent) {
  View* child = parent->AddChildView(std::make_unique<View>());
  child->SetPaintToLayer();
}

// This test does the following:
// . creates a couple of views with layers added to the root.
// . Add a view that overrides OnThemeChanged(). In OnThemeChanged() another
// view is added. This sequence triggered DCHECKs or crashes previously. This
// tests verifies that doesn't happen. Reason for crash was OnThemeChanged() was
// called before the layer hierarchy was updated. OnThemeChanged() should be
// called after the layer hierarchy matches the view hierarchy.
TEST_F(ViewTest, CrashOnAddFromFromOnThemeChanged) {
  ui::TestNativeTheme theme;
  auto widget = std::make_unique<WidgetWithCustomTheme>(&theme);
  test::WidgetDestroyedWaiter waiter(widget.get());
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 350, 350);
  widget->Init(std::move(params));

  AddViewWithChildLayer(widget->GetRootView());
  ViewThatAddsViewInOnThemeChanged* v = widget->GetRootView()->AddChildView(
      std::make_unique<ViewThatAddsViewInOnThemeChanged>());
  EXPECT_TRUE(v->on_native_theme_changed_called());
  // Initiate an explicit close and wait to ensure the |theme| outlives the
  // |widget|.
  widget->Close();
  waiter.Wait();
}

// A View that removes its Layer when hidden.
class NoLayerWhenHiddenView : public View {
  METADATA_HEADER(NoLayerWhenHiddenView, View)

 public:
  using RemovedFromWidgetCallback = base::OnceCallback<void()>;
  explicit NoLayerWhenHiddenView(RemovedFromWidgetCallback removed_from_widget)
      : removed_from_widget_(std::move(removed_from_widget)) {
    SetPaintToLayer();
    SetBounds(0, 0, 100, 100);
  }

  NoLayerWhenHiddenView(const NoLayerWhenHiddenView&) = delete;
  NoLayerWhenHiddenView& operator=(const NoLayerWhenHiddenView&) = delete;

  bool was_hidden() const { return was_hidden_; }

  // View:
  void VisibilityChanged(View* starting_from, bool is_visible) override {
    if (!is_visible) {
      was_hidden_ = true;
      DestroyLayer();
    }
  }

  void RemovedFromWidget() override {
    if (removed_from_widget_)
      std::move(removed_from_widget_).Run();
  }

 private:
  bool was_hidden_ = false;
  RemovedFromWidgetCallback removed_from_widget_;
};

BEGIN_METADATA(NoLayerWhenHiddenView)
END_METADATA

// Test that Views can safely manipulate Layers during Widget closure.
TEST_F(ViewTest, DestroyLayerInClose) {
  bool removed_from_widget = false;
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));
  widget->SetBounds(gfx::Rect(0, 0, 100, 100));
  auto* view = widget->GetContentsView()->AddChildView(
      std::make_unique<NoLayerWhenHiddenView>(base::BindOnce(
          [](bool* removed_from_widget) { *removed_from_widget = true; },
          &removed_from_widget)));
  widget->Show();

  EXPECT_TRUE(view->layer());
  EXPECT_TRUE(view->GetWidget());
  EXPECT_FALSE(view->was_hidden());

  // Close the widget. It will be destroyed once it goes out of scope.
  widget->Close();
  EXPECT_FALSE(view->layer());
  // Ensure the layer went away via VisibilityChanged().
  EXPECT_TRUE(view->was_hidden());

  // Not removed from Widget until Close() completes.
  EXPECT_FALSE(removed_from_widget);
  base::RunLoop().RunUntilIdle();  // Let the Close() complete.
  // Once Close() is completed, destroy the Widget.
  widget.reset();
  EXPECT_TRUE(removed_from_widget);
}

// A View that keeps the children with a special ID above other children.
class OrderableView : public View {
  METADATA_HEADER(OrderableView, View)

 public:
  // ID used by the children that are stacked above other children.
  static constexpr int VIEW_ID_RAISED = 1000;

  OrderableView() = default;

  OrderableView(const OrderableView&) = delete;
  OrderableView& operator=(const OrderableView&) = delete;

  ~OrderableView() override = default;

  View::Views GetChildrenInZOrder() override {
    View::Views children_in_z_order = children();
    std::stable_partition(
        children_in_z_order.begin(), children_in_z_order.end(),
        [](const View* child) { return child->GetID() != VIEW_ID_RAISED; });
    return children_in_z_order;
  }
};

BEGIN_METADATA(OrderableView)
END_METADATA

TEST_F(ViewTest, ChildViewZOrderChanged) {
  const size_t kNumChildren = 4;
  auto view = std::make_unique<OrderableView>();
  view->SetPaintToLayer();
  for (size_t i = 0; i < kNumChildren; ++i)
    AddViewWithChildLayer(view.get());
  View::Views children = view->GetChildrenInZOrder();
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>>& layers =
      view->layer()->children();
  ASSERT_EQ(kNumChildren, children.size());
  ASSERT_EQ(kNumChildren, layers.size());
  for (size_t i = 0; i < kNumChildren; ++i) {
    EXPECT_EQ(view->children()[i], children[i]);
    EXPECT_EQ(view->children()[i]->layer(), layers[i]);
  }

  // Raise one of the children in z-order and add another child to reorder.
  view->children()[2]->SetID(OrderableView::VIEW_ID_RAISED);
  AddViewWithChildLayer(view.get());

  // 2nd child should be now on top, i.e. the last element in the array returned
  // by GetChildrenInZOrder(). Its layer should also be above the others.
  // The rest of the children and layers order should be unchanged.
  constexpr auto expected_order = std::to_array<size_t>({0, 1, 3, 4, 2});
  children = view->GetChildrenInZOrder();
  EXPECT_EQ(kNumChildren + 1, children.size());
  EXPECT_EQ(kNumChildren + 1, layers.size());
  for (size_t i = 0; i < kNumChildren + 1; ++i) {
    EXPECT_EQ(view->children()[expected_order[i]], children[i]);
    EXPECT_EQ(view->children()[expected_order[i]]->layer(), layers[i]);
  }
}

TEST_F(ViewTest, AttachChildViewWithComplicatedLayers) {
  std::unique_ptr<View> grand_parent_view = std::make_unique<View>();
  grand_parent_view->SetPaintToLayer();

  auto parent_view = std::make_unique<OrderableView>();
  parent_view->SetPaintToLayer();

  // child_view1 has layer and has id OrderableView::VIEW_ID_RAISED.
  View* child_view1 = parent_view->AddChildView(std::make_unique<View>());
  child_view1->SetPaintToLayer();
  child_view1->SetID(OrderableView::VIEW_ID_RAISED);

  // child_view2 has no layer.
  View* child_view2 = parent_view->AddChildView(std::make_unique<View>());
  // grand_child_view has layer.
  View* grand_child_view = child_view2->AddChildView(std::make_unique<View>());
  grand_child_view->SetPaintToLayer();
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>>& layers =
      parent_view->layer()->children();
  EXPECT_EQ(2u, layers.size());
  EXPECT_EQ(layers[0], grand_child_view->layer());
  EXPECT_EQ(layers[1], child_view1->layer());

  // Attach parent_view to grand_parent_view. children layers of parent_view
  // should not change.
  OrderableView* parent_view_ptr =
      grand_parent_view->AddChildView(std::move(parent_view));
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>>&
      layers_after_attached = parent_view_ptr->layer()->children();
  EXPECT_EQ(2u, layers_after_attached.size());
  EXPECT_EQ(layers_after_attached[0], grand_child_view->layer());
  EXPECT_EQ(layers_after_attached[1], child_view1->layer());
}

TEST_F(ViewTest, TestEnabledPropertyMetadata) {
  auto test_view = std::make_unique<View>();
  bool enabled_changed = false;
  auto subscription = test_view->AddEnabledChangedCallback(base::BindRepeating(
      [](bool* enabled_changed) { *enabled_changed = true; },
      &enabled_changed));
  ui::metadata::ClassMetaData* view_metadata = View::MetaData();
  ASSERT_TRUE(view_metadata);
  ui::metadata::MemberMetaDataBase* enabled_property =
      view_metadata->FindMemberData("Enabled");
  ASSERT_TRUE(enabled_property);
  std::u16string false_value = u"false";
  enabled_property->SetValueAsString(test_view.get(), false_value);
  EXPECT_TRUE(enabled_changed);
  EXPECT_FALSE(test_view->GetEnabled());
  EXPECT_EQ(enabled_property->GetValueAsString(test_view.get()), false_value);
}

TEST_F(ViewTest, TestMarginsPropertyMetadata) {
  auto test_view = std::make_unique<View>();
  ui::metadata::ClassMetaData* view_metadata = View::MetaData();
  ASSERT_TRUE(view_metadata);
  ui::metadata::MemberMetaDataBase* insets_property =
      view_metadata->FindMemberData("kMarginsKey");
  ASSERT_TRUE(insets_property);
  std::u16string insets_value = u"8,8,8,8";
  insets_property->SetValueAsString(test_view.get(), insets_value);
  EXPECT_EQ(insets_property->GetValueAsString(test_view.get()), insets_value);
}

TEST_F(ViewTest, TestEnabledChangedCallback) {
  auto test_view = std::make_unique<View>();
  bool enabled_changed = false;
  auto subscription = test_view->AddEnabledChangedCallback(base::BindRepeating(
      [](bool* enabled_changed) { *enabled_changed = true; },
      &enabled_changed));
  test_view->SetEnabled(false);
  EXPECT_TRUE(enabled_changed);
  EXPECT_FALSE(test_view->GetEnabled());
}

TEST_F(ViewTest, TestVisibleChangedCallback) {
  auto test_view = std::make_unique<View>();
  bool visibility_changed = false;
  auto subscription = test_view->AddVisibleChangedCallback(base::BindRepeating(
      [](bool* visibility_changed) { *visibility_changed = true; },
      &visibility_changed));
  test_view->SetVisible(false);
  EXPECT_TRUE(visibility_changed);
  EXPECT_FALSE(test_view->GetVisible());
}

TEST_F(ViewTest, TooltipShowsForDisabledView) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));
  widget->SetBounds(gfx::Rect(0, 0, 100, 100));

  View enabled_parent;
  View* const disabled_child =
      enabled_parent.AddChildView(std::make_unique<View>());
  disabled_child->SetEnabled(false);

  enabled_parent.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  disabled_child->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  widget->GetContentsView()->AddChildView(&enabled_parent);
  widget->Show();

  EXPECT_EQ(disabled_child,
            enabled_parent.GetTooltipHandlerForPoint(gfx::Point(50, 50)));
}

TEST_F(ViewTest, DefaultFocusListIsInChildOrder) {
  View parent;
  View* const first = parent.AddChildView(std::make_unique<View>());

  EXPECT_EQ(first->GetPreviousFocusableView(), nullptr);
  EXPECT_EQ(first->GetNextFocusableView(), nullptr);

  View* const second = parent.AddChildView(std::make_unique<View>());

  EXPECT_EQ(first->GetPreviousFocusableView(), nullptr);
  EXPECT_EQ(first->GetNextFocusableView(), second);
  EXPECT_EQ(second->GetPreviousFocusableView(), first);
  EXPECT_EQ(second->GetNextFocusableView(), nullptr);

  View* const last = parent.AddChildView(std::make_unique<View>());

  EXPECT_EQ(first->GetPreviousFocusableView(), nullptr);
  EXPECT_EQ(first->GetNextFocusableView(), second);
  EXPECT_EQ(second->GetPreviousFocusableView(), first);
  EXPECT_EQ(second->GetNextFocusableView(), last);
  EXPECT_EQ(last->GetPreviousFocusableView(), second);
  EXPECT_EQ(last->GetNextFocusableView(), nullptr);
}

TEST_F(ViewTest, RemoveFromFocusList) {
  View parent;
  View* const first = parent.AddChildView(std::make_unique<View>());
  View* const second = parent.AddChildView(std::make_unique<View>());
  View* const last = parent.AddChildView(std::make_unique<View>());

  second->RemoveFromFocusList();
  EXPECT_EQ(second->GetPreviousFocusableView(), nullptr);
  EXPECT_EQ(second->GetNextFocusableView(), nullptr);
  EXPECT_EQ(first->GetNextFocusableView(), last);
  EXPECT_EQ(last->GetPreviousFocusableView(), first);
}

TEST_F(ViewTest, RemoveChildUpdatesFocusList) {
  View parent;

  View* const first = parent.AddChildView(std::make_unique<View>());
  View* const second = parent.AddChildView(std::make_unique<View>());
  View* const last = parent.AddChildView(std::make_unique<View>());

  std::unique_ptr<View> removed = parent.RemoveChildViewT(second);

  EXPECT_EQ(removed->GetPreviousFocusableView(), nullptr);
  EXPECT_EQ(removed->GetNextFocusableView(), nullptr);
  EXPECT_EQ(first->GetNextFocusableView(), last);
  EXPECT_EQ(last->GetPreviousFocusableView(), first);
}

TEST_F(ViewTest, RemoveAllChildViewsNullsFocusListPointers) {
  View parent;

  View* const first = parent.AddChildView(std::make_unique<View>());
  View* const second = parent.AddChildView(std::make_unique<View>());
  View* const last = parent.AddChildView(std::make_unique<View>());

  parent.RemoveAllChildViewsWithoutDeleting();

  EXPECT_EQ(first->GetPreviousFocusableView(), nullptr);
  EXPECT_EQ(first->GetNextFocusableView(), nullptr);
  EXPECT_EQ(second->GetPreviousFocusableView(), nullptr);
  EXPECT_EQ(second->GetNextFocusableView(), nullptr);
  EXPECT_EQ(last->GetPreviousFocusableView(), nullptr);
  EXPECT_EQ(last->GetNextFocusableView(), nullptr);

  // The former child views must be deleted manually since the
  // RemoveAllChildViews released ownership.
  delete first;
  delete second;
  delete last;
}

TEST_F(ViewTest, InsertBeforeInFocusList) {
  View parent;
  View* const v1 = parent.AddChildView(std::make_unique<View>());
  View* const v2 = parent.AddChildView(std::make_unique<View>());
  View* const v3 = parent.AddChildView(std::make_unique<View>());

  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v1, v2, v3));

  v2->InsertBeforeInFocusList(v1);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v2, v1, v3));

  v3->InsertBeforeInFocusList(v1);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v2, v3, v1));

  v1->InsertBeforeInFocusList(v2);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v1, v2, v3));

  v1->InsertBeforeInFocusList(v3);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v2, v1, v3));

  v1->InsertBeforeInFocusList(v3);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v2, v1, v3));
}

TEST_F(ViewTest, InsertAfterInFocusList) {
  View parent;
  View* const v1 = parent.AddChildView(std::make_unique<View>());
  View* const v2 = parent.AddChildView(std::make_unique<View>());
  View* const v3 = parent.AddChildView(std::make_unique<View>());

  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v1, v2, v3));

  v1->InsertAfterInFocusList(v2);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v2, v1, v3));

  v1->InsertAfterInFocusList(v3);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v2, v3, v1));

  v2->InsertAfterInFocusList(v1);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v3, v1, v2));

  v3->InsertAfterInFocusList(v2);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v1, v2, v3));

  v1->InsertAfterInFocusList(v3);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v2, v3, v1));

  v1->InsertAfterInFocusList(v3);
  EXPECT_THAT(parent.GetChildrenFocusList(), ElementsAre(v2, v3, v1));
}

////////////////////////////////////////////////////////////////////////////////
// Observer tests.
////////////////////////////////////////////////////////////////////////////////

class TestViewObserver : public ViewObserver {
 public:
  explicit TestViewObserver(std::vector<View*> views) {
    for (auto* view : views) {
      observations_.AddObservation(view);
    }
  }

  void AddObservation(View* view) { observations_.AddObservation(view); }

  ~TestViewObserver() override = default;

  // ViewObserver:
  void OnChildViewAdded(View* parent, View* child) override {
    child_view_added_times_++;
    child_view_added_ = child;
    child_view_added_parent_ = parent;
  }
  void OnChildViewRemoved(View* parent, View* child) override {
    child_view_removed_times_++;
    child_view_removed_ = child;
    child_view_removed_parent_ = parent;
  }

  void OnViewVisibilityChanged(View* view, View* starting_view) override {
    view_visibility_changed_ = view;
    view_visibility_changed_starting_ = starting_view;
  }

  void OnViewBoundsChanged(View* view) override { view_bounds_changed_ = view; }

  void OnChildViewReordered(View* parent, View* view) override {
    view_reordered_ = view;
  }

  void OnViewHierarchyWillBeDeleted(View* observed_view) override {
    on_view_hierarchy_will_be_deleted_called_ = true;
    if (verify_on_view_hierarchy_will_be_deleted_callback_) {
      std::move(verify_on_view_hierarchy_will_be_deleted_callback_).Run();
    }
  }

  void OnViewIsDeleting(View* observed_view) override {
    observations_.RemoveObservation(observed_view);
  }

  void SetVerifyOnViewHierarchyWillBeDeletedCallback(
      base::OnceClosure verify_on_view_hierarchy_will_be_deleted_callback) {
    verify_on_view_hierarchy_will_be_deleted_callback_ =
        std::move(verify_on_view_hierarchy_will_be_deleted_callback);
  }

  int child_view_added_times() { return child_view_added_times_; }
  int child_view_removed_times() { return child_view_removed_times_; }
  const View* child_view_added() const { return child_view_added_; }
  const View* child_view_added_parent() const {
    return child_view_added_parent_;
  }

  bool has_on_view_hierarchy_will_be_deleted_called() {
    return on_view_hierarchy_will_be_deleted_called_;
  }

  const View* child_view_removed() const { return child_view_removed_; }
  const View* child_view_removed_parent() const {
    return child_view_removed_parent_;
  }
  const View* view_visibility_changed() const {
    return view_visibility_changed_;
  }
  const View* view_visibility_changed_starting() const {
    return view_visibility_changed_starting_;
  }
  const View* view_bounds_changed() const { return view_bounds_changed_; }
  const View* view_reordered() const { return view_reordered_; }

 private:
  base::ScopedMultiSourceObservation<View, ViewObserver> observations_{this};

  int child_view_added_times_ = 0;
  int child_view_removed_times_ = 0;
  bool on_view_hierarchy_will_be_deleted_called_ = false;
  base::OnceClosure verify_on_view_hierarchy_will_be_deleted_callback_;

  raw_ptr<View> child_view_added_parent_ = nullptr;
  raw_ptr<View> child_view_added_ = nullptr;
  raw_ptr<View> child_view_removed_ = nullptr;
  raw_ptr<View> child_view_removed_parent_ = nullptr;
  raw_ptr<View> view_visibility_changed_ = nullptr;
  raw_ptr<View> view_visibility_changed_starting_ = nullptr;
  raw_ptr<View> view_bounds_changed_ = nullptr;
  raw_ptr<View> view_reordered_ = nullptr;
};

using ViewObserverTest = ViewTest;

TEST_F(ViewObserverTest, ViewHierarchyWillBeDeleted) {
  TestViewObserver observer({});
  {
    auto parent = std::make_unique<View>();
    View* main_view = parent->AddChildView(std::make_unique<View>());
    TestView* child_view =
        main_view->AddChildView(std::make_unique<TestView>());
    observer.AddObservation(main_view);
    child_view->SetDestructionCallback(base::BindOnce(
        [](TestViewObserver& observer) {
          EXPECT_TRUE(observer.has_on_view_hierarchy_will_be_deleted_called());
        },
        std::ref(observer)));
    observer.SetVerifyOnViewHierarchyWillBeDeletedCallback(base::BindOnce(
        [](View* main_view, View* parent_view) {
          EXPECT_EQ(parent_view->children().size(), 1u);
          EXPECT_EQ(main_view->children().size(), 1u);
        },
        base::Unretained(main_view), base::Unretained(parent.get())));
  }
}

TEST_F(ViewObserverTest, ViewParentChanged) {
  auto parent1 = std::make_unique<View>();
  auto parent2 = std::make_unique<View>();
  auto child = std::make_unique<View>();
  View* weak_child = child.get();

  {
    TestViewObserver observer({parent1.get(), parent2.get(), weak_child});
    parent1->AddChildView(std::move(child));
    EXPECT_EQ(0, observer.child_view_removed_times());
    EXPECT_EQ(1, observer.child_view_added_times());
    EXPECT_EQ(weak_child, observer.child_view_added());
    EXPECT_EQ(weak_child->parent(), observer.child_view_added_parent());
    EXPECT_EQ(weak_child->parent(), parent1.get());
  }

  // Removed from parent1, added to parent2
  {
    TestViewObserver observer({parent1.get(), parent2.get(), weak_child});
    parent2->AddChildView(parent1->RemoveChildViewT(weak_child));
    EXPECT_EQ(1, observer.child_view_removed_times());
    EXPECT_EQ(1, observer.child_view_added_times());
    EXPECT_EQ(weak_child, observer.child_view_removed());
    EXPECT_EQ(parent1.get(), observer.child_view_removed_parent());
    EXPECT_EQ(weak_child, observer.child_view_added());
    EXPECT_EQ(weak_child->parent(), parent2.get());
  }

  {
    TestViewObserver observer({parent1.get(), parent2.get(), weak_child});
    child = parent2->RemoveChildViewT(weak_child);
    EXPECT_EQ(1, observer.child_view_removed_times());
    EXPECT_EQ(0, observer.child_view_added_times());
    EXPECT_EQ(weak_child, observer.child_view_removed());
    EXPECT_EQ(parent2.get(), observer.child_view_removed_parent());
  }
}

TEST_F(ViewObserverTest, ViewVisibilityChanged) {
  auto parent = std::make_unique<View>();
  View* weak_child = parent->AddChildView(std::make_unique<View>());

  // Ensure setting |view| itself not visible calls the observer.
  {
    TestViewObserver observer({weak_child});
    weak_child->SetVisible(false);
    EXPECT_EQ(weak_child, observer.view_visibility_changed());
    EXPECT_EQ(weak_child, observer.view_visibility_changed_starting());
  }

  // Ditto for setting it visible.
  {
    TestViewObserver observer({weak_child});
    weak_child->SetVisible(true);
    EXPECT_EQ(weak_child, observer.view_visibility_changed());
    EXPECT_EQ(weak_child, observer.view_visibility_changed_starting());
  }

  // Ensure setting |parent| not visible also calls the
  // observer. |view->GetVisible()| should still return true however.
  {
    TestViewObserver observer({weak_child});
    parent->SetVisible(false);
    EXPECT_EQ(weak_child, observer.view_visibility_changed());
    EXPECT_EQ(parent.get(), observer.view_visibility_changed_starting());
  }
}

TEST_F(ViewObserverTest, ViewBoundsChanged) {
  auto view = std::make_unique<View>();

  {
    TestViewObserver observer({view.get()});
    gfx::Rect bounds(2, 2, 2, 2);
    view->SetBoundsRect(bounds);
    EXPECT_EQ(view.get(), observer.view_bounds_changed());
    EXPECT_EQ(bounds, view->bounds());
  }

  {
    TestViewObserver observer({view.get()});
    gfx::Rect new_bounds(1, 1, 1, 1);
    view->SetBoundsRect(new_bounds);
    EXPECT_EQ(view.get(), observer.view_bounds_changed());
    EXPECT_EQ(new_bounds, view->bounds());
  }
}

TEST_F(ViewObserverTest, ChildViewReordered) {
  auto parent = std::make_unique<View>();
  parent->AddChildView(std::make_unique<View>());
  auto* child = parent->AddChildView(std::make_unique<View>());

  {
    TestViewObserver observer({parent.get()});
    parent->ReorderChildView(child, 0);
    EXPECT_EQ(child, observer.view_reordered());
  }
}

class MockViewObserver : public ViewObserver {
 public:
  // ViewObserver:
  MOCK_METHOD(void,
              OnViewPropertyChanged,
              (View * observed_view, const void* key, int64_t old_value),
              (override));
};

ACTION_TEMPLATE(ExpectThatViewProperty,
                HAS_1_TEMPLATE_PARAMS(typename, T),
                AND_1_VALUE_PARAMS(Matcher)) {
  EXPECT_THAT(ui::ClassPropertyCaster<T*>::FromInt64(arg0), Matcher);
}

TEST_F(ViewObserverTest, ViewPropertyChanged) {
  auto view = std::make_unique<View>();

  // Observe `view`.
  StrictMock<MockViewObserver> view_observer;
  base::ScopedObservation<View, ViewObserver> view_observation(&view_observer);
  view_observation.Observe(view.get());

  constexpr auto kNewValue = gfx::Insets::TLBR(1, 2, 3, 4);

  // Expect that setting `kNewValue` to the `kMarginsKey` will notify observers.
  // Because a property at `kMarginsKey` was not previously set, the `old_value`
  // should be a `nullptr`.
  EXPECT_CALL(
      view_observer,
      OnViewPropertyChanged(Eq(view.get()), Eq(kMarginsKey), /*old_value*/ _))
      .WillOnce(WithArg<2>(ExpectThatViewProperty<gfx::Insets>(IsNull())));

  // Set `kNewValue` to `kMarginsKey` and verify expectations.
  view->SetProperty(kMarginsKey, kNewValue);
  Mock::VerifyAndClearExpectations(&view_observer);
  EXPECT_THAT(view->GetProperty(kMarginsKey), Pointee(Eq(kNewValue)));

  constexpr auto kPreviousValue = kNewValue;
  constexpr auto kAnotherNewValue = gfx::Insets::TLBR(5, 6, 7, 8);

  // Expect that setting `kAnotherNewValue` to the `kMarginsKey` will notify
  // observers. Because a property at `kMarginsKey` was previously set, the
  // `old_value` should point to the `kPreviousValue`.
  EXPECT_CALL(
      view_observer,
      OnViewPropertyChanged(Eq(view.get()), Eq(kMarginsKey), /*old_value=*/_))
      .WillOnce(WithArg<2>(
          ExpectThatViewProperty<gfx::Insets>(Pointee(Eq(kPreviousValue)))));

  // Set `kAnotherNewValue` to `kMarginsKey` and verify expectations.
  view->SetProperty(kMarginsKey, kAnotherNewValue);
  Mock::VerifyAndClearExpectations(&view_observer);
  EXPECT_THAT(view->GetProperty(kMarginsKey), Pointee(Eq(kAnotherNewValue)));
}

// Provides a simple parent view implementation which tracks layer change
// notifications from child views.
class TestParentView : public View {
  METADATA_HEADER(TestParentView, View)

 public:
  TestParentView() = default;

  TestParentView(const TestParentView&) = delete;
  TestParentView& operator=(const TestParentView&) = delete;

  void Reset() {
    received_layer_change_notification_ = false;
    layer_change_count_ = 0;
  }

  bool received_layer_change_notification() const {
    return received_layer_change_notification_;
  }

  int layer_change_count() const { return layer_change_count_; }

  // View overrides.
  void OnChildLayerChanged(View* child) override {
    received_layer_change_notification_ = true;
    layer_change_count_++;
  }

 private:
  // Set to true if we receive the OnChildLayerChanged() notification for a
  // child.
  bool received_layer_change_notification_ = false;

  // Contains the number of OnChildLayerChanged() notifications for a child.
  int layer_change_count_ = 0;
};

BEGIN_METADATA(TestParentView)
END_METADATA

// Tests the following cases.
// 1. We receive the OnChildLayerChanged() notification when a layer change
//    occurs in a child view.
// 2. We don't receive two layer changes when a child with an existing layer
//    creates a new layer.
TEST_F(ViewObserverTest, ChildViewLayerNotificationTest) {
  auto parent = std::make_unique<TestParentView>();
  auto* child = parent->AddChildView(std::make_unique<View>());

  EXPECT_FALSE(parent->received_layer_change_notification());
  EXPECT_EQ(0, parent->layer_change_count());

  child->SetPaintToLayer(ui::LAYER_TEXTURED);
  EXPECT_TRUE(parent->received_layer_change_notification());
  EXPECT_EQ(1, parent->layer_change_count());

  parent->Reset();
  child->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  EXPECT_TRUE(parent->received_layer_change_notification());
  EXPECT_EQ(1, parent->layer_change_count());
}

namespace {

// This view always resizes the associated layer when bounds change.
class LayerResizingView : public View {
  METADATA_HEADER(LayerResizingView, View)

 public:
  explicit LayerResizingView(ui::Layer* layer) : layer_(layer) {}

 private:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    // Drop the coordinate since the layer should always be aligned.
    gfx::Rect layer_rect = gfx::Rect(size());
    layer_->SetBounds(layer_rect);
  }

  raw_ptr<ui::Layer> layer_;
};

BEGIN_METADATA(LayerResizingView)
END_METADATA

}  // namespace

// Confirms that the size of a View and the size of a region-attached layer stay
// in sync.
TEST(ViewTestUnfixtured, ViewLayerSizeStayInSync) {
  // Make a layer with implicit animations.
  std::unique_ptr<ui::Layer> region_layer = std::make_unique<ui::Layer>();
  region_layer->SetAnimator(ui::LayerAnimator::CreateImplicitAnimator());

  // Make a view, attach the layer to a region. The view keeps the bounds of the
  // layer in sync. See implementation of LayerResizingView::OnBoundsChanged().
  std::unique_ptr<View> view_owned =
      std::make_unique<LayerResizingView>(region_layer.get());
  view_owned->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  view_owned->AddLayerToRegion(region_layer.get(), views::LayerRegion::kBelow);
  raw_ptr<View> view = view_owned.get();

  // Make a parent view. All it does is keep the child view the same size.
  std::unique_ptr<View> parent_view = std::make_unique<View>();
  parent_view->AddChildView(std::move(view_owned));
  parent_view->SetUseDefaultFillLayout(true);

  // Initial conditions: everything has 0 width.
  EXPECT_EQ(0, view->width());
  EXPECT_EQ(0, region_layer->bounds().width());
  EXPECT_EQ(0, region_layer->GetTargetBounds().width());

  // Setting bounds on the parent view will propagate the size to the child
  // view. The child view then propagates the size to its layer. Note that the
  // layer's bounds are not immediately updated as the animation has not yet
  // started. Instead, the layer's target bounds is updated.
  gfx::Rect bounds = gfx::Rect(0, 0, 60, 80);
  parent_view->SetBoundsRect(bounds);
  EXPECT_EQ(60, view->width());
  EXPECT_EQ(0, region_layer->bounds().width());
  EXPECT_EQ(60, region_layer->GetTargetBounds().width());

  // Now we move the parent view without changing the size. This does not
  // propagate a size change to the child view. However, it does cause the layer
  // to reset its target bounds to its current bounds.
  gfx::Rect new_bounds = bounds;
  new_bounds.set_x(10);
  parent_view->SetBoundsRect(new_bounds);
  EXPECT_EQ(60, view->width());
  EXPECT_EQ(0, region_layer->bounds().width());

  // This is the expected behavior: target bounds does not change.
  // EXPECT_EQ(60, region_layer->GetTargetBounds().width());

  // This is the broken behavior: target bounds is set to the current value of
  // bounds.
  EXPECT_EQ(0, region_layer->GetTargetBounds().width());

  view = nullptr;
}

TEST_F(ViewTest, CompleteAXCacheInitializationOnChildViewAddedWithAXOn) {
  const ::ui::ScopedAXModeSetter ax_mode_setter(ui::AXMode::kNativeAPIs);
  auto parent = std::make_unique<View>();
  auto* child = parent->AddChildView(std::make_unique<View>());
  EXPECT_TRUE(child->GetViewAccessibility().is_initialized());
}

TEST_F(ViewTest, DoNotCompleteAXCacheInitializationOnChildViewAddedWithAXOff) {
  auto parent = std::make_unique<View>();
  auto* child = parent->AddChildView(std::make_unique<View>());

  // TODO(https://crbug.com/325137417): We should only complete the
  // initialization of the accessible cache when we know an accessibility API
  // client fetches information from the browser. We currently don't. Change
  // EXPECT_TRUE to EXPECT_FALSE when we do.
  EXPECT_TRUE(child->GetViewAccessibility().is_initialized());
}

using BaseActionViewInterfaceTest = ViewsTestBase;

TEST_F(BaseActionViewInterfaceTest, TestActionChanged) {
  auto action_view = std::make_unique<View>();
  std::unique_ptr<actions::ActionItem> action_item =
      actions::ActionItem::Builder().SetActionId(0).SetEnabled(false).Build();
  action_view->GetActionViewInterface()->ActionItemChangedImpl(
      action_item.get());
  // Test some properties to ensure that the right ActionViewInterface is linked
  // to the view.
  EXPECT_FALSE(action_view->GetEnabled());
}

}  // namespace views
