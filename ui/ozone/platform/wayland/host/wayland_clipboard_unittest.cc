// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"

#include <linux/input.h>
#include <wayland-server.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_touch.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/platform_clipboard.h"

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Mock;
using testing::Values;

namespace ui {

namespace {

constexpr char kSampleClipboardText[] = "This is a sample text for clipboard.";

template <typename StringType>
ui::PlatformClipboard::Data ToClipboardData(const StringType& data_string) {
  std::vector<uint8_t> data_vector;
  data_vector.assign(data_string.begin(), data_string.end());
  return base::RefCountedBytes::TakeVector(&data_vector);
}

// This must be called on the server thread.
wl::TestSelectionDevice* GetSelectionDevice(wl::TestWaylandServerThread* server,
                                            ClipboardBuffer buffer) {
  DCHECK(server->task_runner()->BelongsToCurrentThread());

  return buffer == ClipboardBuffer::kSelection
             ? server->primary_selection_device_manager()->device()
             : server->data_device_manager()->data_device();
}

// This must be called on the server thread.
wl::TestSelectionSource* GetSelectionSource(wl::TestWaylandServerThread* server,
                                            ClipboardBuffer buffer) {
  DCHECK(server->task_runner()->BelongsToCurrentThread());

  return buffer == ClipboardBuffer::kSelection
             ? server->primary_selection_device_manager()->source()
             : server->data_device_manager()->data_source();
}

}  // namespace

class WaylandClipboardTestBase : public WaylandTest {
 public:
  WaylandClipboardTestBase() = default;
  WaylandClipboardTestBase(const WaylandClipboardTestBase&) = delete;
  WaylandClipboardTestBase& operator=(const WaylandClipboardTestBase&) = delete;
  ~WaylandClipboardTestBase() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl_seat_send_capabilities(server->seat()->resource(),
                                WL_SEAT_CAPABILITY_POINTER |
                                    WL_SEAT_CAPABILITY_TOUCH |
                                    WL_SEAT_CAPABILITY_KEYBOARD);

      ASSERT_TRUE(server->data_device_manager());
    });
    ASSERT_TRUE(connection_->seat()->pointer());
    ASSERT_TRUE(connection_->seat()->touch());
    ASSERT_TRUE(connection_->seat()->keyboard());

    clipboard_ = connection_->clipboard();
    ASSERT_TRUE(clipboard_);

    MaybeSetUpXkb();
  }

  void TearDown() override {
    WaylandTest::TearDown();
    clipboard_ = nullptr;
  }

 protected:
  // Ensure the requests/events are flushed and posted tasks get processed.
  // wl::TestSelection{Source,Offer} use ThreadPool task runners to read/write
  // selection data, so the pool must be explicitly flushed as well.
  void WaitForClipboardTasks() {
    WaylandTestBase::SyncDisplay();
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void SentPointerButtonPress(const gfx::Point& location) {
    PostToServerAndWait(
        [&location, surface_id = window_->root_surface()->get_surface_id()](
            wl::TestWaylandServerThread* server) {
          auto* const pointer = server->seat()->pointer()->resource();
          auto* const surface =
              server->GetObject<wl::MockSurface>(surface_id)->resource();

          wl_pointer_send_enter(pointer, server->GetNextSerial(), surface,
                                wl_fixed_from_int(location.x()),
                                wl_fixed_from_int(location.y()));
          wl_pointer_send_button(pointer, server->GetNextSerial(),
                                 server->GetNextTime(), BTN_LEFT,
                                 WL_POINTER_BUTTON_STATE_PRESSED);
        });
  }
  void SendTouchDown(const gfx::Point& location) {
    PostToServerAndWait(
        [location, surface_id = window_->root_surface()->get_surface_id()](
            wl::TestWaylandServerThread* server) {
          auto* const touch = server->seat()->touch()->resource();
          auto* const surface =
              server->GetObject<wl::MockSurface>(surface_id)->resource();

          wl_touch_send_down(touch, server->GetNextSerial(),
                             server->GetNextTime(), surface, 0 /* id */,
                             wl_fixed_from_int(location.x()),
                             wl_fixed_from_int(location.y()));
          wl_touch_send_frame(touch);
        });
  }

  void SendTouchUp() {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* const touch = server->seat()->touch()->resource();

      wl_touch_send_up(touch, server->GetNextSerial(), server->GetNextTime(),
                       0 /* id */);
    });
  }

  void SendKeyboardKey() {
    PostToServerAndWait(
        [surface_id = window_->root_surface()->get_surface_id()](
            wl::TestWaylandServerThread* server) {
          auto* const keyboard = server->seat()->keyboard()->resource();
          auto* const surface =
              server->GetObject<wl::MockSurface>(surface_id)->resource();

          wl::ScopedWlArray empty({});
          wl_keyboard_send_enter(keyboard, server->GetNextSerial(), surface,
                                 empty.get());
          wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                               server->GetNextTime(), 30 /* a */,
                               WL_KEYBOARD_KEY_STATE_PRESSED);
        });
  }

  raw_ptr<WaylandClipboard> clipboard_ = nullptr;
};

