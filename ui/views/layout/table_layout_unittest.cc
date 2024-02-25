// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/table_layout.h"

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

void ExpectViewBoundsEquals(int x, int y, int w, int h, const View* view) {
  EXPECT_EQ(x, view->x());
  EXPECT_EQ(y, view->y());
  EXPECT_EQ(w, view->width());
  EXPECT_EQ(h, view->height());
}

std::unique_ptr<View> CreateSizedView(const gfx::Size& size) {
  auto view = std::make_unique<View>();
  view->SetPreferredSize(size);
  return view;
}

// View that lets you set the minimum size.
class MinSizeView : public View {
  METADATA_HEADER(MinSizeView, View)

 public:
  explicit MinSizeView(const gfx::Size& min_size) : min_size_(min_size) {}

  MinSizeView(const MinSizeView&) = delete;
  MinSizeView& operator=(const MinSizeView&) = delete;

  ~MinSizeView() override = default;

  // View:
  gfx::Size GetMinimumSize() const override { return min_size_; }

 private:
  const gfx::Size min_size_;
};

std::unique_ptr<MinSizeView> CreateViewWithMinAndPref(const gfx::Size& min,
                                                      const gfx::Size& pref) {
  auto view = std::make_unique<MinSizeView>(min);
  view->SetPreferredSize(pref);
  return view;
}

BEGIN_METADATA(MinSizeView)
END_METADATA

}  // namespace

class TableLayoutTest : public testing::Test {
 public:
  TableLayoutTest() : host_(std::make_unique<View>()) {
    layout_ = host_->SetLayoutManager(std::make_unique<views::TableLayout>());
  }

  gfx::Size GetPreferredSize() {
    return layout_->GetPreferredSize(host_.get());
  }

  View* host() { return host_.get(); }
  TableLayout& layout() { return *layout_; }

 private:
  std::unique_ptr<View> host_;
  raw_ptr<TableLayout> layout_;
};

class TableLayoutAlignmentTest : public testing::Test {
 public:
  TableLayoutAlignmentTest() : host_(std::make_unique<View>()) {
    layout_ = host_->SetLayoutManager(std::make_unique<views::TableLayout>());
  }

  void TestAlignment(LayoutAlignment alignment, gfx::Rect* bounds) {
    layout_
        ->AddColumn(alignment, alignment, 1.0f,
                    TableLayout::ColumnSize::kUsePreferred, 0, 0)
        .AddRows(1, 1.0f);
    auto* v1 = host_->AddChildView(std::make_unique<View>());
    v1->SetPreferredSize(gfx::Size(10, 20));
    gfx::Size pref = layout_->GetPreferredSize(host_.get());
    EXPECT_EQ(gfx::Size(10, 20), pref);
    host_->SetBounds(0, 0, 100, 100);
    layout_->Layout(host_.get());
    *bounds = v1->bounds();
  }

 private:
  std::unique_ptr<View> host_;
  raw_ptr<TableLayout> layout_;
};

TEST_F(TableLayoutAlignmentTest, Fill) {
  gfx::Rect bounds;
  TestAlignment(LayoutAlignment::kStretch, &bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), bounds);
}

TEST_F(TableLayoutAlignmentTest, Leading) {
  gfx::Rect bounds;
  TestAlignment(LayoutAlignment::kStart, &bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), bounds);
}

TEST_F(TableLayoutAlignmentTest, Center) {
  gfx::Rect bounds;
  TestAlignment(LayoutAlignment::kCenter, &bounds);
  EXPECT_EQ(gfx::Rect(45, 40, 10, 20), bounds);
}

TEST_F(TableLayoutAlignmentTest, Trailing) {
  gfx::Rect bounds;
  TestAlignment(LayoutAlignment::kEnd, &bounds);
  EXPECT_EQ(gfx::Rect(90, 80, 10, 20), bounds);
}

TEST_F(TableLayoutTest, TwoColumns) {
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(10, 20)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(20, 20)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(30, 20), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 10, 20, v1);
  ExpectViewBoundsEquals(10, 0, 20, 20, v2);
}

