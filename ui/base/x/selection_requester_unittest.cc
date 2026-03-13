// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requester.h"

#include <stddef.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_clipboard_helper.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

class SelectionRequesterTest : public testing::Test {
 public:
  explicit SelectionRequesterTest() : connection_(*x11::Connection::Get()) {}

  SelectionRequesterTest(const SelectionRequesterTest&) = delete;
  SelectionRequesterTest& operator=(const SelectionRequesterTest&) = delete;

  ~SelectionRequesterTest() override = default;

  // Responds to the SelectionRequester's XConvertSelection() request by
  // - Setting the property passed into the XConvertSelection() request to
  //   |value|.
  // - Sending a SelectionNotify event.
  void SendSelectionNotify(x11::Atom selection,
                           x11::Atom target,
                           const std::string& value) {
    connection_->SetStringProperty(requestor_->x_window_,
                                   requestor_->x_property_, x11::Atom::STRING,
                                   value);
    connection_->Sync();

    SendSelectionNotifyFailure(selection, target, requestor_->x_property_);
  }

  void SendSelectionNotifyFailure(x11::Atom selection,
                                  x11::Atom target,
                                  x11::Atom property = x11::Atom::None) {
    requestor_->OnSelectionNotify({
        .requestor = requestor_->x_window_,
        .selection = selection,
        .target = target,
        .property = property,
    });
  }

 protected:
  void SetUp() override {
    helper_ = std::make_unique<XClipboardHelper>(
        base::BindRepeating([](ClipboardBuffer buffer) {}));
    requestor_ = helper_->GetSelectionRequesterForTest();
  }

  void TearDown() override {
    requestor_ = nullptr;
    helper_.reset();
  }

  raw_ref<x11::Connection> connection_;

  std::unique_ptr<XClipboardHelper> helper_;
  raw_ptr<SelectionRequester> requestor_ = nullptr;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
};

// Test that SelectionRequester correctly handles receiving a request while it
// is processing another request.
TEST_F(SelectionRequesterTest, NestedRequests) {
  // Assume that |selection| will have no owner. If there is an owner, the owner
  // will set the property passed into the XConvertSelection() request which is
  // undesirable.
  x11::Atom selection = x11::GetAtom("FAKE_SELECTION");

  x11::Atom target1 = x11::GetAtom("TARGET1");
  x11::Atom target2 = x11::GetAtom("TARGET2");

  base::test::TestFuture<bool, std::vector<uint8_t>, x11::Atom> future1;
  base::test::TestFuture<bool, std::vector<uint8_t>, x11::Atom> future2;

  requestor_->PerformConvertSelectionAsync(selection, target1,
                                           future1.GetCallback());
  requestor_->PerformConvertSelectionAsync(selection, target2,
                                           future2.GetCallback());

  SendSelectionNotify(selection, target1, "Data1");
  SendSelectionNotify(selection, target2, "Data2");

  EXPECT_TRUE(future1.Get<0>());
  const std::vector<uint8_t>& data1 = future1.Get<1>();
  EXPECT_EQ("Data1", base::as_string_view(data1));
  EXPECT_EQ(x11::Atom::STRING, future1.Get<2>());

  EXPECT_TRUE(future2.Get<0>());
  const std::vector<uint8_t>& data2 = future2.Get<1>();
  EXPECT_EQ("Data2", base::as_string_view(data2));
  EXPECT_EQ(x11::Atom::STRING, future2.Get<2>());
}

TEST_F(SelectionRequesterTest, AbortStaleRequests) {
  x11::Atom selection = x11::GetAtom("FAKE_SELECTION");
  x11::Atom target = x11::GetAtom("TARGET");

  base::test::TestFuture<bool, std::vector<uint8_t>, x11::Atom> future;
  requestor_->PerformConvertSelectionAsync(selection, target,
                                           future.GetCallback());

  // Fast forward to trigger timeout.
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_FALSE(future.Get<0>());
  EXPECT_TRUE(future.Get<1>().empty());
  EXPECT_EQ(x11::Atom::None, future.Get<2>());
}

TEST_F(SelectionRequesterTest, RequestTypesAsync) {
  x11::Atom selection = x11::GetAtom("FAKE_SELECTION");
  x11::Atom target1 = x11::GetAtom("TARGET1");
  x11::Atom target2 = x11::GetAtom("TARGET2");

  base::test::TestFuture<SelectionData> future;
  requestor_->RequestTypesAsync(selection, {target1, target2},
                                future.GetCallback());

  // Respond to the first request with failure.
  SendSelectionNotifyFailure(selection, target1);

  // Respond to the second request with success.
  SendSelectionNotify(selection, target2, "Data2");

  SelectionData result = future.Take();
  EXPECT_EQ(x11::Atom::STRING, result.GetType());
  EXPECT_EQ("Data2", base::as_string_view(result.GetSpan()));
}

}  // namespace ui
