// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>
#include <wayland-server.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_drag_drop_test.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/platform_clipboard.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_drag_handler.h"
#include "ui/platform_window/wm/wm_drop_handler.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::Values;
using ui::mojom::DragEventSource;
using ui::mojom::DragOperation;

namespace ui {
namespace {

constexpr char kSampleTextForDragAndDrop[] =
    "This is a sample text for drag-and-drop.";
constexpr char16_t kSampleTextForDragAndDrop16[] =
    u"This is a sample text for drag-and-drop.";

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

class MockDragFinishedCallback {
 public:
  MOCK_METHOD1(OnDragFinished, void(DragOperation operation));

  ui::WmDragHandler::DragFinishedCallback callback() {
    return base::BindOnce(&MockDragFinishedCallback::OnDragFinished,
                          base::Unretained(this));
  }
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
    if (on_drop_closure_) {
      on_drop_closure_.Run();
      on_drop_closure_.Reset();
    }
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

    // Set output dimensions at some offset.
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* output = server->output();
      output->SetPhysicalAndLogicalBounds({20, 30, 1200, 900});
      output->Flush();
    });

    drag_finished_callback_ = std::make_unique<MockDragFinishedCallback>();
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

  MockDragFinishedCallback* drag_finished_callback() {
    return drag_finished_callback_.get();
  }

  WaylandConnection* connection() { return connection_.get(); }

  WaylandWindow* window() { return window_.get(); }

  std::u16string sample_text_for_dnd() const {
    return kSampleTextForDragAndDrop16;
  }

  void RunMouseDragWithSampleData(WaylandWindow* origin_window,
                                  int operations) {
    ASSERT_TRUE(origin_window);
    OSExchangeData os_exchange_data;
    os_exchange_data.SetString(sample_text_for_dnd());
    origin_window->StartDrag(
        os_exchange_data, operations, DragEventSource::kMouse, /*cursor=*/{},
        /*can_grab_pointer=*/true, drag_finished_callback_->callback(),
        /*loation delegate=*/nullptr);
  }

  std::unique_ptr<WaylandWindow> CreateTestWindow(
      PlatformWindowType type,
      const gfx::Size& size,
      MockPlatformWindowDelegate* delegate,
      gfx::AcceleratedWidget context) {
    CHECK(delegate);
    PlatformWindowInitProperties properties{gfx::Rect(size)};
    properties.type = type;
    properties.parent_widget = context;
    EXPECT_CALL(*delegate, OnAcceleratedWidgetAvailable(_)).Times(1);
    auto window = WaylandWindow::Create(delegate, connection_.get(),
                                        std::move(properties));
    SetWmDropHandler(window.get(), drop_handler_.get());
    EXPECT_NE(gfx::kNullAcceleratedWidget, window->GetWidget());
    return window;
  }

  void ScheduleDragCancel() {
    ScheduleTestTask(base::BindOnce(
        [](WaylandDataDragControllerTest* self) {
          // DnD handlers expect DragLeave to be sent before DragFinished when
          // drag sessions end up with no data transfer (cancelled). Otherwise,
          // it might lead to issues like https://crbug.com/1109324.
          EXPECT_CALL(*self->drop_handler(), OnDragLeave).Times(1);
          // If DnD was cancelled, or data was dropped where it was not
          // accepted, the operation result must be None (0).
          // Regression test for https://crbug.com/1136751.
          EXPECT_CALL(*self->drag_finished_callback(),
                      OnDragFinished(DragOperation::kNone))
              .Times(1);
          self->SendDndCancelled();
        },
        base::Unretained(this)));
  }

  void ScheduleDataDeviceAction(uint32_t action) {
    ScheduleTestTask(base::BindLambdaForTesting(
        [this, action]() { SendDndAction(action); }));
  }

  void FocusAndPressLeftPointerButton(WaylandWindow* window,
                                      MockPlatformWindowDelegate* delegate) {
    SendPointerEnter(window, delegate);
    SendPointerButton(window, delegate, BTN_LEFT, /*pressed=*/true);
  }

  MockPlatformWindowDelegate* delegate() { return &delegate_; }

 protected:
  void SendMotionEvent(const gfx::Point& motion_point) {
    int64_t time =
        (EventTimeForNow() - base::TimeTicks()).InMilliseconds() & UINT32_MAX;

    PostToServerAndWait(
        [motion_point, time](wl::TestWaylandServerThread* server) {
          auto* data_device = server->data_device_manager()->data_device();
          ASSERT_TRUE(data_device);
          data_device->OnMotion(time, wl_fixed_from_int(motion_point.x()),
                                wl_fixed_from_int(motion_point.y()));
        });
  }

  std::unique_ptr<MockDropHandler> drop_handler_;
  std::unique_ptr<MockDragFinishedCallback> drag_finished_callback_;
};

TEST_P(WaylandDataDragControllerTest, StartDrag) {
  FocusAndPressLeftPointerButton(window_.get(), &delegate_);

  auto test = [](WaylandDataDragControllerTest* self) {
    // Now the server can read the data and give it to our callback.
    self->ReadAndCheckData(kMimeTypeTextUtf8, kSampleTextForDragAndDrop);

    self->SendDndCancelled();
  };

  // Post test task to be performed asynchronously once the dnd-related protocol
  // objects are ready.
  ScheduleTestTask(base::BindOnce(test, base::Unretained(this)));

  RunMouseDragWithSampleData(
      window_.get(), DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE);

  // Ensure drag delegate it properly reset when the drag loop quits.
  EXPECT_FALSE(data_device()->drag_delegate_);
}