class WaylandClipboardTest : public WaylandClipboardTestBase {
 public:
  WaylandClipboardTest() = default;
  WaylandClipboardTest(const WaylandClipboardTest&) = delete;
  WaylandClipboardTest& operator=(const WaylandClipboardTest&) = delete;
  ~WaylandClipboardTest() override = default;

  void SetUp() override {
    WaylandClipboardTestBase::SetUp();

    PostToServerAndWait(
        [primary_selection_protocol = GetParam().primary_selection_protocol](
            wl::TestWaylandServerThread* server) {
          ASSERT_TRUE(primary_selection_protocol ==
                          wl::PrimarySelectionProtocol::kNone ||
                      server->primary_selection_device_manager());
        });

    // Make sure clipboard instance for the available primary selection protocol
    // gets properly created, ie: the corresponding 'get_device' request is
    // issued, so server-side objects are created prior to test-case specific
    // calls, otherwise tests, such as ReadFromClipboard, would crash.
    ASSERT_EQ(WhichBufferToUse() == ClipboardBuffer::kSelection,
              !!clipboard_->GetClipboard(ClipboardBuffer::kSelection));
    WaylandTestBase::SyncDisplay();

    offered_data_.clear();
  }

 protected:
  ClipboardBuffer WhichBufferToUse() const {
    return GetParam().primary_selection_protocol !=
                   wl::PrimarySelectionProtocol::kNone
               ? ClipboardBuffer::kSelection
               : ClipboardBuffer::kCopyPaste;
  }

  // Fill the clipboard backing store with sample data.
  void OfferData(ClipboardBuffer buffer,
                 std::string_view data,
                 const std::string& mime_type) {
    std::vector<uint8_t> data_vector(data.begin(), data.end());
    offered_data_[mime_type] = base::RefCountedBytes::TakeVector(&data_vector);

    base::MockCallback<PlatformClipboard::OfferDataClosure> offer_callback;
    EXPECT_CALL(offer_callback, Run()).Times(1);
    clipboard_->OfferClipboardData(buffer, offered_data_, offer_callback.Get());
  }

  PlatformClipboard::DataMap offered_data_;
};

class CopyPasteOnlyClipboardTest : public WaylandClipboardTestBase {
 public:
  void SetUp() override {
    WaylandClipboardTestBase::SetUp();

    ASSERT_FALSE(clipboard_->IsSelectionBufferAvailable());

    ASSERT_EQ(wl::PrimarySelectionProtocol::kNone,
              GetParam().primary_selection_protocol);

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      ASSERT_TRUE(server->data_device_manager());
      ASSERT_FALSE(server->primary_selection_device_manager());
    });
  }
};