// Test linked column sizes, and the column size limit.
TEST_F(TableLayoutTest, LinkedSizes) {
  // Fill widths.
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .LinkColumnSizes({0, 1, 2})
      .AddRows(1, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(10, 20)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(20, 20)));
  auto* v3 = host()->AddChildView(CreateSizedView(gfx::Size(0, 20)));

  gfx::Size pref = GetPreferredSize();

  // |v1| and |v3| should obtain the same width as |v2|, since |v2| is largest.
  pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(20 + 20 + 20, 20), pref);
  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 20, 20, v1);
  ExpectViewBoundsEquals(20, 0, 20, 20, v2);
  ExpectViewBoundsEquals(40, 0, 20, 20, v3);

  // If the limit is zero, behaves as though the columns are not linked.
  layout().SetLinkedColumnSizeLimit(0);
  pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(10 + 20 + 0, 20), pref);
  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 10, 20, v1);
  ExpectViewBoundsEquals(10, 0, 20, 20, v2);
  ExpectViewBoundsEquals(30, 0, 0, 20, v3);

  // Set a size limit.
  layout().SetLinkedColumnSizeLimit(40);
  v1->SetPreferredSize(gfx::Size(35, 20));

  // |v1| now dominates, but it is still below the limit.
  pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(35 + 35 + 35, 20), pref);
  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 35, 20, v1);
  ExpectViewBoundsEquals(35, 0, 35, 20, v2);
  ExpectViewBoundsEquals(70, 0, 35, 20, v3);

  // Go over the limit. |v1| shouldn't influence size at all, but the others
  // should still be linked to the next largest width.
  v1->SetPreferredSize(gfx::Size(45, 20));
  pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(45 + 20 + 20, 20), pref);
  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 45, 20, v1);
  ExpectViewBoundsEquals(45, 0, 20, 20, v2);
  ExpectViewBoundsEquals(65, 0, 20, 20, v3);
}

TEST_F(TableLayoutTest, ColSpan1) {
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(2, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(100, 20)));
  v1->SetProperty(kTableColAndRowSpanKey, gfx::Size(2, 1));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(10, 40)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(100, 60), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 100, 20, v1);
  ExpectViewBoundsEquals(0, 20, 10, 40, v2);
}

TEST_F(TableLayoutTest, ColSpan2) {
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(2, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(100, 20)));
  v1->SetProperty(kTableColAndRowSpanKey, gfx::Size(2, 1));
  host()->AddChildView(std::make_unique<View>());
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(10, 20)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(100, 40), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 100, 20, v1);
  ExpectViewBoundsEquals(90, 20, 10, 20, v2);
}

TEST_F(TableLayoutTest, ColSpan3) {
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(2, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(100, 20)));
  v1->SetProperty(kTableColAndRowSpanKey, gfx::Size(2, 1));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(10, 20)));
  auto* v3 = host()->AddChildView(CreateSizedView(gfx::Size(10, 20)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(100, 40), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 100, 20, v1);
  ExpectViewBoundsEquals(0, 20, 10, 20, v2);
  ExpectViewBoundsEquals(50, 20, 10, 20, v3);
}

TEST_F(TableLayoutTest, ColSpan4) {
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(2, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));
  auto* v3 = host()->AddChildView(CreateSizedView(gfx::Size(25, 20)));
  v3->SetProperty(kTableColAndRowSpanKey, gfx::Size(2, 1));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(25, 30), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 10, 10, v1);
  ExpectViewBoundsEquals(12, 0, 10, 10, v2);
  ExpectViewBoundsEquals(0, 10, 25, 20, v3);
}

// Verifies the sizing of a view that doesn't start in the first column
// and has a column span > 1 (crbug.com/254092).
TEST_F(TableLayoutTest, ColSpanStartSecondColumn) {
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch,
                 TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed, 10,
                 0)
      .AddRows(1, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(20, 10)));
  v2->SetProperty(kTableColAndRowSpanKey, gfx::Size(2, 1));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(30, 10), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 10, 10, v1);
  ExpectViewBoundsEquals(10, 0, 20, 10, v2);
}

TEST_F(TableLayoutTest, SameSizeColumns) {
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .LinkColumnSizes({0, 1})
      .AddRows(1, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(50, 20)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(100, 20), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 50, 20, v1);
  ExpectViewBoundsEquals(50, 0, 10, 10, v2);
}

TEST_F(TableLayoutTest, HorizontalResizeTest1) {
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStart, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(50, 20)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));

  host()->SetBounds(0, 0, 110, 20);
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 100, 20, v1);
  ExpectViewBoundsEquals(100, 0, 10, 10, v2);
}

TEST_F(TableLayoutTest, HorizontalResizeTest2) {
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStart, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kEnd, LayoutAlignment::kStart, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(50, 20)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));

  host()->SetBounds(0, 0, 120, 20);
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 80, 20, v1);
  ExpectViewBoundsEquals(110, 0, 10, 10, v2);
}

