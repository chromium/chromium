// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/prefix_selector.h"

#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/prefix_delegate.h"
#include "ui/views/test/views_test_base.h"

using base::ASCIIToUTF16;

namespace views {

class TestPrefixDelegate : public View, public PrefixDelegate {
 public:
  TestPrefixDelegate() {
    rows_.push_back(u"aardvark");
    rows_.push_back(u"antelope");
    rows_.push_back(u"badger");
    rows_.push_back(u"gnu");
  }

  TestPrefixDelegate(const TestPrefixDelegate&) = delete;
  TestPrefixDelegate& operator=(const TestPrefixDelegate&) = delete;

  ~TestPrefixDelegate() override = default;

  size_t GetRowCount() override { return rows_.size(); }

  std::optional<size_t> GetSelectedRow() override { return selected_row_; }

  void SetSelectedRow(std::optional<size_t> row) override {
    selected_row_ = row;
  }

  std::u16string GetTextForRow(size_t row) override { return rows_[row]; }

 private:
  std::vector<std::u16string> rows_;
  std::optional<size_t> selected_row_ = 0;
};

class PrefixSelectorTest : public ViewsTestBase {
 public:
  PrefixSelectorTest() {
    selector_ = std::make_unique<PrefixSelector>(&delegate_, &delegate_);
  }

  PrefixSelectorTest(const PrefixSelectorTest&) = delete;
  PrefixSelectorTest& operator=(const PrefixSelectorTest&) = delete;

  ~PrefixSelectorTest() override {
    // Explicitly release |selector_| here which can happen before releasing
    // |delegate_|.
    selector_.reset();
  }

 protected:
  std::unique_ptr<PrefixSelector> selector_;
  TestPrefixDelegate delegate_;
};

TEST_F(PrefixSelectorTest, PrefixSelect) {
  selector_->InsertText(
      u"an",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(1u, delegate_.GetSelectedRow());

  // Invoke OnViewBlur() to reset time.
  selector_->OnViewBlur();
  selector_->InsertText(
      u"a",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(0u, delegate_.GetSelectedRow());

  selector_->OnViewBlur();
  selector_->InsertText(
      u"g",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(3u, delegate_.GetSelectedRow());

  selector_->OnViewBlur();
  selector_->InsertText(
      u"b",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  selector_->InsertText(
      u"a",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(2u, delegate_.GetSelectedRow());

  selector_->OnViewBlur();
  selector_->InsertText(
      u"\t",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  selector_->InsertText(
      u"b",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  selector_->InsertText(
      u"a",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(2u, delegate_.GetSelectedRow());
}

}  // namespace views
