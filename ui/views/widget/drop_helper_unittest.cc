// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/drop_helper.h"

#include <memory>
#include <set>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

// A view implementation for validating drop location.
class TestDropTargetView : public views::View {
  METADATA_HEADER(TestDropTargetView, View)

 public:
  TestDropTargetView() : drop_location_(-1, -1) {}

  bool GetDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override {
    *formats = ui::OSExchangeData::STRING;
    return true;
  }

  bool CanDrop(const OSExchangeData& data) override { return true; }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    return static_cast<int>(ui::mojom::DragOperation::kCopy);
  }

  gfx::Point drop_location() { return drop_location_; }

  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override {
    return base::BindOnce(&TestDropTargetView::PerformDrop,
                          base::Unretained(this));
  }

 private:
  gfx::Point drop_location_;

  void PerformDrop(const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
    drop_location_ = event.location();
    output_drag_op = ui::mojom::DragOperation::kCopy;
  }
};

BEGIN_METADATA(TestDropTargetView)
END_METADATA

class DropHelperTest : public ViewsTestBase {
 public:
  DropHelperTest() = default;
  ~DropHelperTest() override = default;
};

// Verifies that drop coordinates are dispatched in View coordinates.
TEST_F(DropHelperTest, DropCoordinates) {
  // Create widget
  Widget widget;
  Widget::InitParams init_params(
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  init_params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  widget.Init(std::move(init_params));

  // Setup a widget with a view that isn't aligned with the corner. In screen
  // coordinates, the target view starts at (120, 80) and ends at (220, 130).
  gfx::Rect bounds(100, 50, 200, 100);
  widget.SetBounds(bounds);
  views::View* container =
      widget.SetContentsView(std::make_unique<views::View>());
  TestDropTargetView* drop_target =
      container->AddChildView(std::make_unique<TestDropTargetView>());
  drop_target->SetBounds(20, 30, 100, 50);
  widget.Show();

  auto drop_helper = std::make_unique<DropHelper>(widget.GetRootView());

  // Construct drag data
  auto data = std::make_unique<ui::OSExchangeData>();
  data->SetString(u"Drag and drop is cool");
  int drag_operation = static_cast<int>(ui::mojom::DragOperation::kCopy);
  gfx::Point enter_location = {10, 0};

  // Enter parent view
  drop_helper->OnDragOver(*data, enter_location, drag_operation);

  // Location of center of target view in root.
  gfx::Point target = {70, 55};
  // Enter target view
  drop_helper->OnDragOver(*data, target, drag_operation);

  // Start drop.
  DropHelper::DropCallback callback =
      drop_helper->GetDropCallback(*data, target, drag_operation);
  ASSERT_TRUE(callback);

  // Perform the drop.
  ui::mojom::DragOperation output_op = ui::mojom::DragOperation::kNone;
  std::move(callback).Run(std::move(data), output_op,
                          /*drag_image_layer_owner=*/nullptr);

  // The test view always executes a copy operation.
  EXPECT_EQ(output_op, ui::mojom::DragOperation::kCopy);
  // Verify the location of the drop is centered in the target view.
  EXPECT_EQ(drop_target->drop_location(), gfx::Point(50, 25));
}

}  // namespace
}  // namespace views
