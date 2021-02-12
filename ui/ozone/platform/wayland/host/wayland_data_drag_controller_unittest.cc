// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/wayland/common/data_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_drag_drop_test.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/platform_clipboard.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_drag_handler.h"
#include "ui/platform_window/wm/wm_drop_handler.h"
#include "url/gurl.h"

using testing::_;
using testing::Mock;

namespace ui {

namespace {

constexpr char kSampleTextForDragAndDrop[] =
    "This is a sample text for drag-and-drop.";

constexpr FilenameToURLPolicy kFilenameToURLPolicy =
    FilenameToURLPolicy::CONVERT_FILENAMES;

template <typename StringType>
PlatformClipboard::Data ToClipboardData(const StringType& data_string) {
  auto* begin = reinterpret_cast<typename std::vector<uint8_t>::const_pointer>(
      data_string.data());
  std::vector<uint8_t> result(
      begin,
      begin + (data_string.size() * sizeof(typename StringType::value_type)));
  return scoped_refptr<base::RefCountedBytes>(
      base::RefCountedBytes::TakeVector(&result));
}

}  // namespace

class MockDragHandlerDelegate : public WmDragHandler::Delegate {
 public:
  MOCK_METHOD1(OnDragLocationChanged, void(const gfx::Point& location));
  MOCK_METHOD1(OnDragOperationChanged,
               void(DragDropTypes::DragOperation operation));
  MOCK_METHOD1(OnDragFinished, void(int operation));
};

class MockDropHandler : public WmDropHandler {
 public:
  MockDropHandler() = default;
  ~MockDropHandler() override = default;

  MOCK_METHOD0(MockOnDragEnter, void());
  MOCK_METHOD3(MockDragMotion,
               int(const gfx::PointF& point, int operation, int modifiers));
  MOCK_METHOD0(MockOnDragDrop, void());
  MOCK_METHOD0(OnDragLeave, void());

  void SetOnDropClosure(base::RepeatingClosure closure) {
    on_drop_closure_ = closure;
  }

  void SetPreferredOperations(int preferred_operations) {
    preferred_operations_ = preferred_operations;
  }

  OSExchangeData* dropped_data() { return dropped_data_.get(); }

  int available_operations() const { return available_operations_; }

 protected:
  void OnDragEnter(const gfx::PointF& point,
                   std::unique_ptr<ui::OSExchangeData> data,
                   int operation,
                   int modifiers) override {
    dropped_data_ = std::move(data);
    MockOnDragEnter();
  }
  void OnDragDrop(std::unique_ptr<OSExchangeData> data,
                  int modifiers) override {
    MockOnDragDrop();
    on_drop_closure_.Run();
    on_drop_closure_.Reset();
  }

  int OnDragMotion(const gfx::PointF& point,
                   int operation,
                   int modifiers) override {
    available_operations_ = operation;
    MockDragMotion(point, operation, modifiers);
    return preferred_operations_;
  }

 private:
  base::RepeatingClosure on_drop_closure_;

  std::unique_ptr<OSExchangeData> dropped_data_;
  int preferred_operations_ = ui::DragDropTypes::DRAG_COPY;
  int available_operations_ = ui::DragDropTypes::DRAG_NONE;
};

class WaylandDataDragControllerTest : public WaylandDragDropTest {
 public:
  WaylandDataDragControllerTest() = default;
  ~WaylandDataDragControllerTest() override = default;

  void SetUp() override {
    WaylandDragDropTest::SetUp();

    drag_handler_delegate_ = std::make_unique<MockDragHandlerDelegate>();
    drop_handler_ = std::make_unique<MockDropHandler>();
    SetWmDropHandler(window_.get(), drop_handler_.get());
  }

  WaylandDataDragController* drag_controller() const {
    return connection_->data_drag_controller();
  }

  WaylandDataDevice* data_device() const {
    return connection_->data_device_manager()->GetDevice();
  }

  MockDropHandler* drop_handler() { return drop_handler_.get(); }