// Verifies that copy-to-clipboard works as expected. Actual Wayland input
// events are used in order to exercise all the components involved, e.g:
// Wayland{Pointer,Keyboard,Touch}, Serial tracker and WaylandClipboard.
//
// Regression test for https://crbug.com/1282220.
// TODO(crbug.com/41495216): Flaky test.
TEST_P(WaylandClipboardTest, DISABLED_WriteToClipboard) {
  const base::RepeatingClosure send_input_event_closures[]{
      // Mouse button press
      base::BindLambdaForTesting([&]() {
        SentPointerButtonPress({10, 10});
      }),
      // Key press
      base::BindLambdaForTesting([&]() { SendKeyboardKey(); }),
      // Touch down
      base::BindLambdaForTesting([&]() {
        SendTouchDown({200, 200});
      }),
      // Touch tap (down > up)
      base::BindLambdaForTesting([&]() {
        SendTouchDown({300, 300});
        SendTouchUp();
      })};

  auto* window_manager = connection_->window_manager();

  // Triggering copy on touch-down event.
  for (auto send_input_event : send_input_event_closures) {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      if (GetParam().primary_selection_protocol !=
          wl::PrimarySelectionProtocol::kNone) {
        server->primary_selection_device_manager()->set_source(nullptr);
      } else {
        server->data_device_manager()->set_data_source(nullptr);
      }
    });

    send_input_event.Run();
    auto client_selection_serial = connection_->serial_tracker().GetSerial(
        {wl::SerialType::kTouchPress, wl::SerialType::kMousePress,
         wl::SerialType::kKeyPress});
    ASSERT_TRUE(client_selection_serial.has_value());

    // 1. Offer sample text as selection data.
    OfferData(WhichBufferToUse(), kSampleClipboardText, {kMimeTypeTextUtf8});
    WaylandTestBase::SyncDisplay();

    // 2. Emulate an external client requesting to read the offered data and
    // make sure the appropriate string gets delivered.
    // These three objects are accessed from the server thread, but must persist
    // several server calls, that is why they are allocated here.
    std::string delivered_text;
    base::MockCallback<wl::TestSelectionSource::ReadDataCallback> callback;
    base::RunLoop run_loop;

    PostToServerAndWait([&delivered_text, &callback,
                         buffer = WhichBufferToUse(),
                         serial = client_selection_serial->value,
                         quit_closure = run_loop.QuitClosure()](
                            wl::TestWaylandServerThread* server) {
      ASSERT_TRUE(GetSelectionSource(server, buffer));

      EXPECT_EQ(serial, GetSelectionDevice(server, buffer)->selection_serial());

      EXPECT_CALL(callback, Run(_))
          .WillOnce(
              [&delivered_text, quit_closure](std::vector<uint8_t>&& data) {
                delivered_text = std::string(data.begin(), data.end());
                quit_closure.Run();
              });

      GetSelectionSource(server, buffer)
          ->ReadData(kMimeTypeTextUtf8, callback.Get());
    });

    WaitForClipboardTasks();

    PostToServerAndWait([&delivered_text](wl::TestWaylandServerThread* server) {
      EXPECT_EQ(kSampleClipboardText, delivered_text);
    });

    window_manager->SetPointerFocusedWindow(nullptr);
    window_manager->SetTouchFocusedWindow(nullptr);
    window_manager->SetKeyboardFocusedWindow(nullptr);
  }
}

TEST_P(WaylandClipboardTest, ReadFromClipboard) {
  // 1. Emulate a selection data offer coming in.
  PostToServerAndWait(
      [buffer = WhichBufferToUse()](wl::TestWaylandServerThread* server) {
        auto* device = GetSelectionDevice(server, buffer);
        auto* data_offer = device->OnDataOffer();
        data_offer->OnOffer(kMimeTypeTextUtf8,
                            ToClipboardData(std::string(kSampleClipboardText)));
        device->OnSelection(data_offer);
      });

  // 2. Request to read the offered data and check whether the read text matches
  // the previously offered one.
  std::string text;
  base::MockCallback<PlatformClipboard::RequestDataClosure> callback;
  EXPECT_CALL(callback, Run(_)).WillOnce([&text](PlatformClipboard::Data data) {
    ASSERT_TRUE(data);
    text = std::string(base::as_string_view(*data));
  });

  clipboard_->RequestClipboardData(WhichBufferToUse(), kMimeTypeTextUtf8,
                                   callback.Get());
  EXPECT_EQ(kSampleClipboardText, text);
}

// Regression test for crbug.com/1183939. Ensures unicode mime types take
// priority over text/plain when reading text.
TEST_P(WaylandClipboardTest, ReadFromClipboardPrioritizeUtf) {
  PostToServerAndWait(
      [buffer = WhichBufferToUse()](wl::TestWaylandServerThread* server) {
        auto* data_offer = GetSelectionDevice(server, buffer)->OnDataOffer();
        data_offer->OnOffer(kMimeTypeText,
                            ToClipboardData(std::string("ascii_text")));
        data_offer->OnOffer(kMimeTypeTextUtf8,
                            ToClipboardData(std::string("utf8_text")));
        GetSelectionDevice(server, buffer)->OnSelection(data_offer);
      });

  std::string text;
  base::MockCallback<PlatformClipboard::RequestDataClosure> callback;
  EXPECT_CALL(callback, Run(_)).WillOnce([&text](PlatformClipboard::Data data) {
    ASSERT_TRUE(data);
    text = std::string(base::as_string_view(*data));
  });

  clipboard_->RequestClipboardData(WhichBufferToUse(), kMimeTypeTextUtf8,
                                   callback.Get());
  EXPECT_EQ("utf8_text", text);
}

TEST_P(WaylandClipboardTest, ReadFromClipboardWithoutOffer) {
  // When no data offer is advertised and client requests clipboard data from
  // the server, the response callback should be gracefully called with null
  // data.
  base::MockCallback<PlatformClipboard::RequestDataClosure> callback;
  EXPECT_CALL(callback, Run(Eq(nullptr))).Times(1);
  clipboard_->RequestClipboardData(WhichBufferToUse(), kMimeTypeTextUtf8,
                                   callback.Get());
}