TEST_P(WaylandDataDragControllerTest, StartDragWithWrongMimeType) {
  FocusAndPressLeftPointerButton(window_.get(), &delegate_);

  // The client starts dragging offering data with |kMimeTypeHTML|
  OSExchangeData os_exchange_data;
  os_exchange_data.SetHtml(sample_text_for_dnd(), {});
  int operations = DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
  drag_controller()->StartSession(os_exchange_data, operations,
                                  DragEventSource::kMouse);

  // The server should get an empty data buffer in ReadData callback when trying
  // to read it with a different mime type.
  ReadAndCheckData(kMimeTypeText, {});
}

// Ensures data drag controller properly offers dragged data with custom
// formats. Regression test for a bunch of bugs, such as:
//  - https://crbug.com/1236708
//  - https://crbug.com/1207607
//  - https://crbug.com/1247063
TEST_P(WaylandDataDragControllerTest, StartDragWithCustomFormats) {
  FocusAndPressLeftPointerButton(window_.get(), &delegate_);
  OSExchangeData data(OSExchangeDataProviderFactory::CreateProvider());
  ClipboardFormatType kCustomFormats[] = {
      ClipboardFormatType::WebCustomDataType(),
      ClipboardFormatType::GetType("chromium/x-bookmark-entries"),
      ClipboardFormatType::GetType("xyz/arbitrary-custom-type")};
  for (auto format : kCustomFormats)
    data.SetPickledData(format, {});

  // The client starts dragging offering pickled data with custom formats.
  drag_controller()->StartSession(data, DragDropTypes::DRAG_MOVE,
                                  DragEventSource::kMouse);

  PostToServerAndWait([kCustomFormats](wl::TestWaylandServerThread* server) {
    auto* data_source = server->data_device_manager()->data_source();
    ASSERT_TRUE(data_source);
    auto mime_types = data_source->mime_types();
    EXPECT_EQ(3u, mime_types.size());
    for (auto format : kCustomFormats) {
      EXPECT_TRUE(base::Contains(mime_types, format.GetName()))
          << "Format '" << format.GetName() << "' should be offered.";
    }
  });
}

TEST_P(WaylandDataDragControllerTest, StartDragWithText) {
  FocusAndPressLeftPointerButton(window_.get(), &delegate_);

  // The client starts dragging offering text mime type.
  OSExchangeData os_exchange_data;
  os_exchange_data.SetString(sample_text_for_dnd());
  int operations = DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
  drag_controller()->StartSession(os_exchange_data, operations,
                                  DragEventSource::kMouse);

  // The server should get a "text" representation in ReadData callback when
  // trying to read it as mime type other than |kMimeTypeText| and
  // |kTextMimeTypeUtf8|.
  ReadAndCheckData(kMimeTypeText, kSampleTextForDragAndDrop);
}

TEST_P(WaylandDataDragControllerTest, StartDragWithFileContents) {
  FocusAndPressLeftPointerButton(window_.get(), &delegate_);

  // The client starts dragging offering text mime type.
  OSExchangeData os_exchange_data;
  os_exchange_data.SetFileContents(
      base::FilePath(FILE_PATH_LITERAL("t\\est\".jpg")),
      kSampleTextForDragAndDrop);
  int operations = DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
  drag_controller()->StartSession(os_exchange_data, operations,
                                  DragEventSource::kMouse);

  constexpr char kText[] = "application/octet-stream;name=\"t\\\\est\\\".jpg\"";
  ReadAndCheckData(kText, kSampleTextForDragAndDrop);
  PostToServerAndWait([kText](wl::TestWaylandServerThread* server) {
    auto* source = server->data_device_manager()->data_source();
    ASSERT_TRUE(source);
    EXPECT_EQ(1u, source->mime_types().size());
    EXPECT_EQ(kText, source->mime_types().front());
  });
}

MATCHER_P(PointFNear, n, "") {
  return arg.IsWithinDistance(n, 0.01f);
}

TEST_P(WaylandDataDragControllerTest, ReceiveDrag) {
  const uint32_t surface_id = window_->root_surface()->get_surface_id();

  // Consume the move event from pointer enter.
  EXPECT_CALL(*drop_handler_,
              MockDragMotion(PointFNear(gfx::PointF(10, 10)), _, _));

  PostToServerAndWait([surface_id, mime_type_text = std::string(kMimeTypeText),
                       sample_text = std::string(kSampleTextForDragAndDrop)](
                          wl::TestWaylandServerThread* server) {
    // HiDPI
    server->output()->SetScale(2);
    server->output()->Flush();

    // Place the window onto the output.
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(surface_id);
    wl_surface_send_enter(surface->resource(), server->output()->resource());

    auto* data_device = server->data_device_manager()->data_device();
    auto* data_offer = data_device->CreateAndSendDataOffer();
    data_offer->OnOffer(mime_type_text, ToClipboardData(sample_text));

    gfx::Point entered_point(10, 10);
    // The server sends an enter event.
    data_device->OnEnter(server->GetNextSerial(), surface->resource(),
                         wl_fixed_from_int(entered_point.x()),
                         wl_fixed_from_int(entered_point.y()), data_offer);
  });

  ASSERT_EQ(drag_controller(), data_device()->drag_delegate_);

  // In 2x window scale, we expect received coordinates still be in DIP.
  EXPECT_CALL(*drop_handler_,
              MockDragMotion(PointFNear(gfx::PointF(30, 30)), _, _));

  // The server sends an motion event in DP.
  SendMotionEvent(gfx::Point(30, 30));

  auto callback = base::BindOnce([](PlatformClipboard::Data contents) {
    std::string result;
    EXPECT_TRUE(contents);
    result.assign(contents->front_as<char>(), contents->size());
    EXPECT_EQ(kSampleTextForDragAndDrop, result);
  });

  // The client requests the data and gets callback with it.
  data_device()->RequestData(drag_controller()->data_offer_.get(),
                             kMimeTypeText, std::move(callback));

  SendDndLeave();
  ASSERT_FALSE(data_device()->drag_delegate_);
}