// Tests that space leftover due to rounding is distributed to the last
// resizable column.
TEST_F(TableLayoutTest, HorizontalResizeTest3) {
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStart, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStart, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kEnd, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));
  auto* v3 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));

  host()->SetBounds(0, 0, 31, 10);
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 10, 10, v1);
  ExpectViewBoundsEquals(10, 0, 11, 10, v2);
  ExpectViewBoundsEquals(21, 0, 10, 10, v3);
}

TEST_F(TableLayoutTest, TestVerticalResize1) {
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, 1.0f)
      .AddRows(1, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(50, 20)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(50, 30), pref);

  host()->SetBounds(0, 0, 50, 100);
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 50, 90, v1);
  ExpectViewBoundsEquals(0, 90, 50, 10, v2);
}

TEST_F(TableLayoutTest, Border) {
  host()->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(1, 2, 3, 4)));
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(10, 20)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(16, 24), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(2, 1, 10, 20, v1);
}

TEST_F(TableLayoutTest, FixedSize) {
  host()->SetBorder(CreateEmptyBorder(2));

  constexpr size_t kRowCount = 2;
  constexpr size_t kColumnCount = 4;
  constexpr int kTitleWidth = 100;
  constexpr int kPrefWidth = 10;
  constexpr int kPrefHeight = 20;

  for (size_t i = 0; i < kColumnCount; ++i) {
    layout().AddColumn(LayoutAlignment::kCenter, LayoutAlignment::kCenter,
                       TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed,
                       kTitleWidth, kTitleWidth);
  }
  layout().AddRows(kRowCount, TableLayout::kFixedSize);

  for (size_t row = 0; row < kRowCount; ++row) {
    for (size_t column = 0; column < kColumnCount; ++column)
      host()->AddChildView(CreateSizedView(gfx::Size(kPrefWidth, kPrefHeight)));
  }

  layout().Layout(host());

  auto i = host()->children().cbegin();
  for (size_t row = 0; row < kRowCount; ++row) {
    for (size_t column = 0; column < kColumnCount; ++column, ++i) {
      ExpectViewBoundsEquals(
          2 + kTitleWidth * column + (kTitleWidth - kPrefWidth) / 2,
          2 + kPrefHeight * row, kPrefWidth, kPrefHeight, *i);
    }
  }

  EXPECT_EQ(
      gfx::Size(kColumnCount * kTitleWidth + 4, kRowCount * kPrefHeight + 4),
      GetPreferredSize());
}

TEST_F(TableLayoutTest, RowSpanWithPaddingRow) {
  layout()
      .AddColumn(LayoutAlignment::kCenter, LayoutAlignment::kCenter,
                 TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed, 10,
                 10)
      .AddRows(1, TableLayout::kFixedSize)
      .AddPaddingRow(TableLayout::kFixedSize, 10);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(10, 10)));
  v1->SetProperty(kTableColAndRowSpanKey, gfx::Size(1, 2));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(10, 10), pref);

  host()->SetBounds(0, 0, 10, 20);
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 10, 10, v1);
}

TEST_F(TableLayoutTest, RowSpan) {
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize)
      .AddRows(1, 1.0f);
  host()->AddChildView(CreateSizedView(gfx::Size(20, 10)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(20, 40)));
  v2->SetProperty(kTableColAndRowSpanKey, gfx::Size(1, 2));
  auto* v3 = host()->AddChildView(CreateSizedView(gfx::Size(20, 10)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(40, 40), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 10, 20, 10, v3);
}