TEST_P(WaylandClipboardTest, IsSelectionOwner) {
  connection_->serial_tracker().UpdateSerial(wl::SerialType::kMousePress, 1);

  OfferData(WhichBufferToUse(), kSampleClipboardText, {kMimeTypeTextUtf8});

  PostToServerAndWait(
      [buffer = WhichBufferToUse()](wl::TestWaylandServerThread* server) {
        ASSERT_TRUE(GetSelectionSource(server, buffer));
      });

  ASSERT_TRUE(clipboard_->IsSelectionOwner(WhichBufferToUse()));

  // The compositor sends OnCancelled whenever another application on the system
  // sets a new selection. It means we are not the application that owns the
  // current selection data.
  PostToServerAndWait(
      [buffer = WhichBufferToUse()](wl::TestWaylandServerThread* server) {
        GetSelectionSource(server, buffer)->OnCancelled();
      });

  ASSERT_FALSE(clipboard_->IsSelectionOwner(WhichBufferToUse()));

  connection_->serial_tracker().ResetSerial(wl::SerialType::kMousePress);
}

// Ensures WaylandClipboard correctly handles overlapping read requests for
// different clipboard buffers.
TEST_P(WaylandClipboardTest, OverlapReadingFromDifferentBuffers) {
  // Offer a sample text as selection data.
  PostToServerAndWait(
      [buffer = WhichBufferToUse()](wl::TestWaylandServerThread* server) {
        auto* data_offer = GetSelectionDevice(server, buffer)->OnDataOffer();
        data_offer->OnOffer(kMimeTypeTextUtf8,
                            ToClipboardData(std::string(kSampleClipboardText)));
        GetSelectionDevice(server, buffer)->OnSelection(data_offer);
      });

  // Post a read request for the other buffer, which will start its execution
  // after the request above.
  auto other_buffer = WhichBufferToUse() == ClipboardBuffer::kSelection
                          ? ClipboardBuffer::kCopyPaste
                          : ClipboardBuffer::kSelection;
  base::MockCallback<PlatformClipboard::RequestDataClosure> callback;
  EXPECT_CALL(callback, Run(Eq(nullptr))).Times(1);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PlatformClipboard::RequestClipboardData,
                                base::Unretained(clipboard_), other_buffer,
                                kMimeTypeTextUtf8, callback.Get()));

  // Instantly start a clipboard read request for kCopyPaste buffer (the actual
  // data transfer will take place asynchronously. See WaylandDataDevice impl)
  // and ensure read callback is called with the expected resulting data,
  // regardless any other request that may arrive in the meantime.
  std::string text;
  base::MockCallback<PlatformClipboard::RequestDataClosure> got_text;
  EXPECT_CALL(got_text, Run(_)).WillOnce([&](PlatformClipboard::Data data) {
    ASSERT_NE(nullptr, data);
    text = std::string(base::as_string_view(*data));
  });
  clipboard_->RequestClipboardData(WhichBufferToUse(), kMimeTypeTextUtf8,
                                   got_text.Get());

  WaitForClipboardTasks();
  EXPECT_EQ(kSampleClipboardText, text);
}

// Ensures clipboard change callback is fired only once per read/write.
TEST_P(WaylandClipboardTest, ClipboardChangeNotifications) {
  base::MockCallback<PlatformClipboard::ClipboardDataChangedCallback>
      clipboard_changed_callback;
  clipboard_->SetClipboardDataChangedCallback(clipboard_changed_callback.Get());
  const auto buffer = WhichBufferToUse();

  // 1. For selection offered by an external application.
  EXPECT_CALL(clipboard_changed_callback, Run(buffer)).Times(1);
  PostToServerAndWait(
      [buffer = WhichBufferToUse()](wl::TestWaylandServerThread* server) {
        auto* data_offer = GetSelectionDevice(server, buffer)->OnDataOffer();
        data_offer->OnOffer(kMimeTypeTextUtf8,
                            ToClipboardData(std::string(kSampleClipboardText)));
        GetSelectionDevice(server, buffer)->OnSelection(data_offer);
      });
  EXPECT_FALSE(clipboard_->IsSelectionOwner(buffer));

  // 2. For selection offered by Chromium.
  connection_->serial_tracker().UpdateSerial(wl::SerialType::kMousePress, 1);
  EXPECT_CALL(clipboard_changed_callback, Run(buffer)).Times(1);
  OfferData(buffer, kSampleClipboardText, {kMimeTypeTextUtf8});
  PostToServerAndWait(
      [buffer = WhichBufferToUse()](wl::TestWaylandServerThread* server) {
        ASSERT_TRUE(GetSelectionSource(server, buffer));
      });
  EXPECT_TRUE(clipboard_->IsSelectionOwner(buffer));
  connection_->serial_tracker().ResetSerial(wl::SerialType::kMousePress);
}