TEST_P(WaylandDataDragControllerTest, ReceiveDragPixelSurface) {
  constexpr int32_t kTripleScale = 3;

  // Set connection to use pixel coordinates.
  connection_->set_surface_submission_in_pixel_coordinates(true);

  const uint32_t surface_id = window_->root_surface()->get_surface_id();
  gfx::Point entered_point{900, 600};
  {
    gfx::PointF expected_position(entered_point);
    expected_position.InvScale(kTripleScale);

    EXPECT_CALL(*drop_handler_,
                MockDragMotion(PointFNear(expected_position), _, _));

    PostToServerAndWait([surface_id, entered_point,
                         mime_type_text = std::string(kMimeTypeText),
                         sample_text = std::string(kSampleTextForDragAndDrop)](
                            wl::TestWaylandServerThread* server) {
      wl::TestOutput* output = server->output();
      // Place the window onto the output.
      wl::MockSurface* surface = server->GetObject<wl::MockSurface>(surface_id);
      wl_surface_send_enter(surface->resource(), output->resource());

      // Change the scale of the output.  Windows looking into that output must
      // get the new scale and update scale of their buffers.  The default UI
      // scale equals the output scale.
      if (output->xdg_output()) {
        // Use logical size to control the scale when the pixel coordinates
        // is enabled.
        output->SetLogicalSize({400, 300});
      } else {
        output->SetScale(kTripleScale);
      }
      output->Flush();

      auto* data_offer = server->data_device_manager()
                             ->data_device()
                             ->CreateAndSendDataOffer();
      data_offer->OnOffer(mime_type_text, ToClipboardData(sample_text));

      // The server sends an enter event at the bottom right corner of the
      // window.
      server->data_device_manager()->data_device()->OnEnter(
          1002, surface->resource(), wl_fixed_from_int(entered_point.x()),
          wl_fixed_from_int(entered_point.y()), data_offer);
    });
  }

  EXPECT_EQ(window_->applied_state().window_scale, kTripleScale);

  gfx::Point center_point{400, 300};
  {
    gfx::PointF expected_position(center_point);
    expected_position.InvScale(kTripleScale);

    EXPECT_CALL(*drop_handler_,
                MockDragMotion(PointFNear(expected_position), _, _))
        .Times(::testing::AtLeast(1));

    // The server sends a motion event through the center of the output.
    SendMotionEvent(center_point);
  }

  EXPECT_CALL(*drop_handler_,
              MockDragMotion(PointFNear(gfx::PointF(0, 0)), _, _))
      .Times(::testing::AtLeast(1));

  // The server sends a motion event to the top-left corner.
  gfx::Point top_left(0, 0);
  SendMotionEvent(top_left);
}

// Emulating an incoming DnD session, ensures that data drag controller
// fetches all the data for the mime-types offered by the source client.
TEST_P(WaylandDataDragControllerTest, DropSeveralMimeTypes) {
  const uint32_t surface_id = window_->root_surface()->get_surface_id();

  // As data for each offered mime-type is asynchronously read (eg: using
  // wl_display.sync callbacks, etc), a nested run loop is used to ensure
  // it is reliably done. Furthermore, WmDropHandler::OnDragEnter() is expected
  // to be called only once the data is fully fetched, so it's used here as
  // condition to quit the loop and verify the expectations, otherwise some
  // flakiness may be observed, see https://crbug.com/1395127.
  base::RunLoop loop;
  EXPECT_CALL(*drop_handler_, MockOnDragEnter()).WillOnce([&loop]() {
    loop.Quit();
  });
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* data_offer =
        server->data_device_manager()->data_device()->CreateAndSendDataOffer();
    data_offer->OnOffer(
        kMimeTypeText, ToClipboardData(std::string(kSampleTextForDragAndDrop)));
    data_offer->OnOffer(
        kMimeTypeMozillaURL,
        ToClipboardData(std::u16string(u"https://sample.com/\r\nSample")));
    data_offer->OnOffer(
        kMimeTypeURIList,
        ToClipboardData(std::string("file:///home/user/file\r\n")));

    gfx::Point entered_point(10, 10);
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(surface_id);
    server->data_device_manager()->data_device()->OnEnter(
        1002, surface->resource(), wl_fixed_from_int(entered_point.x()),
        wl_fixed_from_int(entered_point.y()), data_offer);
  });
  loop.Run();
  Mock::VerifyAndClearExpectations(drop_handler_.get());

  EXPECT_CALL(*drop_handler_, MockOnDragDrop()).Times(1);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->data_device_manager()->data_device()->OnDrop();
  });
  Mock::VerifyAndClearExpectations(drop_handler_.get());

  ASSERT_NE(drop_handler_->dropped_data(), nullptr);
  EXPECT_TRUE(drop_handler_->dropped_data()->HasString());
  EXPECT_TRUE(drop_handler_->dropped_data()->HasFile());
  EXPECT_TRUE(drop_handler_->dropped_data()->HasURL(kFilenameToURLPolicy));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->data_device_manager()->data_device()->OnLeave();
  });
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
    EXPECT_CALL(*drop_handler_, MockOnDragEnter()).Times(1);
    const uint32_t surface_id = window_->root_surface()->get_surface_id();
    PostToServerAndWait([surface_id, content = kCase.content](
                            wl::TestWaylandServerThread* server) {
      auto* data_offer = server->data_device_manager()
                             ->data_device()
                             ->CreateAndSendDataOffer();
      data_offer->OnOffer(kMimeTypeURIList, ToClipboardData(content));

      gfx::Point entered_point(10, 10);
      wl::MockSurface* surface = server->GetObject<wl::MockSurface>(surface_id);
      server->data_device_manager()->data_device()->OnEnter(
          server->GetNextSerial(), surface->resource(),
          wl_fixed_from_int(entered_point.x()),
          wl_fixed_from_int(entered_point.y()), data_offer);
    });

    EXPECT_CALL(*drop_handler_, MockOnDragDrop()).Times(1);
    base::RunLoop loop;
    drop_handler_->SetOnDropClosure(loop.QuitClosure());
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      server->data_device_manager()->data_device()->OnDrop();
    });
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

    EXPECT_CALL(*drop_handler_, OnDragLeave()).Times(AtMost(1));
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      server->data_device_manager()->data_device()->OnLeave();
    });
    Mock::VerifyAndClearExpectations(drop_handler_.get());
  }
}

