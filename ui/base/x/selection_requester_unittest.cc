// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requester.h"

#include <stddef.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_clipboard_helper.h"
#include "ui/base/x/x11_util.h"
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

  // Returns the property passed into the XConvertSelection() request for the
  // in-flight request.
  x11::Atom CurrentProperty() {
    SelectionRequester::Request* request = requestor_->GetCurrentRequest();
    CHECK(request);
    return request->property;
  }

  // Responds to the SelectionRequester's XConvertSelection() request by
  // - Setting the property passed into the XConvertSelection() request to
  //   `value`.
  // - Sending a SelectionNotify event.
  void SendSelectionNotify(x11::Atom selection,
                           x11::Atom target,
                           const std::string& value) {
    SetCurrentProperty(value);
    SendSelectionNotifyFailure(selection, target, CurrentProperty());
  }

  // Sets the property passed into the in-flight XConvertSelection() request to
  // `value` without sending a SelectionNotify event.
  void SetCurrentProperty(const std::string& value) {
    connection_->SetStringProperty(requestor_->x_window_, CurrentProperty(),
                                   x11::Atom::STRING, value);
    connection_->Sync();
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
    connection_->SetProperty<uint32_t>(requestor_->x_window_, CurrentProperty(),
                                       connection_->GetAtom(kIncr), 1024);
    connection_->Sync();

    SendSelectionNotifyFailure(selection, target, CurrentProperty());
  }

  // Sends a chunk of an INCR transfer. An empty `value` ends the transfer.
  void SendIncrChunk(const std::string& value) {
    connection_->SetStringProperty(requestor_->x_window_, CurrentProperty(),
                                   x11::Atom::STRING, value);
    connection_->Sync();

    requestor_->OnPropertyEvent({
        .window = requestor_->x_window_,
        .atom = CurrentProperty(),
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
    for (x11::Window window : owned_windows_) {
      connection_->DestroyWindow({window});
    }
    connection_->Sync();
    requestor_ = nullptr;
    helper_.reset();
    connection_->DispatchAll();
  }

  // Returns a selection atom isolated to this test worker process so parallel
  // test shards sharing one Xvfb server do not clobber each other's owner.
  x11::Atom CreateUniqueSelection() {
    return connection_->GetAtom(
        base::StringPrintf("FAKE_SELECTION_%d", getpid()).c_str());
  }

  x11::Window CreateSelectionOwner(x11::Atom selection) {
    x11::Window window = connection_->CreateDummyWindow("Test Selection Owner");
    owned_windows_.push_back(window);
    connection_->SetSelectionOwner({
        .owner = window,
        .selection = selection,
        .time = x11::Time::CurrentTime,
    });
    connection_->Sync();
    return window;
  }

  // Flushes pending X11 requests, waits for server replies, and dispatches
  // queued responses in sequence order.
  void PumpX11() {
    connection_->Sync();
    connection_->DispatchAll();
  }

  raw_ref<x11::Connection> connection_;
  std::vector<x11::Window> owned_windows_;

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
  x11::Atom selection = connection_->GetAtom("FAKE_SELECTION");

  x11::Atom target1 = connection_->GetAtom("TARGET1");
  x11::Atom target2 = connection_->GetAtom("TARGET2");

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
  x11::Atom selection = connection_->GetAtom("FAKE_SELECTION");
  x11::Atom target1 = connection_->GetAtom("TARGET1");
  x11::Atom target2 = connection_->GetAtom("TARGET2");

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
  x11::Atom selection = connection_->GetAtom("FAKE_SELECTION");
  x11::Atom target = connection_->GetAtom("TARGET");

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
  x11::Atom selection = connection_->GetAtom("FAKE_SELECTION");
  x11::Atom target1 = connection_->GetAtom("TARGET1");
  x11::Atom target2 = connection_->GetAtom("TARGET2");

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
  x11::Atom selection = connection_->GetAtom("FAKE_SELECTION");
  x11::Atom target1 = connection_->GetAtom("TARGET1");
  x11::Atom target2 = connection_->GetAtom("TARGET2");

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
  x11::Atom selection = connection_->GetAtom("FAKE_SELECTION");
  x11::Atom target = connection_->GetAtom("TARGET");

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
  x11::Atom selection = connection_->GetAtom("FAKE_SELECTION");
  x11::Atom target = connection_->GetAtom("TARGET");

  ConvertSelectionFuture future;
  PerformConvertSelection(selection, target, future);

  SendIncrStart(selection, target);

  // Never send any chunks.
  task_environment_.FastForwardBy(base::Seconds(10));
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().success);
}

