// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/prefix_selector.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/prefix_delegate.h"
#include "ui/views/test/views_test_base.h"

using base::ASCIIToUTF16;

namespace views {

class TestPrefixDelegate : public View, public PrefixDelegate {
 public:
  TestPrefixDelegate() {
    rows_.push_back(ASCIIToUTF16("aardvark"));
    rows_.push_back(ASCIIToUTF16("antelope"));
    rows_.push_back(ASCIIToUTF16("badger"));
    rows_.push_back(ASCIIToUTF16("gnu"));
  }

  ~TestPrefixDelegate() override = default;

  int GetRowCount() override { return static_cast<int>(rows_.size()); }

  int GetSelectedRow() override { return selected_row_; }

  void SetSelectedRow(int row) override { selected_row_ = row; }

  base::string16 GetTextForRow(int row) override { return rows_[row]; }

 private:
  std::vector<base::string16> rows_;
  int selected_row_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestPrefixDelegate);
};

class PrefixSelectorTest : public ViewsTestBase {
 public:
  PrefixSelectorTest() {
    selector_ = std::make_unique<PrefixSelector>(&delegate_, &delegate_);
  }
  ~PrefixSelectorTest() override {
    // Explicitly release |selector_| here which can happen before releasing
    // |delegate_|.
    selector_.reset();
  }

 protected:
  std::unique_ptr<PrefixSelector> selector_;
  TestPrefixDelegate delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefixSelectorTest);
};

TEST_F(PrefixSelectorTest, PrefixSelect) {
  selector_->InsertText(ASCIIToUTF16("an"));
  EXPECT_EQ(1, delegate_.GetSelectedRow());

  // Invoke OnViewBlur() to reset time.
  selector_->OnViewBlur();
  selector_->InsertText(ASCIIToUTF16("a"));
  EXPECT_EQ(0, delegate_.GetSelectedRow());

  selector_->OnViewBlur();
  selector_->InsertText(ASCIIToUTF16("g"));
  EXPECT_EQ(3, delegate_.GetSelectedRow());

  selector_->OnViewBlur();
  selector_->InsertText(ASCIIToUTF16("b"));
  selector_->InsertText(ASCIIToUTF16("a"));
  EXPECT_EQ(2, delegate_.GetSelectedRow());

  selector_->OnViewBlur();
  selector_->InsertText(ASCIIToUTF16("\t"));
  selector_->InsertText(ASCIIToUTF16("b"));
  selector_->InsertText(ASCIIToUTF16("a"));
  EXPECT_EQ(2, delegate_.GetSelectedRow());
}

}  // namespace views