// Tests URI validation for text/x-moz-url MIME type.  Log warnings rendered in
// the console when this test is running are the expected and valid side effect.
TEST_P(WaylandDataDragControllerTest, ValidateDroppedXMozUrl) {
  const struct {
    std::u16string content;
    std::string expected_url;
    std::u16string expected_title;
  } kCases[] = {
      {{}, {}, {}},
      {u"http://sample.com/\r\nSample", "http://sample.com/", u"Sample"},
      {u"http://title.must.be.set/", {}, {}},
      {u"url.must.be.valid/and/have.scheme\r\nInvalid URL", {}, {}},
      {u"file:///files/are/ok\r\nThe policy allows that",
       "file:///files/are/ok", u"The policy allows that"}};

  for (const auto& kCase : kCases) {
    EXPECT_CALL(*drop_handler_, MockOnDragEnter()).Times(1);
    const uint32_t surface_id = window_->root_surface()->get_surface_id();
    PostToServerAndWait([surface_id, content = kCase.content](
                            wl::TestWaylandServerThread* server) {
      auto* data_offer = server->data_device_manager()
                             ->data_device()
                             ->CreateAndSendDataOffer();
      data_offer->OnOffer(kMimeTypeMozillaURL, ToClipboardData(content));

      gfx::Point entered_point(10, 10);
      wl::MockSurface* surface = server->GetObject<wl::MockSurface>(surface_id);
      server->data_device_manager()->data_device()->OnEnter(
          server->GetNextSerial(), surface->resource(),
          wl_fixed_from_int(entered_point.x()),
          wl_fixed_from_int(entered_point.y()), data_offer);
    });

    EXPECT_CALL(*drop_handler_, MockOnDragDrop()).Times(1);
    base::RunLoop loop;
    drop_handler_->SetOnDropClosure(loop.QuitClosure());
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      server->data_device_manager()->data_device()->OnDrop();
    });
    loop.Run();
    Mock::VerifyAndClearExpectations(drop_handler_.get());

    const auto* const dropped_data = drop_handler_->dropped_data();
    if (kCase.expected_url.empty()) {
      EXPECT_FALSE(dropped_data->HasURL(kFilenameToURLPolicy));
    } else {
      EXPECT_TRUE(dropped_data->HasURL(kFilenameToURLPolicy));
      GURL url;
      std::u16string title;
      EXPECT_TRUE(
          dropped_data->GetURLAndTitle(kFilenameToURLPolicy, &url, &title));
      EXPECT_EQ(url.spec(), kCase.expected_url);
      EXPECT_EQ(title, kCase.expected_title);
    }

    EXPECT_CALL(*drop_handler_, OnDragLeave()).Times(AtMost(1));
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      server->data_device_manager()->data_device()->OnLeave();
    });
    Mock::VerifyAndClearExpectations(drop_handler_.get());
  }
}

// Verifies the correct delegate functions are called when a drag session is
// started and cancelled within the same surface.
TEST_P(WaylandDataDragControllerTest, StartAndCancel) {
  FocusAndPressLeftPointerButton(window_.get(), &delegate_);

  ScheduleDataDeviceAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
  // Schedule a wl_data_source::cancelled event to be sent asynchronously
  // once the drag session gets started.
  ScheduleDragCancel();

  RunMouseDragWithSampleData(window_.get(), DragDropTypes::DRAG_COPY);
}