  MockDragHandlerDelegate* drag_handler() {
    return drag_handler_delegate_.get();
  }

  wl::MockSurface* GetMockSurface(uint32_t id) {
    return server_.GetObject<wl::MockSurface>(id);
  }

  WaylandConnection* connection() { return connection_.get(); }

  WaylandWindow* window() { return window_.get(); }

  base::string16 sample_text_for_dnd() const {
    static auto text = base::ASCIIToUTF16(kSampleTextForDragAndDrop);
    return text;
  }

  void RunDragLoopWithSampleData(WaylandWindow* origin_window, int operations) {
    ASSERT_TRUE(origin_window);
    OSExchangeData os_exchange_data;
    os_exchange_data.SetString(sample_text_for_dnd());
    origin_window->StartDrag(os_exchange_data, operations, /*cursor=*/{},
                             /*can_grab_pointer=*/true,
                             drag_handler_delegate_.get());
    Sync();
  }

  std::unique_ptr<WaylandWindow> CreateTestWindow(
      PlatformWindowType type,
      const gfx::Size& size,
      MockPlatformWindowDelegate* delegate) {
    DCHECK(delegate);
    PlatformWindowInitProperties properties{gfx::Rect(size)};
    properties.type = type;
    EXPECT_CALL(*delegate, OnAcceleratedWidgetAvailable(_)).Times(1);
    auto window = WaylandWindow::Create(delegate, connection_.get(),
                                        std::move(properties));
    SetWmDropHandler(window.get(), drop_handler_.get());
    EXPECT_NE(gfx::kNullAcceleratedWidget, window->GetWidget());
    Sync();
    return window;
  }

  void ScheduleDragCancel() {
    ScheduleTestTask(base::BindOnce(
        [](WaylandDataDragControllerTest* self) {
          self->SendDndCancelled();

          // DnD handlers expect DragLeave to be sent before DragFinished when
          // drag sessions end up with no data transfer (cancelled). Otherwise,
          // it might lead to issues like https://crbug.com/1109324.
          EXPECT_CALL(*self->drop_handler(), OnDragLeave).Times(1);
          EXPECT_CALL(*self->drag_handler(), OnDragFinished).Times(1);

          self->Sync();
        },
        base::Unretained(this)));
  }

 protected:
  std::unique_ptr<MockDropHandler> drop_handler_;
  std::unique_ptr<MockDragHandlerDelegate> drag_handler_delegate_;
};

TEST_P(WaylandDataDragControllerTest, StartDrag) {
  const bool restored_focus = window_->has_pointer_focus();
  window_->SetPointerFocus(true);

  auto test = [](WaylandDataDragControllerTest* self) {
    // Now the server can read the data and give it to our callback.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    auto read_callback = base::BindOnce(
        [](base::RunLoop* loop, std::vector<uint8_t>&& data) {
          std::string result(data.begin(), data.end());
          EXPECT_EQ(kSampleTextForDragAndDrop, result);
          loop->Quit();
        },
        &run_loop);
    self->ReadData(kMimeTypeTextUtf8, std::move(read_callback));
    run_loop.Run();

    self->SendDndCancelled();
    self->Sync();
  };

  // Post test task to be performed asynchronously once the dnd-related protocol
  // objects are ready.
  ScheduleTestTask(base::BindOnce(test, base::Unretained(this)));

  RunDragLoopWithSampleData(
      window_.get(), DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE);

  // Ensure drag delegate it properly reset when the drag loop quits.
  EXPECT_FALSE(data_device()->drag_delegate_);

  window_->SetPointerFocus(restored_focus);
}