TEST_F(TableLayoutTest, RowSpan2) {
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize)
      .AddPaddingRow(TableLayout::kFixedSize, 10)
      .AddRows(1, TableLayout::kFixedSize);
  host()->AddChildView(CreateSizedView(gfx::Size(20, 20)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(64, 64)));
  v2->SetProperty(kTableColAndRowSpanKey, gfx::Size(1, 3));
  host()->AddChildView(CreateSizedView(gfx::Size(10, 20)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(84, 64), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(20, 0, 64, 64, v2);
}

// Make sure that for views that span columns the underlying columns are resized
// based on the resize percent of the column.
TEST_F(TableLayoutTest, ColumnSpanResizing) {
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kCenter, 2.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kCenter, 4.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(2, TableLayout::kFixedSize);
  // span_view spans two columns and is twice as big the views added below.
  View* span_view = host()->AddChildView(CreateSizedView(gfx::Size(12, 40)));
  span_view->SetProperty(kTableColAndRowSpanKey, gfx::Size(2, 1));
  span_view->SetProperty(kTableHorizAlignKey, LayoutAlignment::kStart);
  span_view->SetProperty(kTableVertAlignKey, LayoutAlignment::kStart);

  auto* view1 = host()->AddChildView(CreateSizedView(gfx::Size(2, 40)));
  auto* view2 = host()->AddChildView(CreateSizedView(gfx::Size(4, 40)));

  host()->SetBounds(0, 0, 12, 80);
  layout().Layout(host());

  ExpectViewBoundsEquals(0, 0, 12, 40, span_view);

  // view1 should be 4 pixels wide
  // column_pref + (remaining_width * column_resize / total_column_resize) =
  // 2 + (6 * 2 / 6).
  ExpectViewBoundsEquals(0, 40, 4, 40, view1);

  // And view2 should be 8 pixels wide:
  // 4 + (6 * 4 / 6).
  ExpectViewBoundsEquals(4, 40, 8, 40, view2);
}

// Make sure that for views that span both fixed and resizable columns the
// underlying resiable column is resized and the fixed sized column is not.
TEST_F(TableLayoutTest, ColumnSpanResizing2) {
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kCenter, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kCenter,
                 TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed, 10,
                 0)
      .AddRows(2, TableLayout::kFixedSize);
  View* span_view = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(20, 40), gfx::Size(80, 40)));
  span_view->SetProperty(kTableColAndRowSpanKey, gfx::Size(2, 1));

  auto* view1 = host()->AddChildView(CreateSizedView(gfx::Size(1, 40)));
  auto* view2 = host()->AddChildView(CreateSizedView(gfx::Size(1, 40)));

  // Host width is shorter than the preferred width.
  host()->SetBounds(0, 0, 30, 80);
  layout().Layout(host());

  // `span_view` should shrink to respect the host bound.
  ExpectViewBoundsEquals(0, 0, 30, 40, span_view);

  // The first column is host_width - col2_width = 30 - 10 = 20 pixels wide.
  ExpectViewBoundsEquals(0, 40, 20, 40, view1);

  // The second column is fixed 10 pixels wide.
  ExpectViewBoundsEquals(20, 40, 10, 40, view2);
}

// Make sure that for views that span both fixed and resizable columns the
// underlying resizable column is resized and the fixed sized column is not.
// The host width in this test is shorter than the minimum size of columns.
TEST_F(TableLayoutTest, ColumnSpanResizing3) {
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kCenter, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kCenter,
                 TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed, 10,
                 0)
      .AddRows(2, TableLayout::kFixedSize);
  View* span_view = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(0, 40), gfx::Size(80, 40)));
  span_view->SetProperty(kTableColAndRowSpanKey, gfx::Size(2, 1));

  auto* view1 = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(0, 40), gfx::Size(1, 40)));
  auto* view2 = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(0, 40), gfx::Size(1, 40)));

  // Host width is shorter than the minimum size of columns
  // i.e 5 < col1_min_width + col2_min_width = 0 + 10 = 10.
  host()->SetBounds(0, 0, 5, 80);
  layout().Layout(host());

  // `span_view` should shrink to the fixed width of col2.
  ExpectViewBoundsEquals(0, 0, 10, 40, span_view);

  // The first column is 0 pixels wide.
  ExpectViewBoundsEquals(0, 40, 0, 40, view1);

  // The second width column is fixed 10 pixels wide.
  ExpectViewBoundsEquals(0, 40, 10, 40, view2);
}

TEST_F(TableLayoutTest, MinimumPreferredSize) {
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize);
  host()->AddChildView(CreateSizedView(gfx::Size(10, 20)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(10, 20), pref);

  layout().SetMinimumSize(gfx::Size(40, 40));
  pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(40, 40), pref);
}

TEST_F(TableLayoutTest, ColumnMinForcesPreferredWidth) {
  // Column's min width is greater than views preferred/min width. This should
  // force the preferred width to the min width of the column.
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 5.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 100)
      .AddRows(1, TableLayout::kFixedSize);
  host()->AddChildView(CreateSizedView(gfx::Size(20, 10)));

  EXPECT_EQ(gfx::Size(100, 10), GetPreferredSize());
}

TEST_F(TableLayoutTest, HonorsColumnMin) {
  // Verifies that a column with a min width is never shrunk smaller than the
  // min width.
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 5.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 100)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 5.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize);
  View* view1 = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(10, 10), gfx::Size(125, 10)));
  View* view2 = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(10, 10), gfx::Size(50, 10)));

  EXPECT_EQ(gfx::Size(175, 10), GetPreferredSize());

  host()->SetBounds(0, 0, 175, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 125, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(125, 0, 50, 10), view2->bounds());

  host()->SetBounds(0, 0, 125, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(100, 0, 25, 10), view2->bounds());

  host()->SetBounds(0, 0, 120, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(100, 0, 20, 10), view2->bounds());
}

