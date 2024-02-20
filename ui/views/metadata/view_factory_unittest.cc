// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/view_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

using ViewFactoryTest = views::test::WidgetTest;

namespace internal {

class TestView : public views::View {
  METADATA_HEADER(TestView, views::View)

 public:
  TestView() = default;
  TestView(const TestView&) = delete;
  TestView& operator=(const TestView&) = delete;
  ~TestView() override = default;

  // Define a method with an arbitrary number of parameters (>1).
  // Most PODs and enums should work. Stick to parameters passed by-value
  // if possible.
  // TODO(kylixrd): Figure out support for ref and const-ref parameters
  //                  if ever needed
  void ArbitraryMethod(int int_param,
                       float float_param,
                       views::PropertyEffects property_effects) {
    int_param_ = int_param;
    float_param_ = float_param;
    property_effects_ = property_effects;
  }

  int get_int_param() const { return int_param_; }
  float get_float_param() const { return float_param_; }
  views::PropertyEffects get_property_effects() const {
    return property_effects_;
  }

 private:
  int int_param_ = 0;
  float float_param_ = 0.0;
  views::PropertyEffects property_effects_ = views::kPropertyEffectsNone;
};

BEGIN_VIEW_BUILDER(, TestView, views::View)
VIEW_BUILDER_METHOD(ArbitraryMethod, int, float, views::PropertyEffects)
END_VIEW_BUILDER

BEGIN_METADATA(TestView)
END_METADATA

}  // namespace internal

DEFINE_VIEW_BUILDER(, ::internal::TestView)

TEST_F(ViewFactoryTest, TestViewBuilder) {
  views::View* parent = nullptr;
  views::LabelButton* button = nullptr;
  views::LabelButton* scroll_button = nullptr;
  views::ScrollView* scroll_view = nullptr;
  views::View* view_with_layout_manager = nullptr;
  auto layout_manager = std::make_unique<views::FillLayout>();
  auto* layout_manager_ptr = layout_manager.get();
  auto view =
      views::Builder<views::View>()
          .CopyAddressTo(&parent)
          .SetEnabled(false)
          .SetVisible(true)
          .SetBackground(views::CreateSolidBackground(SK_ColorWHITE))
          .SetBorder(views::CreateEmptyBorder(0))
          .AddChildren(views::Builder<views::View>()
                           .SetEnabled(false)
                           .SetVisible(true)
                           .SetProperty(views::kMarginsKey, gfx::Insets(5)),
                       views::Builder<views::View>()
                           .SetGroup(5)
                           .SetID(1)
                           .SetFocusBehavior(views::View::FocusBehavior::NEVER),
                       views::Builder<views::LabelButton>()
                           .CopyAddressTo(&button)
                           .SetIsDefault(true)
                           .SetEnabled(true)
                           .SetText(u"Test"),
                       views::Builder<views::ScrollView>()
                           .CopyAddressTo(&scroll_view)
                           .SetContents(views::Builder<views::LabelButton>()
                                            .CopyAddressTo(&scroll_button)
                                            .SetText(u"ScrollTest"))
                           .SetHeader(views::Builder<views::View>().SetID(2)),
                       views::Builder<views::LabelButton>()
                           .CopyAddressTo(&view_with_layout_manager)
                           .SetLayoutManager(std::move(layout_manager)))
          .Build();
  ASSERT_TRUE(view.get());
  EXPECT_NE(parent, nullptr);
  EXPECT_NE(button, nullptr);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->GetEnabled());
  ASSERT_GT(view->children().size(), size_t{2});
  EXPECT_EQ(view->children()[1]->GetFocusBehavior(),
            views::View::FocusBehavior::NEVER);
  EXPECT_EQ(view->children()[2], button);
  EXPECT_EQ(button->GetText(), u"Test");
  EXPECT_NE(scroll_view, nullptr);
  EXPECT_NE(scroll_button, nullptr);
  EXPECT_EQ(scroll_button->GetText(), u"ScrollTest");
  EXPECT_EQ(scroll_button, scroll_view->contents());
  EXPECT_NE(view_with_layout_manager, nullptr);
  EXPECT_TRUE(views::IsViewClass<views::LabelButton>(view_with_layout_manager));
  EXPECT_EQ(view_with_layout_manager->GetLayoutManager(), layout_manager_ptr);
}

TEST_F(ViewFactoryTest, TestViewBuilderOwnerships) {
  views::View* parent = nullptr;
  views::LabelButton* button = nullptr;
  views::LabelButton* scroll_button = nullptr;
  views::ScrollView* scroll_view = nullptr;
  auto view_builder = views::Builder<views::View>();
  view_builder.CopyAddressTo(&parent)
      .SetEnabled(false)
      .SetVisible(true)
      .SetBackground(views::CreateSolidBackground(SK_ColorWHITE))
      .SetBorder(views::CreateEmptyBorder(0));
  view_builder.AddChild(views::Builder<views::View>()
                            .SetEnabled(false)
                            .SetVisible(true)
                            .SetProperty(views::kMarginsKey, gfx::Insets(5)));
  view_builder.AddChild(
      views::Builder<views::View>().SetGroup(5).SetID(1).SetFocusBehavior(
          views::View::FocusBehavior::NEVER));
  view_builder.AddChild(views::Builder<views::LabelButton>()
                            .CopyAddressTo(&button)
                            .SetIsDefault(true)
                            .SetEnabled(true)
                            .SetText(u"Test"));
  view_builder.AddChild(views::Builder<views::ScrollView>()
                            .CopyAddressTo(&scroll_view)
                            .SetContents(views::Builder<views::LabelButton>()
                                             .CopyAddressTo(&scroll_button)
                                             .SetText(u"ScrollTest"))
                            .SetHeader(views::Builder<views::View>().SetID(2)));
  auto view = std::move(view_builder).Build();

  ASSERT_TRUE(view.get());
  EXPECT_NE(parent, nullptr);
  EXPECT_NE(button, nullptr);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->GetEnabled());
  ASSERT_GT(view->children().size(), size_t{2});
  EXPECT_EQ(view->children()[1]->GetFocusBehavior(),
            views::View::FocusBehavior::NEVER);
  EXPECT_EQ(view->children()[2], button);
  EXPECT_EQ(button->GetText(), u"Test");
  EXPECT_NE(scroll_view, nullptr);
  EXPECT_NE(scroll_button, nullptr);
  EXPECT_EQ(scroll_button->GetText(), u"ScrollTest");
  EXPECT_EQ(scroll_button, scroll_view->contents());
}