TEST_P(WaylandDataDragControllerTest, StartDragWithWrongMimeType) {
  bool restored_focus = window_->has_pointer_focus();
  window_->SetPointerFocus(true);

  // The client starts dragging offering data with |kMimeTypeHTML|
  OSExchangeData os_exchange_data;
  os_exchange_data.SetHtml(sample_text_for_dnd(), {});
  int operation = DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
  drag_controller()->StartSession(os_exchange_data, operation);
  Sync();

  // The server should get an empty data buffer in ReadData callback when trying
  // to read it with a different mime type.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::RunLoop* loop, std::vector<uint8_t>&& data) {
        std::string result(data.begin(), data.end());
        EXPECT_TRUE(result.empty());
        loop->Quit();
      },
      &run_loop);
  data_device_manager_->data_source()->ReadData(kMimeTypeText,
                                                std::move(callback));
  run_loop.Run();
  window_->SetPointerFocus(restored_focus);
}

TEST_P(WaylandDataDragControllerTest, StartDragWithText) {
  bool restored_focus = window_->has_pointer_focus();
  window_->SetPointerFocus(true);

  // The client starts dragging offering text mime type.
  OSExchangeData os_exchange_data;
  os_exchange_data.SetString(sample_text_for_dnd());
  int operation = DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
  drag_controller()->StartSession(os_exchange_data, operation);
  Sync();

  // The server should get a "text" representation in ReadData callback when
  // trying to read it as mime type other than |kMimeTypeText| and
  // |kTextMimeTypeUtf8|.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::RunLoop* loop, std::vector<uint8_t>&& data) {
        std::string result(data.begin(), data.end());
        EXPECT_EQ(kSampleTextForDragAndDrop, result);
        loop->Quit();
      },
      &run_loop);
  data_device_manager_->data_source()->ReadData(kMimeTypeMozillaURL,
                                                std::move(callback));
  run_loop.Run();
  window_->SetPointerFocus(restored_focus);
}

TEST_P(WaylandDataDragControllerTest, ReceiveDrag) {
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(kMimeTypeText,
                      ToClipboardData(std::string(kSampleTextForDragAndDrop)));

  gfx::Point entered_point(10, 10);
  // The server sends an enter event.
  data_device_manager_->data_device()->OnEnter(
      1002, surface_->resource(), wl_fixed_from_int(entered_point.x()),
      wl_fixed_from_int(entered_point.y()), data_offer);

  Sync();

  int64_t time =
      (EventTimeForNow() - base::TimeTicks()).InMilliseconds() & UINT32_MAX;
  gfx::Point motion_point(11, 11);

  // The server sends an motion event.
  data_device_manager_->data_device()->OnMotion(
      time, wl_fixed_from_int(motion_point.x()),
      wl_fixed_from_int(motion_point.y()));

  Sync();

  auto callback = base::BindOnce([](PlatformClipboard::Data contents) {
    std::string result;
    EXPECT_TRUE(contents);
    result.assign(contents->front_as<char>(), contents->size());
    EXPECT_EQ(kSampleTextForDragAndDrop, result);
  });

  // The client requests the data and gets callback with it.
  data_device()->RequestData(drag_controller()->data_offer_.get(),
                             kMimeTypeText, std::move(callback));
  Sync();

  data_device_manager_->data_device()->OnLeave();
}

TEST_P(WaylandDataDragControllerTest, DropSeveralMimeTypes) {
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(kMimeTypeText,
                      ToClipboardData(std::string(kSampleTextForDragAndDrop)));
  data_offer->OnOffer(kMimeTypeMozillaURL, ToClipboardData(base::UTF8ToUTF16(
                                               "https://sample.com/\r\n"
                                               "Sample")));
  data_offer->OnOffer(
      kMimeTypeURIList,
      ToClipboardData(std::string("file:///home/user/file\r\n")));

  EXPECT_CALL(*drop_handler_, MockOnDragEnter()).Times(1);
  gfx::Point entered_point(10, 10);
  data_device_manager_->data_device()->OnEnter(
      1002, surface_->resource(), wl_fixed_from_int(entered_point.x()),
      wl_fixed_from_int(entered_point.y()), data_offer);
  // Here we are expecting three data items, so there will be three roundtrips
  // to the Wayland and back.  Hence Sync() three times.
  Sync();
  Sync();
  Sync();

  EXPECT_CALL(*drop_handler_, MockOnDragDrop()).Times(1);
  base::RunLoop loop;
  drop_handler_->SetOnDropClosure(loop.QuitClosure());
  data_device_manager_->data_device()->OnDrop();

  Sync();
  loop.Run();
  Mock::VerifyAndClearExpectations(drop_handler_.get());

  ASSERT_NE(drop_handler_->dropped_data(), nullptr);
  EXPECT_TRUE(drop_handler_->dropped_data()->HasString());
  EXPECT_TRUE(drop_handler_->dropped_data()->HasFile());
  EXPECT_TRUE(drop_handler_->dropped_data()->HasURL(kFilenameToURLPolicy));

  data_device_manager_->data_device()->OnLeave();
}