TEST_P(WaylandDataDragControllerTest, ForeignDragHandleAskAction) {
  const uint32_t surface_id = window_->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* data_offer =
        server->data_device_manager()->data_device()->CreateAndSendDataOffer();
    data_offer->OnOffer(
        kMimeTypeText, ToClipboardData(std::string(kSampleTextForDragAndDrop)));
    data_offer->OnSourceActions(WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE |
                                WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
    data_offer->OnAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK);

    gfx::Point entered_point(10, 10);
    // The server sends an enter event.
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(surface_id);
    server->data_device_manager()->data_device()->OnEnter(
        server->GetNextSerial(), surface->resource(),
        wl_fixed_from_int(entered_point.x()),
        wl_fixed_from_int(entered_point.y()), data_offer);
  });

  // Verify ask handling with drop handler preferring "copy" operation.
  drop_handler_->SetPreferredOperations(ui::DragDropTypes::DRAG_COPY);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    const gfx::Point motion_point(11, 11);
    server->data_device_manager()->data_device()->OnMotion(
        server->GetNextTime(), wl_fixed_from_int(motion_point.x()),
        wl_fixed_from_int(motion_point.y()));
  });

  auto* client_data_offer = drag_controller()->data_offer_.get();
  ASSERT_TRUE(client_data_offer);
  const uint32_t client_data_offer_id = client_data_offer->id();
  PostToServerAndWait(
      [client_data_offer_id](wl::TestWaylandServerThread* server) {
        auto* data_offer =
            server->GetObject<wl::TestDataOffer>(client_data_offer_id);
        ASSERT_TRUE(data_offer);
        EXPECT_EQ(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY,
                  data_offer->preferred_action());
        EXPECT_EQ(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY,
                  data_offer->supported_actions());
        server->data_device_manager()->data_device()->OnLeave();
      });
}

// Verifies entered surface destruction is properly handled.
// Regression test for https://crbug.com/1143707.
TEST_P(WaylandDataDragControllerTest, DestroyEnteredSurface) {
  auto* window_1 = window_.get();
  FocusAndPressLeftPointerButton(window_1, &delegate_);

  ASSERT_EQ(PlatformWindowType::kWindow, window_1->type());

  auto test = [](WaylandDataDragControllerTest* self) {
    // Init and open |target_window|.
    MockPlatformWindowDelegate delegate_2;
    auto window_2 =
        self->CreateTestWindow(PlatformWindowType::kWindow, gfx::Size(80, 80),
                               &delegate_2, gfx::kNullAcceleratedWidget);

    // Leave |window_1| and enter |window_2|.
    self->SendDndLeave();
    self->SendDndEnter(window_2.get(), gfx::Point(20, 20));

    // Destroy the entered window at client side and emulates a
    // wl_data_device::leave to ensure no UAF happens.
    window_2->PrepareForShutdown();
    window_2.reset();
    self->SendDndLeave();

    // Emulate server sending an wl_data_source::cancelled event so the drag
    // loop is finished.
    self->SendDndCancelled();
  };

  // Post test task to be performed asynchronously once the drag session gets
  // started.
  ScheduleTestTask(base::BindOnce(test, base::Unretained(this)));

  RunMouseDragWithSampleData(window_.get(), DragDropTypes::DRAG_COPY);
}

// Verifies that early origin surface destruction is properly handled.
// Regression test for https://crbug.com/1143707.
TEST_P(WaylandDataDragControllerTest, DestroyOriginSurface) {
  auto* window_1 = window_.get();
  SetPointerFocusedWindow(nullptr);

  ASSERT_EQ(PlatformWindowType::kWindow, window_1->type());

  auto test = [](WaylandDataDragControllerTest* self,
                 std::unique_ptr<WaylandWindow>* origin) {
    // Leave origin surface and enter |window_|.
    self->SendDndLeave();
    self->SendDndEnter(self->window(), gfx::Point(20, 20));

    // Shutdown and destroy the popup window where the drag session was
    // initiated, which leads to the drag loop to finish.
    (*origin)->PrepareForShutdown();
    origin->reset();
  };

  // Init and open |target_window|.
  MockPlatformWindowDelegate delegate_2;
  auto window_2 =
      CreateTestWindow(PlatformWindowType::kPopup, gfx::Size(80, 80),
                       &delegate_2, window_1->GetWidget());
  FocusAndPressLeftPointerButton(window_2.get(), &delegate_);

  // Post test task to be performed asynchronously once the drag session gets
  // started.
  ScheduleTestTask(base::BindOnce(test, base::Unretained(this),
                                  base::Unretained(&window_2)));

  // Request to start the drag session, which spins a nested run loop.
  OSExchangeData os_exchange_data;
  os_exchange_data.SetString(sample_text_for_dnd());
  window_2->StartDrag(os_exchange_data, DragDropTypes::DRAG_COPY,
                      DragEventSource::kMouse, {}, true,
                      drag_finished_callback_->callback(), nullptr);

  // Send wl_data_source::cancelled event. The drag controller is then
  // expected to gracefully reset its internal state.
  SendDndLeave();
  SendDndCancelled();
}

