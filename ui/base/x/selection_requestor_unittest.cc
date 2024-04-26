// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requestor.h"

#include <stddef.h>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_clipboard_helper.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

class SelectionRequestorTest : public testing::Test {
 public:
  explicit SelectionRequestorTest() : connection_(*x11::Connection::Get()) {}

  SelectionRequestorTest(const SelectionRequestorTest&) = delete;
  SelectionRequestorTest& operator=(const SelectionRequestorTest&) = delete;

  ~SelectionRequestorTest() override = default;

  // Responds to the SelectionRequestor's XConvertSelection() request by
  // - Setting the property passed into the XConvertSelection() request to
  //   |value|.
  // - Sending a SelectionNotify event.
  void SendSelectionNotify(x11::Atom selection,
                           x11::Atom target,
                           const std::string& value) {
    connection_->SetStringProperty(x_window_, requestor_->x_property_,
                                   x11::Atom::STRING, value);

    requestor_->OnSelectionNotify({
        .requestor = x_window_,
        .selection = selection,
        .target = target,
        .property = requestor_->x_property_,
    });
  }

 protected:
  void SetUp() override {
    // Create a window for the selection requestor to use.
    x_window_ = connection_->CreateDummyWindow();
    helper_ = std::make_unique<XClipboardHelper>(
        base::BindRepeating([](ClipboardBuffer buffer) {}));
    requestor_ = helper_->GetSelectionRequestorForTest();
  }

  void TearDown() override {
    helper_.reset();
    requestor_ = nullptr;
    connection_->DestroyWindow({x_window_});
  }

  raw_ref<x11::Connection> connection_;

  // |requestor_|'s window.
  x11::Window x_window_ = x11::Window::None;

  std::unique_ptr<XClipboardHelper> helper_;
  raw_ptr<SelectionRequestor> requestor_ = nullptr;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

namespace {

// Converts |selection| to |target| and checks the returned values.
void PerformBlockingConvertSelection(SelectionRequestor* requestor,
                                     x11::Atom selection,
                                     x11::Atom target,
                                     const std::string& expected_data) {
  std::vector<uint8_t> out_data;
  x11::Atom out_type = x11::Atom::None;
  EXPECT_TRUE(requestor->PerformBlockingConvertSelection(selection, target,
                                                         &out_data, &out_type));
  EXPECT_EQ(expected_data.size(), out_data.size());
  EXPECT_EQ(expected_data, ui::RefCountedMemoryToString(
                               base::RefCountedBytes::TakeVector(&out_data)));
  EXPECT_EQ(x11::Atom::STRING, out_type);
}

}  // namespace

// Test that SelectionRequestor correctly handles receiving a request while it
// is processing another request.
// TODO(crbug.com/40398800): Reenable once clipboard interface is async.
TEST_F(SelectionRequestorTest, DISABLED_NestedRequests) {
  // Assume that |selection| will have no owner. If there is an owner, the owner
  // will set the property passed into the XConvertSelection() request which is
  // undesirable.
  x11::Atom selection = x11::GetAtom("FAKE_SELECTION");

  x11::Atom target1 = x11::GetAtom("TARGET1");
  x11::Atom target2 = x11::GetAtom("TARGET2");

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PerformBlockingConvertSelection,
                                base::Unretained(requestor_), selection,
                                target2, "Data2"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectionRequestorTest::SendSelectionNotify,
                     base::Unretained(this), selection, target1, "Data1"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectionRequestorTest::SendSelectionNotify,
                     base::Unretained(this), selection, target2, "Data2"));
  PerformBlockingConvertSelection(requestor_, selection, target1, "Data1");
}

}  // namespace ui