// Tests URI validation for text/uri-list MIME type.  Log warnings rendered in
// the console when this test is running are the expected and valid side effect.
TEST_P(WaylandDataDragControllerTest, ValidateDroppedUriList) {
  const struct {
    std::string content;
    base::flat_set<std::string> expected_uris;
  } kCases[] = {{{}, {}},
                {"file:///home/user/file\r\n", {"/home/user/file"}},
                {"# Comment\r\n"
                 "file:///home/user/file\r\n"
                 "file:///home/guest/file\r\n"
                 "not a filename at all\r\n"
                 "https://valid.url/but/scheme/is/not/file/so/invalid\r\n",
                 {"/home/user/file", "/home/guest/file"}}};

  for (const auto& kCase : kCases) {
    auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
    data_offer->OnOffer(kMimeTypeURIList, ToClipboardData(kCase.content));

    EXPECT_CALL(*drop_handler_, MockOnDragEnter()).Times(1);
    gfx::Point entered_point(10, 10);
    data_device_manager_->data_device()->OnEnter(
        1002, surface_->resource(), wl_fixed_from_int(entered_point.x()),
        wl_fixed_from_int(entered_point.y()), data_offer);
    Sync();

    EXPECT_CALL(*drop_handler_, MockOnDragDrop()).Times(1);
    base::RunLoop loop;
    drop_handler_->SetOnDropClosure(loop.QuitClosure());
    data_device_manager_->data_device()->OnDrop();

    Sync();
    loop.Run();
    Mock::VerifyAndClearExpectations(drop_handler_.get());

    if (kCase.expected_uris.empty()) {
      EXPECT_FALSE(drop_handler_->dropped_data()->HasFile());
    } else {
      EXPECT_TRUE(drop_handler_->dropped_data()->HasFile());
      std::vector<FileInfo> filenames;
      EXPECT_TRUE(drop_handler_->dropped_data()->GetFilenames(&filenames));
      EXPECT_EQ(filenames.size(), kCase.expected_uris.size());
      for (const auto& filename : filenames)
        EXPECT_EQ(kCase.expected_uris.count(filename.path.AsUTF8Unsafe()), 1U);
    }

    EXPECT_CALL(*drop_handler_, OnDragLeave()).Times(1);
    data_device_manager_->data_device()->OnLeave();
    Sync();
    Mock::VerifyAndClearExpectations(drop_handler_.get());
  }
}

