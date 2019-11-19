// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ime/candidate_view.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_delegate.h"

namespace ui {
namespace ime {
namespace {

const char* const kDummyCandidates[] = {
  "candidate1",
  "candidate2",
  "candidate3",
};

}  // namespace

class CandidateViewTest : public views::ViewsTestBase,
                          public views::ButtonListener {
 public:
  CandidateViewTest() : widget_(NULL), last_pressed_(NULL) {}
  ~CandidateViewTest() override {}

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    views::Widget::InitParams init_params(CreateParams(
        views::Widget::InitParams::TYPE_WINDOW));

    init_params.delegate = new views::WidgetDelegateView();

    container_ = init_params.delegate->GetContentsView();
    container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    for (size_t i = 0; i < base::size(kDummyCandidates); ++i) {
      CandidateView* candidate = new CandidateView(
          this, ui::CandidateWindow::VERTICAL);
      ui::CandidateWindow::Entry entry;
      entry.value = base::UTF8ToUTF16(kDummyCandidates[i]);
      candidate->SetEntry(entry);
      container_->AddChildView(candidate);
    }

    widget_ = new views::Widget();
    widget_->Init(std::move(init_params));
    widget_->Show();

    aura::Window* native_window = widget_->GetNativeWindow();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        native_window->GetRootWindow(), native_window);
  }

  void TearDown() override {
    widget_->Close();

    views::ViewsTestBase::TearDown();
  }

 protected:
  CandidateView* GetCandidateAt(size_t index) {
    return static_cast<CandidateView*>(container_->children()[index]);
  }

  size_t GetHighlightedCount() const {
    const auto& children = container_->children();
    return std::count_if(
        children.cbegin(), children.cend(),
        [](const views::View* v) { return !!v->background(); });
  }

  int GetHighlightedIndex() const {
    const auto& children = container_->children();
    const auto it =
        std::find_if(children.cbegin(), children.cend(),
                     [](const views::View* v) { return !!v->background(); });
    return (it == children.cend()) ? -1 : std::distance(children.cbegin(), it);
  }

  int GetLastPressedIndexAndReset() {
    const auto& children = container_->children();
    const auto it =
        std::find(children.cbegin(), children.cend(), last_pressed_);
    if (it != children.cend()) {
      last_pressed_ = nullptr;
      return std::distance(children.cbegin(), it);
    }

    DCHECK(!last_pressed_);
    return -1;
  }

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

 private:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    last_pressed_ = sender;
  }

  views::Widget* widget_;
  views::View* container_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  views::View* last_pressed_;

  DISALLOW_COPY_AND_ASSIGN(CandidateViewTest);
};

TEST_F(CandidateViewTest, MouseHovers) {
  GetCandidateAt(0)->SetHighlighted(true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());

  // Mouse hover shouldn't change the background.
  event_generator()->MoveMouseTo(
      GetCandidateAt(0)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());

  // Mouse hover shouldn't change the background.
  event_generator()->MoveMouseTo(
      GetCandidateAt(1)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());

  // Mouse hover shouldn't change the background.
  event_generator()->MoveMouseTo(
      GetCandidateAt(2)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());
}

TEST_F(CandidateViewTest, MouseClick) {
  event_generator()->MoveMouseTo(
      GetCandidateAt(1)->GetBoundsInScreen().CenterPoint());
  event_generator()->ClickLeftButton();
  EXPECT_EQ(1, GetLastPressedIndexAndReset());
}

TEST_F(CandidateViewTest, ClickAndMove) {
  GetCandidateAt(0)->SetHighlighted(true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());

  event_generator()->MoveMouseTo(
      GetCandidateAt(2)->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(2, GetHighlightedIndex());

  // Highlight follows the drag.
  event_generator()->MoveMouseTo(
      GetCandidateAt(1)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(1, GetHighlightedIndex());

  event_generator()->MoveMouseTo(
      GetCandidateAt(0)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());

  event_generator()->MoveMouseTo(
      GetCandidateAt(1)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(1, GetHighlightedIndex());

  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(1, GetLastPressedIndexAndReset());
}

}  // namespace ime
}  // namespace ui