// Ensures drag/drop events are properly propagated to non-toplevel windows.
TEST_P(WaylandDataDragControllerTest, DragToNonToplevelWindows) {
  auto* origin_window = window_.get();
  FocusAndPressLeftPointerButton(origin_window, &delegate_);

  auto test = [](WaylandDataDragControllerTest* self,
                 PlatformWindowType window_type, gfx::AcceleratedWidget context,
                 bool should_schedule_cancel) {
    // Init and open |target_window|.
    MockPlatformWindowDelegate aux_window_delegate;
    auto aux_window = self->CreateTestWindow(window_type, gfx::Size(100, 40),
                                             &aux_window_delegate, context);

    // Leave |origin_window| and enter non-toplevel |aux_window|.
    EXPECT_CALL(*self->drop_handler(), OnDragLeave).Times(1);
    self->SendDndLeave();

    EXPECT_CALL(*self->drop_handler(), MockOnDragEnter()).Times(1);
    EXPECT_CALL(*self->drop_handler(), MockDragMotion(_, _, _)).Times(1);
    self->SendDndEnter(aux_window.get(), {});

    // Goes back to |origin_window|, as |aux_window| is going to get destroyed
    // once this test task finishes.
    EXPECT_CALL(*self->drop_handler(), OnDragLeave).Times(1);
    self->SendDndLeave();

    EXPECT_CALL(*self->drop_handler(), MockOnDragEnter()).Times(1);
    EXPECT_CALL(*self->drop_handler(), MockDragMotion(_, _, _)).Times(1);
    self->SendDndEnter(self->window(), {});

    // Post a wl_data_source::cancelled notifying the client to tear down the
    // drag session.
    if (should_schedule_cancel)
      self->ScheduleDragCancel();
  };

  // Post test tasks, for each non-toplevel window type, to be performed
  // asynchronously once the dnd-related protocol objects are ready.
  constexpr PlatformWindowType kNonToplevelWindowTypes[]{
      PlatformWindowType::kPopup, PlatformWindowType::kMenu,
      PlatformWindowType::kTooltip, PlatformWindowType::kBubble};
  for (auto window_type : kNonToplevelWindowTypes) {
    // Given there is no guarantee how tasks are scheduled are executed, the end
    // of the test must only be scheduled once all the test cases run.
    const bool should_schedule_end_of_the_test =
        kNonToplevelWindowTypes[3] == window_type;
    ScheduleTestTask(base::BindOnce(test, base::Unretained(this), window_type,
                                    window_type == PlatformWindowType::kBubble
                                        ? gfx::kNullAcceleratedWidget
                                        : origin_window->GetWidget(),
                                    should_schedule_end_of_the_test));
  }

  // Request to start the drag session, which spins a nested run loop.
  RunMouseDragWithSampleData(origin_window, DragDropTypes::DRAG_COPY);
}

// Ensures that requests to create a |PlatformWindowType::kPopup| during drag
// sessions return xdg_popup-backed windows.
TEST_P(WaylandDataDragControllerTest, PopupRequestCreatesPopupWindow) {
  auto* origin_window = window_.get();
  FocusAndPressLeftPointerButton(origin_window, &delegate_);

  std::unique_ptr<WaylandWindow> popup_window;

  ScheduleTestTask(base::BindLambdaForTesting([&]() {
    MockPlatformWindowDelegate delegate;
    popup_window =
        CreateTestWindow(PlatformWindowType::kPopup, gfx::Size(100, 40),
                         &delegate, origin_window->GetWidget());
    popup_window->Show(false);
  }));

  // Post a wl_data_source::cancelled notifying the client to tear down the drag
  // session.
  ScheduleDragCancel();

  // Request to start the drag session, which spins a nested run loop.
  RunMouseDragWithSampleData(origin_window, DragDropTypes::DRAG_MOVE);

  ASSERT_TRUE(popup_window.get());
  const uint32_t surface_id = popup_window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id);
    ASSERT_TRUE(surface);
    EXPECT_NE(nullptr, surface->xdg_surface()->xdg_popup());
  });
}

// Ensures that requests to create a |PlatformWindowType::kMenu| during drag
// sessions return xdg_popup-backed windows.
TEST_P(WaylandDataDragControllerTest, MenuRequestCreatesPopupWindow) {
  auto* origin_window = window_.get();
  FocusAndPressLeftPointerButton(origin_window, &delegate_);

  auto test = [](WaylandDataDragControllerTest* self,
                 gfx::AcceleratedWidget context) {
    MockPlatformWindowDelegate delegate;
    auto menu_window = self->CreateTestWindow(
        PlatformWindowType::kMenu, gfx::Size(100, 40), &delegate, context);
    menu_window->Show(false);

    const uint32_t surface_id = menu_window->root_surface()->get_surface_id();
    self->PostToServerAndWait(
        [surface_id](wl::TestWaylandServerThread* server) {
          auto* surface = server->GetObject<wl::MockSurface>(surface_id);
          ASSERT_TRUE(surface);
          EXPECT_EQ(nullptr, surface->sub_surface());
        });
  };

  ScheduleTestTask(
      base::BindOnce(test, base::Unretained(this), origin_window->GetWidget()));

  // Post a wl_data_source::cancelled notifying the client to tear down the drag
  // session.
  ScheduleDragCancel();

  // Request to start the drag session, which spins a nested run loop.
  RunMouseDragWithSampleData(origin_window, DragDropTypes::DRAG_COPY);
}