TEST_F(TableLayoutTest, TwoViewsOneSizeSmallerThanMinimum) {
  // Two columns, equally resizable with two views. Only the first view is
  // resizable.
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 5.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 5.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize);
  View* view1 = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(20, 10), gfx::Size(100, 10)));
  View* view2 = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(100, 10), gfx::Size(100, 10)));

  host()->SetBounds(0, 0, 110, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(20, 0, 100, 10), view2->bounds());
}

TEST_F(TableLayoutTest, TwoViewsBothSmallerThanMinimumDifferentResizeWeights) {
  // Two columns, equally resizable with two views. Only the first view is
  // resizable.
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 8.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 2.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize);
  View* view1 = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(91, 10), gfx::Size(100, 10)));
  View* view2 = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(10, 10), gfx::Size(100, 10)));

  // 200 is the preferred, each should get their preferred width.
  host()->SetBounds(0, 0, 200, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(100, 0, 100, 10), view2->bounds());

  // 1 pixel smaller than pref.
  host()->SetBounds(0, 0, 199, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 99, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(99, 0, 100, 10), view2->bounds());

  // 10 pixels smaller than pref.
  host()->SetBounds(0, 0, 190, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 92, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(92, 0, 98, 10), view2->bounds());

  // 11 pixels smaller than pref.
  host()->SetBounds(0, 0, 189, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 91, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(91, 0, 98, 10), view2->bounds());

  // 12 pixels smaller than pref.
  host()->SetBounds(0, 0, 188, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 91, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(91, 0, 97, 10), view2->bounds());

  host()->SetBounds(0, 0, 5, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 91, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(91, 0, 10, 10), view2->bounds());
}

TEST_F(TableLayoutTest, TwoViewsOneColumnUsePrefOtherFixed) {
  layout()
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 8.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 2.0f,
                 TableLayout::ColumnSize::kFixed, 100, 0)
      .AddRows(1, TableLayout::kFixedSize);
  View* view1 = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(10, 10), gfx::Size(100, 10)));
  View* view2 = host()->AddChildView(
      CreateViewWithMinAndPref(gfx::Size(10, 10), gfx::Size(100, 10)));

  host()->SetBounds(0, 0, 120, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), view1->bounds());
  // Even though column 2 has a resize percent, it's FIXED, so it won't shrink.
  EXPECT_EQ(gfx::Rect(20, 0, 100, 10), view2->bounds());

  host()->SetBounds(0, 0, 10, 0);
  layout().Layout(host());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(10, 0, 100, 10), view2->bounds());
}

TEST_F(TableLayoutTest, InsufficientChildren) {
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(2, TableLayout::kFixedSize);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(10, 20)));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(20, 20)));
  auto* v3 = host()->AddChildView(CreateSizedView(gfx::Size(10, 20)));

  gfx::Size pref = GetPreferredSize();
  EXPECT_EQ(gfx::Size(30, 40), pref);

  host()->SetBounds(0, 0, pref.width(), pref.height());
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 10, 20, v1);
  ExpectViewBoundsEquals(10, 0, 20, 20, v2);
  ExpectViewBoundsEquals(0, 20, 10, 20, v3);
}

TEST_F(TableLayoutTest, DistributeRemainingHeight) {
  layout()
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(2, 1.0f);
  auto* v1 = host()->AddChildView(CreateSizedView(gfx::Size(10, 40)));
  v1->SetProperty(views::kTableColAndRowSpanKey, gfx::Size(1, 2));
  auto* v2 = host()->AddChildView(CreateSizedView(gfx::Size(10, 18)));
  auto* v3 = host()->AddChildView(CreateSizedView(gfx::Size(10, 19)));

  // The 3 extra height from v1 (compared to v2 + v3) should be fully
  // distributed between v2 and v3, so the total height is not less than 40.
  constexpr gfx::Size kDesiredSize(20, 40);
  EXPECT_EQ(kDesiredSize, GetPreferredSize());

  host()->SetBoundsRect(gfx::Rect(kDesiredSize));
  layout().Layout(host());
  ExpectViewBoundsEquals(0, 0, 10, 40, v1);
  ExpectViewBoundsEquals(10, 0, 10, 18, v2);
  // Because 3 extra height doesn't divide evenly, it gets rounded, so v2 gets
  // an extra dip compared to v3; thus v3 should start at y = 18 + 2 = 20.
  ExpectViewBoundsEquals(10, 20, 10, 19, v3);
}

}  // namespace views
