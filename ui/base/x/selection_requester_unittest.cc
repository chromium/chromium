// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requester.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_clipboard_helper.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

// The result of SelectionRequester::PerformConvertSelectionAsync().
struct ConvertSelectionResult {
  bool success = false;
  std::vector<uint8_t> data;
  x11::Atom type = x11::Atom::None;
};

using ConvertSelectionFuture = base::test::TestFuture<ConvertSelectionResult>;

}  // namespace

class SelectionRequesterTest : public testing::Test {
 public:
  explicit SelectionRequesterTest() : connection_(*x11::Connection::Get()) {}

  SelectionRequesterTest(const SelectionRequesterTest&) = delete;
  SelectionRequesterTest& operator=(const SelectionRequesterTest&) = delete;

  ~SelectionRequesterTest() override = default;

  // Requests `target` from `selection`. The result is delivered to `future`.
  void PerformConvertSelection(x11::Atom selection,
                               x11::Atom target,
                               ConvertSelectionFuture& future) {
    requestor_->PerformConvertSelectionAsync(
        selection, target,
        base::BindOnce(
            [](base::OnceCallback<void(ConvertSelectionResult)> callback,
               bool success, std::vector<uint8_t> data, x11::Atom type) {
              std::move(callback).Run({
                  .success = success,
                  .data = std::move(data),
                  .type = type,
              });
            },
            future.GetCallback()));
  }

  // Responds to the SelectionRequester's XConvertSelection() request by
  // - Setting the property passed into the XConvertSelection() request to
  //   `value`.
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

  // Responds to the SelectionRequester's XConvertSelection() request by
  // starting an INCR transfer.
  void SendIncrStart(x11::Atom selection, x11::Atom target) {
    connection_->SetProperty<uint32_t>(requestor_->x_window_,
                                       requestor_->x_property_,
                                       x11::GetAtom(kIncr), 1024);
    connection_->Sync();

    SendSelectionNotifyFailure(selection, target, requestor_->x_property_);
  }

  // Sends a chunk of an INCR transfer. An empty `value` ends the transfer.
  void SendIncrChunk(const std::string& value) {
    connection_->SetStringProperty(requestor_->x_window_,
                                   requestor_->x_property_, x11::Atom::STRING,
                                   value);
    connection_->Sync();

    requestor_->OnPropertyEvent({
        .window = requestor_->x_window_,
        .atom = requestor_->x_property_,
        .state = x11::Property::NewValue,
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

  ConvertSelectionFuture future1;
  ConvertSelectionFuture future2;

  PerformConvertSelection(selection, target1, future1);
  PerformConvertSelection(selection, target2, future2);

  SendSelectionNotify(selection, target1, "Data1");
  SendSelectionNotify(selection, target2, "Data2");

  const ConvertSelectionResult& result1 = future1.Get();
  EXPECT_TRUE(result1.success);
  EXPECT_EQ("Data1", base::as_string_view(result1.data));
  EXPECT_EQ(x11::Atom::STRING, result1.type);

  const ConvertSelectionResult& result2 = future2.Get();
  EXPECT_TRUE(result2.success);
  EXPECT_EQ("Data2", base::as_string_view(result2.data));
  EXPECT_EQ(x11::Atom::STRING, result2.type);
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

// Test that a request is aborted if the selection owner never responds.
TEST_F(SelectionRequesterTest, AbortStaleRequests) {
  x11::Atom selection = x11::GetAtom("FAKE_SELECTION");
  x11::Atom target = x11::GetAtom("TARGET");

  ConvertSelectionFuture future;
  PerformConvertSelection(selection, target, future);

  // Fast forward to trigger timeout.
  task_environment_.FastForwardBy(base::Seconds(10));

  const ConvertSelectionResult& result = future.Get();
  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.data.empty());
  EXPECT_EQ(x11::Atom::None, result.type);
}

// Test that a request which is never answered does not block subsequent
// requests indefinitely. Regression test for https://crbug.com/571343363.
TEST_F(SelectionRequesterTest, UnansweredRequestDoesNotBlockQueue) {
  x11::Atom selection = x11::GetAtom("FAKE_SELECTION");
  x11::Atom target1 = x11::GetAtom("TARGET1");
  x11::Atom target2 = x11::GetAtom("TARGET2");

  ConvertSelectionFuture future1;
  ConvertSelectionFuture future2;
  PerformConvertSelection(selection, target1, future1);
  PerformConvertSelection(selection, target2, future2);

  // Never respond to the first request. Advance past its timeout, but not far
  // enough for the second request to also time out.
  task_environment_.FastForwardBy(base::Seconds(6));
  ASSERT_TRUE(future1.IsReady());
  EXPECT_FALSE(future1.Get().success);

  // The second request should now be in flight and able to complete.
  EXPECT_FALSE(future2.IsReady());
  SendSelectionNotify(selection, target2, "Data2");
  const ConvertSelectionResult& result2 = future2.Get();
  EXPECT_TRUE(result2.success);
  EXPECT_EQ("Data2", base::as_string_view(result2.data));
}

// Test that the timeout for a queued request is measured from when it is sent
// to the selection owner, not from when it was queued.
TEST_F(SelectionRequesterTest, TimeoutStartsWhenRequestIsSent) {
  x11::Atom selection = x11::GetAtom("FAKE_SELECTION");
  x11::Atom target1 = x11::GetAtom("TARGET1");
  x11::Atom target2 = x11::GetAtom("TARGET2");

  ConvertSelectionFuture future1;
  ConvertSelectionFuture future2;
  PerformConvertSelection(selection, target1, future1);
  PerformConvertSelection(selection, target2, future2);

  task_environment_.FastForwardBy(base::Seconds(4));
  SendSelectionNotify(selection, target1, "Data1");
  EXPECT_TRUE(future1.Get().success);

  // The second request was queued more than the timeout ago, but has only been
  // in flight for a short time.
  task_environment_.FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(future2.IsReady());

  task_environment_.FastForwardBy(base::Seconds(2));
  ASSERT_TRUE(future2.IsReady());
  EXPECT_FALSE(future2.Get().success);
}

// Test that an INCR transfer that keeps making progress is not aborted, even if
// the total transfer time exceeds the timeout.
TEST_F(SelectionRequesterTest, IncrProgressResetsTimeout) {
  x11::Atom selection = x11::GetAtom("FAKE_SELECTION");
  x11::Atom target = x11::GetAtom("TARGET");

  ConvertSelectionFuture future;
  PerformConvertSelection(selection, target, future);

  SendIncrStart(selection, target);

  task_environment_.FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(future.IsReady());
  SendIncrChunk("Da");

  task_environment_.FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(future.IsReady());
  SendIncrChunk("ta");

  task_environment_.FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(future.IsReady());
  SendIncrChunk("");

  const ConvertSelectionResult& result = future.Get();
  EXPECT_TRUE(result.success);
  EXPECT_EQ("Data", base::as_string_view(result.data));
  EXPECT_EQ(x11::Atom::STRING, result.type);
}

// Test that an INCR transfer that stalls is aborted.
TEST_F(SelectionRequesterTest, StalledIncrTransferTimesOut) {
  x11::Atom selection = x11::GetAtom("FAKE_SELECTION");
  x11::Atom target = x11::GetAtom("TARGET");

  ConvertSelectionFuture future;
  PerformConvertSelection(selection, target, future);

  SendIncrStart(selection, target);

  // Never send any chunks.
  task_environment_.FastForwardBy(base::Seconds(10));
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().success);
}

}  // namespace ui