// Regression test for https://crbug.com/1209269.
//
// Emulates "quick" wl_pointer.button release events being sent by the
// compositor, and processed by the ozone/wayland either (1) before or (2) just
// after WaylandWindow::StartDrag is called. The drag start happens in
// response to sequence of input events. Such event processing may take some
// time, for example, when they happen in web contents, which involves async
// browser <=> renderer IPC, etc. In both cases, drag controller is expected to
// gracefully reset state and quit drag loop as if the drag session was
// cancelled as usual.
TEST_P(WaylandDataDragControllerTest, AsyncNoopStartDrag) {
  OSExchangeData os_exchange_data;
  os_exchange_data.SetString(sample_text_for_dnd());

  // 1. Send wl_pointer.button release before drag start.
  FocusAndPressLeftPointerButton(window_.get(), &delegate_);
  SendPointerButton(window_.get(), &delegate_, BTN_LEFT, /*pressed=*/false);

  // Attempt to start drag session and ensure it fails.
  bool result_1 = window_->StartDrag(
      os_exchange_data, DragDropTypes::DRAG_COPY, DragEventSource::kMouse,
      /*cursor=*/{},
      /*can_grab_pointer=*/true, drag_finished_callback_->callback(), nullptr);
  EXPECT_FALSE(result_1);
  Mock::VerifyAndClearExpectations(drop_handler_.get());
  EXPECT_FALSE(drag_controller()->origin_window_);
  EXPECT_FALSE(drag_controller()->nested_dispatcher_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Drag mustn't be started. Availability of data_source can be used to
    // determine if the client has initiated a drag session.
    ASSERT_FALSE(server->data_device_manager()->data_source());
  });

  // 2. Send wl_pointer.button release just after drag start.
  FocusAndPressLeftPointerButton(window_.get(), &delegate_);

  // Schedule a wl_pointer.button up, attempt to start drag session and ensure
  // it exits with cancellation status.
  ScheduleTestTask(base::BindLambdaForTesting([&]() {
    EXPECT_CALL(*drop_handler_, OnDragLeave).Times(1);
    EXPECT_CALL(*drag_finished_callback_,
                OnDragFinished(Eq(DragOperation::kNone)))
        .Times(1);
    SendPointerButton(window_.get(), &delegate_, BTN_LEFT, /*pressed=*/false);
  }));

  bool result_2 = window_->StartDrag(
      os_exchange_data, DragDropTypes::DRAG_COPY, DragEventSource::kMouse,
      /*cursor=*/{},
      /*can_grab_pointer=*/true, drag_finished_callback_->callback(), nullptr);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Drag must be started. Availability of data_source can be used to
    // determine if the client has initiated a drag session.
    ASSERT_TRUE(server->data_device_manager()->data_source());
  });
  // TODO(crbug.com/1022722): Double-check if this should return false instead.
  EXPECT_TRUE(result_2);
  Mock::VerifyAndClearExpectations(drop_handler_.get());
  Mock::VerifyAndClearExpectations(drag_finished_callback_.get());
  EXPECT_FALSE(drag_controller()->origin_window_);
  EXPECT_FALSE(drag_controller()->nested_dispatcher_);
}

// Regression test for https://crbug.com/1175083.
TEST_P(WaylandDataDragControllerTest, StartDragWithCorrectSerial) {
  FocusAndPressLeftPointerButton(window_.get(), &delegate_);
  absl::optional<wl::Serial> mouse_press_serial =
      connection()->serial_tracker().GetSerial(wl::SerialType::kMousePress);
  ASSERT_TRUE(mouse_press_serial.has_value());

  // Emulate a wl_keyboard.key press event being processed by the compositor
  // before the drag starts. In this case, the client is expected to send the
  // correct serial value when starting the drag session (ie: the one received
  // with wl_pointer.button).
  const uint32_t surface_id = window_->root_surface()->get_surface_id();
  PostToServerAndWait([mouse_serial = mouse_press_serial->value,
                       surface_id](wl::TestWaylandServerThread* server) {
    auto* keyboard = server->seat()->keyboard();
    ASSERT_TRUE(keyboard);
    wl::ScopedWlArray empty({});
    auto* surface = server->GetObject<wl::MockSurface>(surface_id);
    wl_keyboard_send_enter(keyboard->resource(), 1, surface->resource(),
                           empty.get());
    wl_keyboard_send_key(keyboard->resource(), (mouse_serial + 1), 0,
                         30 /* a */, WL_KEYBOARD_KEY_STATE_PRESSED);
  });

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Drag must be started. Availability of data_source can be used to
    // determine if the client has initiated a drag session.
    ASSERT_FALSE(server->data_device_manager()->data_source());
  });

  // Post a wl_data_source::cancelled notifying the client to tear down the drag
  // session.
  ScheduleDragCancel();

  // Request to start the drag session, which spins a nested run loop, and
  // ensure correct serial (mouse button press) is sent to the server with
  // wl_data_device.start_drag request.
  RunMouseDragWithSampleData(window_.get(), DragDropTypes::DRAG_COPY);
  Mock::VerifyAndClearExpectations(drop_handler_.get());
  PostToServerAndWait([serial = mouse_press_serial->value](
                          wl::TestWaylandServerThread* server) {
    // Drag must be started. Availability of data_source can be used to
    // determine if the client has initiated a drag session.
    EXPECT_EQ(serial,
              server->data_device_manager()->data_device()->drag_serial());
    ASSERT_TRUE(server->data_device_manager()->data_source());
  });
}