// Test that a stale SelectionNotify for a completed INCR transfer is not
// mistaken for the reply to the next request for the same target. xsel sends
// such an event after it finishes an INCR transfer.
TEST_F(SelectionRequesterTest, StaleSelectionNotifyAfterIncrSameTarget) {
  x11::Atom selection = connection_->GetAtom("FAKE_SELECTION");
  x11::Atom target = connection_->GetAtom("TARGET");

  ConvertSelectionFuture future1;
  ConvertSelectionFuture future2;
  PerformConvertSelection(selection, target, future1);
  PerformConvertSelection(selection, target, future2);

  x11::Atom incr_property = CurrentProperty();
  SendIncrStart(selection, target);
  SendIncrChunk("Data1");
  SendIncrChunk("");
  ASSERT_TRUE(future1.IsReady());
  EXPECT_TRUE(future1.Get().success);
  EXPECT_EQ("Data1", base::as_string_view(future1.Get().data));

  // The owner confirms the finished INCR transfer after the second request was
  // sent, but before replying to it.
  SendSelectionNotifyFailure(selection, target, incr_property);
  EXPECT_FALSE(future2.IsReady());

  SendSelectionNotify(selection, target, "Data2");
  ASSERT_TRUE(future2.IsReady());
  EXPECT_TRUE(future2.Get().success);
  EXPECT_EQ("Data2", base::as_string_view(future2.Get().data));
}

// Test that a stale SelectionNotify for a completed INCR transfer does not
// delete the reply to the next request for a different target.
TEST_F(SelectionRequesterTest, StaleSelectionNotifyAfterIncrOtherTarget) {
  x11::Atom selection = connection_->GetAtom("FAKE_SELECTION");
  x11::Atom target1 = connection_->GetAtom("TARGET1");
  x11::Atom target2 = connection_->GetAtom("TARGET2");

  ConvertSelectionFuture future1;
  ConvertSelectionFuture future2;
  PerformConvertSelection(selection, target1, future1);
  PerformConvertSelection(selection, target2, future2);

  x11::Atom incr_property = CurrentProperty();
  SendIncrStart(selection, target1);
  SendIncrChunk("Data1");
  SendIncrChunk("");
  ASSERT_TRUE(future1.IsReady());
  EXPECT_TRUE(future1.Get().success);

  // The owner has already written the reply to the second request when the
  // stale confirmation of the INCR transfer is processed.
  SetCurrentProperty("Data2");
  SendSelectionNotifyFailure(selection, target1, incr_property);
  EXPECT_FALSE(future2.IsReady());

  SendSelectionNotifyFailure(selection, target2, CurrentProperty());
  ASSERT_TRUE(future2.IsReady());
  EXPECT_TRUE(future2.Get().success);
  EXPECT_EQ("Data2", base::as_string_view(future2.Get().data));
}