// Tests URI validation for text/x-moz-url MIME type.  Log warnings rendered in
// the console when this test is running are the expected and valid side effect.
TEST_P(WaylandDataDragControllerTest, ValidateDroppedXMozUrl) {
  const struct {
    std::string content;
    std::string expected_url;
    std::string expected_title;
  } kCases[] = {
      {{}, {}, {}},
      {"http://sample.com/\r\nSample", "http://sample.com/", "Sample"},
      {"http://title.must.be.set/", {}, {}},
      {"url.must.be.valid/and/have.scheme\r\nInvalid URL", {}, {}},
      {"file:///files/are/ok\r\nThe policy allows that", "file:///files/are/ok",
       "The policy allows that"}};

  for (const auto& kCase : kCases) {
    auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
    data_offer->OnOffer(kMimeTypeMozillaURL,
                        ToClipboardData(base::UTF8ToUTF16(kCase.content)));

    EXPECT_CALL(*drop_handler_, MockOnDragEnter()).Times(1);
    gfx::Point entered_point(10, 10);
    data_device_manager_->data_device()->OnEnter(
        1002, surface_->resource(), wl_fixed_from_int(entered_point.x()),
        wl_fixed_from_int(entered_point.y()), data_offer);
    Sync();

    EXPECT_CALL(*drop_handler_, MockOnDragDrop()).Times(1);
    base::RunLoop loop;
    drop_handler_->SetOnDropClosure(loop.QuitClosure());
    data_device_manager_->data_device()->OnDrop();

    Sync();
    loop.Run();
    Mock::VerifyAndClearExpectations(drop_handler_.get());

    const auto* const dropped_data = drop_handler_->dropped_data();
    if (kCase.expected_url.empty()) {
      EXPECT_FALSE(dropped_data->HasURL(kFilenameToURLPolicy));
    } else {
      EXPECT_TRUE(dropped_data->HasURL(kFilenameToURLPolicy));
      GURL url;
      base::string16 title;
      EXPECT_TRUE(
          dropped_data->GetURLAndTitle(kFilenameToURLPolicy, &url, &title));
      EXPECT_EQ(url.spec(), kCase.expected_url);
      EXPECT_EQ(title, base::UTF8ToUTF16(kCase.expected_title));
    }

    EXPECT_CALL(*drop_handler_, OnDragLeave()).Times(1);
    data_device_manager_->data_device()->OnLeave();
    Sync();
    Mock::VerifyAndClearExpectations(drop_handler_.get());
  }
}

// Verifies the correct delegate functions are called when a drag session is
// started and cancelled within the same surface.
TEST_P(WaylandDataDragControllerTest, StartAndCancel) {
  const bool restored_focus = window_->has_pointer_focus();
  window_->SetPointerFocus(true);

  // Schedule a wl_data_source::cancelled event to be sent asynchronously
  // once the drag session gets started.
  ScheduleDragCancel();

  RunDragLoopWithSampleData(window_.get(), DragDropTypes::DRAG_COPY);

  window_->SetPointerFocus(restored_focus);
}

TEST_P(WaylandDataDragControllerTest, ForeignDragHandleAskAction) {
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(kMimeTypeText,
                      ToClipboardData(std::string(kSampleTextForDragAndDrop)));
  data_offer->OnSourceActions(WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE |
                              WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
  data_offer->OnAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK);

  gfx::Point entered_point(10, 10);
  // The server sends an enter event.
  data_device_manager_->data_device()->OnEnter(
      1002, surface_->resource(), wl_fixed_from_int(entered_point.x()),
      wl_fixed_from_int(entered_point.y()), data_offer);
  Sync();

  int64_t time = 1;
  gfx::Point motion_point(11, 11);

  // Verify ask handling with drop handler preferring "copy" operation.
  drop_handler_->SetPreferredOperations(ui::DragDropTypes::DRAG_COPY);
  data_device_manager_->data_device()->OnMotion(
      time, wl_fixed_from_int(motion_point.x()),
      wl_fixed_from_int(motion_point.y()));
  Sync();

  EXPECT_EQ(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY,
            data_offer->preferred_action());
  EXPECT_EQ(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY,
            data_offer->supported_actions());

  data_device_manager_->data_device()->OnLeave();
}