// Check drag session is correctly started when there are both mouse button and
// a touch point pressed.
TEST_P(WaylandDataDragControllerTest, StartDragWithCorrectSerialForDragSource) {
  OSExchangeData os_exchange_data;
  os_exchange_data.SetString(sample_text_for_dnd());

  auto* const window_manager = connection_->window_manager();
  ASSERT_FALSE(window_manager->GetCurrentPointerFocusedWindow());
  ASSERT_FALSE(window_manager->GetCurrentTouchFocusedWindow());

  FocusAndPressLeftPointerButton(window_.get(), &delegate_);
  absl::optional<wl::Serial> mouse_press_serial =
      connection()->serial_tracker().GetSerial(wl::SerialType::kMousePress);
  ASSERT_TRUE(mouse_press_serial.has_value());
  ASSERT_EQ(window_.get(), window_manager->GetCurrentPointerFocusedWindow());
  ASSERT_FALSE(window_manager->GetCurrentTouchFocusedWindow());

  // Check drag does not start when requested with kTouch drag source, even when
  // there is a mouse button pressed (ie: kMousePress serial available).
  bool drag_started = window_->StartDrag(
      os_exchange_data, DragDropTypes::DRAG_COPY, DragEventSource::kTouch,
      /*cursor=*/{}, /*can_grab_pointer=*/true,
      drag_finished_callback_->callback(), nullptr);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Drag must be started. Availability of data_source can be used to
    // determine if the client has initiated a drag session.
    ASSERT_FALSE(server->data_device_manager()->data_source());
  });
  EXPECT_FALSE(drag_started);
  Mock::VerifyAndClearExpectations(drop_handler_.get());

  SendTouchDown(window_.get(), &delegate_, 1, {30, 30});
  ASSERT_EQ(window_.get(), window_manager->GetCurrentTouchFocusedWindow());
  ASSERT_EQ(window_.get(), window_manager->GetCurrentPointerFocusedWindow());

  // Schedule dnd session cancellation so that the drag loop gracefully exits.
  ScheduleDragCancel();

  // Check drag is started with correct serial value, as per the drag source
  // passed in, even when it is not the most recent serial, ie: touch down
  // received after mouse button press.
  bool success = window_->StartDrag(
      os_exchange_data, DragDropTypes::DRAG_COPY, DragEventSource::kMouse,
      /*cursor=*/{}, /*can_grab_pointer=*/true,
      drag_finished_callback_->callback(), nullptr);
  EXPECT_TRUE(success);
  Mock::VerifyAndClearExpectations(drop_handler_.get());
  PostToServerAndWait([mouse_serial = mouse_press_serial->value](
                          wl::TestWaylandServerThread* server) {
    // Drag must be started. Availability of data_source can be used to
    // determine if the client has initiated a drag session.
    EXPECT_EQ(mouse_serial,
              server->data_device_manager()->data_device()->drag_serial());
    ASSERT_TRUE(server->data_device_manager()->data_source());
  });
}

// With an incoming DnD session, this ensures that data drag controller
// gracefully handles drop events received while the data fetching is still
// unfinished. Regression test for https://crbug.com/1400872.
TEST_P(WaylandDataDragControllerTest, DropWhileFetchingData) {
  const uint32_t surface_id = window_->root_surface()->get_surface_id();

  // Data for each offered mime-type is asynchronously read (eg: using
  // wl_display.sync callbacks, etc), so a single roundtrip - done implicitly
  // in PostToServerAndWait() impl - isn't enough to fetch all the data, which
  // is exactly the goal in this test case, so that we can send the "early" drop
  // and ensure it does not crash in the next step.
  EXPECT_CALL(*drop_handler_, MockOnDragEnter()).Times(0);
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* data_device = server->data_device_manager()->data_device();
    auto* data_offer = data_device->CreateAndSendDataOffer();
    data_offer->OnOffer(
        kMimeTypeText, ToClipboardData(std::string(kSampleTextForDragAndDrop)));

    auto* surface = server->GetObject<wl::MockSurface>(surface_id);
    data_device->OnEnter(server->GetNextSerial(), surface->resource(),
                         wl_fixed_from_int(0), wl_fixed_from_int(0),
                         data_offer);

    // Sending `drop` right after `enter` here ensures drag controller has not
    // yet had the chance to (even) start the data fetching, which helps
    // avoiding flakiness (quite common in this kind of test case).
    data_device->OnDrop();
  });
  Mock::VerifyAndClearExpectations(drop_handler_.get());

  EXPECT_FALSE(drop_handler_->dropped_data());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->data_device_manager()->data_device()->OnLeave();
  });
}

// Regression test for https://crbug.com/1405176.
TEST_P(WaylandDataDragControllerTest, DndActionsToDragOperations) {
  const uint32_t surface_id = window_->root_surface()->get_surface_id();

  // Consume the move event from pointer enter.
  EXPECT_CALL(*drop_handler_, MockDragMotion(_, _, _));

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    // Place the window onto the output.
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(surface_id);
    wl_surface_send_enter(surface->resource(), server->output()->resource());

    auto* data_device = server->data_device_manager()->data_device();
    auto* data_offer = data_device->CreateAndSendDataOffer();
    data_offer->OnSourceActions(WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE |
                                WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                                WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK);

    gfx::Point entered_point(10, 10);
    // The server sends an enter event.
    data_device->OnEnter(server->GetNextSerial(), surface->resource(),
                         wl_fixed_from_int(entered_point.x()),
                         wl_fixed_from_int(entered_point.y()), data_offer);
  });

  EXPECT_CALL(*drop_handler_, MockDragMotion(_,
                                             DragDropTypes::DRAG_COPY |
                                                 DragDropTypes::DRAG_MOVE |
                                                 DragDropTypes::DRAG_LINK,
                                             _));

  SendMotionEvent(gfx::Point(10, 10));
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandDataDragControllerTest,
                         Values(wl::ServerConfig{}));

INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithAuraShell,
    WaylandDataDragControllerTest,
    Values(wl::ServerConfig{
        .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled}));

}  // namespace ui