TEST_F(ViewFactoryTest, TestViewBuilderArbitraryMethod) {
  auto view = views::Builder<internal::TestView>()
                  .SetEnabled(false)
                  .ArbitraryMethod(10, 5.5, views::kPropertyEffectsLayout)
                  .Build();

  EXPECT_FALSE(view->GetEnabled());
  EXPECT_EQ(view->get_int_param(), 10);
  EXPECT_EQ(view->get_float_param(), 5.5);
  EXPECT_EQ(view->get_property_effects(), views::kPropertyEffectsLayout);
}

TEST_F(ViewFactoryTest, TestViewBuilderCustomConfigure) {
  views::UniqueWidgetPtr widget =
      base::WrapUnique(CreateTopLevelPlatformWidget());
  auto* view = widget->GetContentsView()->AddChildView(
      views::Builder<internal::TestView>()
          .CustomConfigure(base::BindOnce([](internal::TestView* view) {
            view->SetEnabled(false);
            view->GetViewAccessibility().SetPosInSet(5);
            view->GetViewAccessibility().SetSetSize(10);
          }))
          .Build());
  EXPECT_FALSE(view->GetEnabled());
  ui::AXNodeData node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet), 5);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize), 10);
}

TEST_F(ViewFactoryTest, TestViewBuilderAddChildAtIndex) {
  views::View* parent = nullptr;
  views::LabelButton* ok_button = nullptr;
  views::LabelButton* cancel_button = nullptr;
  std::unique_ptr<views::View> view =
      views::Builder<views::View>()
          .CopyAddressTo(&parent)
          .AddChild(views::Builder<views::LabelButton>()
                        .CopyAddressTo(&cancel_button)
                        .SetIsDefault(false)
                        .SetEnabled(true)
                        .SetText(u"Cancel"))
          .AddChildAt(views::Builder<views::LabelButton>()
                          .CopyAddressTo(&ok_button)
                          .SetIsDefault(false)
                          .SetEnabled(true)
                          .SetText(u"OK"),
                      0)
          .Build();

  EXPECT_NE(parent, nullptr);
  EXPECT_NE(ok_button, nullptr);
  EXPECT_NE(cancel_button, nullptr);
  EXPECT_EQ(view.get(), parent);
  EXPECT_TRUE(view->GetVisible());
  // Make sure the OK button is inserted into the child list at index 0.
  EXPECT_EQ(ok_button, view->children()[0]);
  EXPECT_EQ(cancel_button, view->children()[1]);
}

TEST_F(ViewFactoryTest, TestOrderOfOperations) {
  using ViewCallback = base::OnceCallback<void(views::View*)>;

  views::View* view = nullptr;
  base::MockCallback<ViewCallback> custom_configure_callback;
  base::MockCallback<ViewCallback> after_build_callback;

  EXPECT_CALL(custom_configure_callback, Run).Times(0);
  EXPECT_CALL(after_build_callback, Run).Times(0);

  views::Builder<views::View> builder;
  builder.CopyAddressTo(&view)
      .SetID(1)
      .AddChild(views::Builder<views::View>())
      .CustomConfigure(custom_configure_callback.Get())
      .CustomConfigure(custom_configure_callback.Get())
      .AfterBuild(after_build_callback.Get())
      .AfterBuild(after_build_callback.Get());

  // Addresses should be copied *before* build but properties shouldn't be set,
  // children shouldn't be added, and callbacks shouldn't be run until *after*.
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(view->GetID(), 0);
  EXPECT_EQ(view->children().size(), 0u);
  testing::Mock::VerifyAndClearExpectations(&custom_configure_callback);
  testing::Mock::VerifyAndClearExpectations(&after_build_callback);

  {
    testing::InSequence sequence;

    // Expect that two custom configure callbacks will be run *before* any
    // after build callbacks. The order of the custom configure callbacks is
    // not guaranteed by the builder.
    EXPECT_CALL(custom_configure_callback, Run(testing::Pointer(view)))
        .Times(2)
        .WillRepeatedly(testing::Invoke([](views::View* view) {
          // Properties should be set *before* but children shouldn't be added
          // until *after* custom callbacks are run.
          EXPECT_EQ(view->GetID(), 1);
          EXPECT_EQ(view->children().size(), 0u);
        }));

    // Expect that two after build callbacks will be run *after* any custom
    // configure callbacks. The order of the after build callbacks is not
    // guaranteed by the builder.
    EXPECT_CALL(after_build_callback, Run(testing::Pointer(view)))
        .Times(2)
        .WillRepeatedly(testing::Invoke([](views::View* view) {
          // Children should be added *before* after build callbacks are run.
          EXPECT_EQ(view->children().size(), 1u);
        }));
  }

  // Build the view and verify order of operations.
  std::ignore = std::move(builder).Build();
}