// Verifies entered surface destruction is properly handled.
// Regression test for https://crbug.com/1143707.
TEST_P(WaylandDataDragControllerTest, DestroyEnteredSurface) {
  auto* window_1 = window_.get();
  const bool restored_focus = window_1->has_pointer_focus();
  window_1->SetPointerFocus(true);
  ASSERT_EQ(PlatformWindowType::kWindow, window_1->type());

  auto test = [](WaylandDataDragControllerTest* self) {
    // Init and open |target_window|.
    MockPlatformWindowDelegate delegate_2;
    auto window_2 = self->CreateTestWindow(PlatformWindowType::kWindow,
                                           gfx::Size(80, 80), &delegate_2);

    // Leave |window_1| and enter |window_2|.
    self->SendDndLeave();
    self->SendDndEnter(window_2.get(), gfx::Point(20, 20));
    self->Sync();

    // Destroy the entered window at client side and emulates a
    // wl_data_device::leave to ensure no UAF happens.
    window_2->PrepareForShutdown();
    window_2.reset();
    self->SendDndLeave();
    self->Sync();

    // Emulate server sending an wl_data_source::cancelled event so the drag
    // loop is finished.
    self->SendDndCancelled();
    self->Sync();
  };

  // Post test task to be performed asynchronously once the drag session gets
  // started.
  ScheduleTestTask(base::BindOnce(test, base::Unretained(this)));

  RunDragLoopWithSampleData(window_.get(), DragDropTypes::DRAG_COPY);

  window_1->SetPointerFocus(restored_focus);
}

// Verifies that early origin surface destruction is properly handled.
// Regression test for https://crbug.com/1143707.
TEST_P(WaylandDataDragControllerTest, DestroyOriginSurface) {
  auto* window_1 = window_.get();
  const bool restored_focus = window_1->has_pointer_focus();
  window_1->SetPointerFocus(false);
  ASSERT_EQ(PlatformWindowType::kWindow, window_1->type());

  auto test = [](WaylandDataDragControllerTest* self,
                 std::unique_ptr<WaylandWindow>* origin) {
    // Leave origin surface and enter |window_|.
    self->SendDndLeave();
    self->SendDndEnter(self->window(), gfx::Point(20, 20));
    self->Sync();

    // Shutdown and destroy the popup window where the drag session was
    // initiated, which leads to the drag loop to finish.
    (*origin)->PrepareForShutdown();
    origin->reset();
  };

  // Init and open |target_window|.
  MockPlatformWindowDelegate delegate_2;
  auto window_2 = CreateTestWindow(PlatformWindowType::kPopup,
                                   gfx::Size(80, 80), &delegate_2);
  window_2->SetPointerFocus(true);
  Sync();

  // Post test task to be performed asynchronously once the drag session gets
  // started.
  ScheduleTestTask(base::BindOnce(test, base::Unretained(this),
                                  base::Unretained(&window_2)));

  // Request to start the drag session, which spins a nested run loop.
  OSExchangeData os_exchange_data;
  os_exchange_data.SetString(sample_text_for_dnd());
  window_2->StartDrag(os_exchange_data, DragDropTypes::DRAG_COPY, {}, true,
                      drag_handler_delegate_.get());
  Sync();

  // Send wl_data_source::cancelled event. The drag controller is then
  // expected to gracefully reset its internal state.
  SendDndLeave();
  SendDndCancelled();
  Sync();

  window_1->SetPointerFocus(restored_focus);
}