// Verifies clipboard calls targeting primary selection buffer no-op and run
// gracefully when no primary selection protocol is available.
TEST_P(CopyPasteOnlyClipboardTest, PrimarySelectionRequestsNoop) {
  const auto buffer = ClipboardBuffer::kSelection;

  base::MockCallback<PlatformClipboard::OfferDataClosure> offer_done;
  EXPECT_CALL(offer_done, Run()).Times(1);
  clipboard_->OfferClipboardData(buffer, {}, offer_done.Get());
  EXPECT_FALSE(clipboard_->IsSelectionOwner(buffer));

  base::MockCallback<PlatformClipboard::RequestDataClosure> got_data;
  EXPECT_CALL(got_data, Run(IsNull())).Times(1);
  clipboard_->RequestClipboardData(buffer, kMimeTypeTextUtf8, got_data.Get());

  base::MockCallback<PlatformClipboard::GetMimeTypesClosure> got_mime_types;
  EXPECT_CALL(got_mime_types, Run(IsEmpty())).Times(1);
  clipboard_->GetAvailableMimeTypes(buffer, got_mime_types.Get());
}

// Makes sure overlapping read requests for the same clipboard buffer are
// properly handled.
// TODO(crbug.com/40398800): Re-enable once Clipboard API becomes async.
TEST_P(CopyPasteOnlyClipboardTest, DISABLED_OverlappingReadRequests) {
  // Create an selection data offer containing plain and html mime types.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* data_device = server->data_device_manager()->data_device();
    auto* data_offer = data_device->OnDataOffer();
    data_offer->OnOffer(kMimeTypeText, ToClipboardData(std::string("text")));
    data_offer->OnOffer(kMimeTypeHTML, ToClipboardData(std::string("html")));
    data_device->OnSelection(data_offer);
  });

  // Schedule a clipboard read task (for text/html mime type). As read requests
  // are processed asynchronously, this will actually start when the request
  // below is already under processing, thus emulating 2 "overlapping" requests.
  std::string html;
  base::MockCallback<PlatformClipboard::RequestDataClosure> got_html;
  EXPECT_CALL(got_html, Run(_)).WillOnce([&](PlatformClipboard::Data data) {
    html = std::string(base::as_string_view(*data));
  });
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformClipboard::RequestClipboardData,
                     base::Unretained(clipboard_), ClipboardBuffer::kCopyPaste,
                     kMimeTypeHTML, got_html.Get()));

  // Instantly start a read request for text/plain mime type.
  std::string text;
  base::MockCallback<PlatformClipboard::RequestDataClosure> got_text;
  EXPECT_CALL(got_text, Run(_)).WillOnce([&](PlatformClipboard::Data data) {
    text = std::string(base::as_string_view(*data));
  });
  clipboard_->RequestClipboardData(ClipboardBuffer::kCopyPaste, kMimeTypeText,
                                   got_text.Get());

  // Wait for clipboard tasks to complete and ensure both requests were
  // processed correctly.
  WaitForClipboardTasks();
  EXPECT_EQ("html", html);
  EXPECT_EQ("text", text);
}

INSTANTIATE_TEST_SUITE_P(
    WithZwpPrimarySelection,
    WaylandClipboardTest,
    Values(wl::ServerConfig{
        .primary_selection_protocol = wl::PrimarySelectionProtocol::kZwp}));

INSTANTIATE_TEST_SUITE_P(
    WithGtkPrimarySelection,
    WaylandClipboardTest,
    Values(wl::ServerConfig{
        .primary_selection_protocol = wl::PrimarySelectionProtocol::kGtk}));

INSTANTIATE_TEST_SUITE_P(
    WithoutPrimarySelection,
    WaylandClipboardTest,
    Values(wl::ServerConfig{
        .primary_selection_protocol = wl::PrimarySelectionProtocol::kNone}));

INSTANTIATE_TEST_SUITE_P(
    WithoutPrimarySelection,
    CopyPasteOnlyClipboardTest,
    Values(wl::ServerConfig{
        .primary_selection_protocol = wl::PrimarySelectionProtocol::kNone}));

}  // namespace ui