// Test that a live selection owner taking longer than 5 seconds to produce its
// initial SelectionNotify (e.g. encoding a large PNG image on demand) is not
// aborted prematurely. Regression test for https://crbug.com/487168662.
TEST_F(SelectionRequesterTest, SlowSelectionOwnerDoesNotTimeOutEarly) {
  x11::Atom selection = CreateUniqueSelection();
  x11::Atom target = connection_->GetAtom("TARGET");
  CreateSelectionOwner(selection);

  ConvertSelectionFuture future;
  PerformConvertSelection(selection, target, future);
  PumpX11();

  // Simulate an owner taking 15 seconds to encode the requested format before
  // starting an INCR transfer.
  for (int i = 0; i < 3; ++i) {
    task_environment_.FastForwardBy(base::Seconds(5));
    PumpX11();
    EXPECT_FALSE(future.IsReady());
  }

  SendIncrStart(selection, target);
  SendIncrChunk("LargeImage");
  SendIncrChunk("Payload");
  SendIncrChunk("");

  ASSERT_TRUE(future.IsReady());
  const ConvertSelectionResult& result = future.Get();
  EXPECT_TRUE(result.success);
  EXPECT_EQ("LargeImagePayload", base::as_string_view(result.data));
  EXPECT_EQ(x11::Atom::STRING, result.type);
}

// Test that if the selection owner loses ownership before replying, the
// request still aborts after the short 5-second timeout rather than waiting
// for the full slow-owner deadline.
TEST_F(SelectionRequesterTest, LostSelectionOwnershipTimesOutQuickly) {
  x11::Atom selection = CreateUniqueSelection();
  x11::Atom target = connection_->GetAtom("TARGET");
  CreateSelectionOwner(selection);

  ConvertSelectionFuture future;
  PerformConvertSelection(selection, target, future);
  PumpX11();

  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_FALSE(future.IsReady());

  // The owner exits or clears ownership before replying.
  connection_->SetSelectionOwner({
      .owner = x11::Window::None,
      .selection = selection,
      .time = x11::Time::CurrentTime,
  });
  connection_->Sync();

  task_environment_.FastForwardBy(base::Seconds(2));
  PumpX11();
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().success);
}

// Test that if another window takes ownership of the selection while a request
// is waiting on the previous owner, the request aborts after the short
// 5-second timeout rather than waiting for the full slow-owner deadline.
TEST_F(SelectionRequesterTest, ReplacedSelectionOwnerTimesOutQuickly) {
  x11::Atom selection = CreateUniqueSelection();
  x11::Atom target = connection_->GetAtom("TARGET");
  CreateSelectionOwner(selection);

  ConvertSelectionFuture future;
  PerformConvertSelection(selection, target, future);
  PumpX11();

  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_FALSE(future.IsReady());

  // Another client (such as a clipboard manager) takes ownership.
  CreateSelectionOwner(selection);

  task_environment_.FastForwardBy(base::Seconds(2));
  PumpX11();
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().success);
}

// Test that a selection owner that stays alive owning the selection without
// ever replying still eventually times out so subsequent requests can proceed.
TEST_F(SelectionRequesterTest, UnresponsiveSelectionOwnerEventuallyTimesOut) {
  x11::Atom selection = CreateUniqueSelection();
  x11::Atom target1 = connection_->GetAtom("TARGET1");
  x11::Atom target2 = connection_->GetAtom("TARGET2");
  CreateSelectionOwner(selection);

  ConvertSelectionFuture future1;
  ConvertSelectionFuture future2;
  PerformConvertSelection(selection, target1, future1);
  PerformConvertSelection(selection, target2, future2);
  PumpX11();

  for (int i = 0; i < 12; ++i) {
    EXPECT_FALSE(future1.IsReady());
    task_environment_.FastForwardBy(base::Seconds(5));
    PumpX11();
  }

  ASSERT_TRUE(future1.IsReady());
  EXPECT_FALSE(future1.Get().success);

  // The next queued request should now be in flight and able to succeed.
  EXPECT_FALSE(future2.IsReady());
  SendSelectionNotify(selection, target2, "Data2");
  ASSERT_TRUE(future2.IsReady());
  EXPECT_TRUE(future2.Get().success);
  EXPECT_EQ("Data2", base::as_string_view(future2.Get().data));
}

}  // namespace ui