// Ensures drag/drop events are properly propagated to non-toplevel windows.
TEST_P(WaylandDataDragControllerTest, DragToNonToplevelWindows) {
  auto* origin_window = window_.get();
  const bool restored_focus = origin_window->has_pointer_focus();
  origin_window->SetPointerFocus(true);

  auto test = [](WaylandDataDragControllerTest* self,
                 PlatformWindowType window_type) {
    // Init and open |target_window|.
    MockPlatformWindowDelegate aux_window_delegate;
    auto aux_window = self->CreateTestWindow(window_type, gfx::Size(100, 40),
                                             &aux_window_delegate);

    // Leave |origin_window| and enter non-toplevel |aux_window|.
    self->SendDndLeave();
    EXPECT_CALL(*self->drop_handler(), OnDragLeave).Times(1);
    self->Sync();

    self->SendDndEnter(aux_window.get(), {});
    EXPECT_CALL(*self->drop_handler(), MockOnDragEnter()).Times(1);
    EXPECT_CALL(*self->drop_handler(), MockDragMotion(_, _, _)).Times(1);
    self->Sync();

    // Goes back to |origin_window|, as |aux_window| is going to get destroyed
    // once this test task finishes.
    self->SendDndLeave();
    EXPECT_CALL(*self->drop_handler(), OnDragLeave).Times(1);
    self->Sync();

    self->SendDndEnter(self->window(), {});
    EXPECT_CALL(*self->drop_handler(), MockOnDragEnter()).Times(1);
    EXPECT_CALL(*self->drop_handler(), MockDragMotion(_, _, _)).Times(1);
    self->Sync();
  };

  // Post test tasks, for each non-toplevel window type, to be performed
  // asynchronously once the dnd-related protocol objects are ready.
  constexpr PlatformWindowType kNonToplevelWindowTypes[]{
      PlatformWindowType::kPopup, PlatformWindowType::kMenu,
      PlatformWindowType::kTooltip, PlatformWindowType::kBubble};
  for (auto window_type : kNonToplevelWindowTypes)
    ScheduleTestTask(base::BindOnce(test, base::Unretained(this), window_type));

  // Post a wl_data_source::cancelled notifying the client to tear down the drag
  // session.
  ScheduleDragCancel();

  // Request to start the drag session, which spins a nested run loop.
  RunDragLoopWithSampleData(origin_window, DragDropTypes::DRAG_COPY);

  origin_window->SetPointerFocus(restored_focus);
}

// Ensures that requests to create a |PlatformWindowType::kPopup| during drag
// sessions return wl_subsurface-backed windows.
TEST_P(WaylandDataDragControllerTest, PopupRequestCreatesAuxiliaryWindow) {
  auto* origin_window = window_.get();
  const bool restored_focus = origin_window->has_pointer_focus();
  origin_window->SetPointerFocus(true);

  auto test = [](WaylandDataDragControllerTest* self) {
    MockPlatformWindowDelegate delegate;
    auto popup_window = self->CreateTestWindow(PlatformWindowType::kPopup,
                                               gfx::Size(100, 40), &delegate);
    popup_window->Show(false);
    self->Sync();

    auto* surface =
        self->GetMockSurface(popup_window->root_surface()->GetSurfaceId());
    ASSERT_TRUE(surface);
    EXPECT_NE(nullptr, surface->sub_surface());
  };

  ScheduleTestTask(base::BindOnce(test, base::Unretained(this)));

  // Post a wl_data_source::cancelled notifying the client to tear down the drag
  // session.
  ScheduleDragCancel();

  // Request to start the drag session, which spins a nested run loop.
  RunDragLoopWithSampleData(origin_window, DragDropTypes::DRAG_COPY);

  origin_window->SetPointerFocus(restored_focus);
}

// Ensures that requests to create a |PlatformWindowType::kMenu| during drag
// sessions return xdg_popup-backed windows.
TEST_P(WaylandDataDragControllerTest, MenuRequestCreatesPopupWindow) {
  auto* origin_window = window_.get();
  const bool restored_focus = origin_window->has_pointer_focus();
  origin_window->SetPointerFocus(true);

  auto test = [](WaylandDataDragControllerTest* self) {
    MockPlatformWindowDelegate delegate;
    auto menu_window = self->CreateTestWindow(PlatformWindowType::kMenu,
                                              gfx::Size(100, 40), &delegate);
    menu_window->Show(false);
    self->Sync();

    auto* surface =
        self->GetMockSurface(menu_window->root_surface()->GetSurfaceId());
    ASSERT_TRUE(surface);
    EXPECT_EQ(nullptr, surface->sub_surface());
  };

  ScheduleTestTask(base::BindOnce(test, base::Unretained(this)));

  // Post a wl_data_source::cancelled notifying the client to tear down the drag
  // session.
  ScheduleDragCancel();

  // Request to start the drag session, which spins a nested run loop.
  RunDragLoopWithSampleData(origin_window, DragDropTypes::DRAG_COPY);

  origin_window->SetPointerFocus(restored_focus);
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandDataDragControllerTest,
                         ::testing::Values(kXdgShellStable));

INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandDataDragControllerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
