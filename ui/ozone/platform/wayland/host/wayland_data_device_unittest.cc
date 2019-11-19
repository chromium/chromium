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
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/base_event_utils.h"
#include "ui/ozone/platform/wayland/test/constants.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/platform_clipboard.h"
#include "ui/platform_window/platform_window_handler/wm_drop_handler.h"
#include "url/gurl.h"

using testing::_;
using testing::Mock;

namespace ui {

namespace {

constexpr OSExchangeData::FilenameToURLPolicy kFilenameToURLPolicy =
    OSExchangeData::FilenameToURLPolicy::CONVERT_FILENAMES;

template <typename StringType>
ui::PlatformClipboard::Data ToClipboardData(const StringType& data_string) {
  ui::PlatformClipboard::Data result;
  auto* begin =
      reinterpret_cast<typename ui::PlatformClipboard::Data::const_pointer>(
          data_string.data());
  result.assign(begin, begin + (data_string.size() *
                                sizeof(typename StringType::value_type)));
  return result;
}

}  // namespace

class MockDropHandler : public WmDropHandler {
 public:
  MockDropHandler() = default;
  ~MockDropHandler() override {}

  MOCK_METHOD3(OnDragEnter,
               void(const gfx::PointF& point,
                    std::unique_ptr<OSExchangeData> data,
                    int operation));
  MOCK_METHOD2(OnDragMotion, int(const gfx::PointF& point, int operation));
  MOCK_METHOD0(MockOnDragDrop, void());
  MOCK_METHOD0(OnDragLeave, void());

  void SetOnDropClosure(base::RepeatingClosure closure) {
    on_drop_closure_ = closure;
  }

  ui::OSExchangeData* dropped_data() { return dropped_data_.get(); }

 protected:
  void OnDragDrop(std::unique_ptr<ui::OSExchangeData> data) override {
    dropped_data_ = std::move(data);
    MockOnDragDrop();
    on_drop_closure_.Run();
    on_drop_closure_.Reset();
  }

 private:
  base::RepeatingClosure on_drop_closure_;

  std::unique_ptr<ui::OSExchangeData> dropped_data_;
};

// This class mocks how a real clipboard/ozone client would
// hook to PlatformClipboard, with one difference: real clients
// have no access to the WaylandConnection instance like this
// MockClipboardClient impl does. Instead, clients and ozone gets
// plumbbed up by calling the appropriated Ozone API,
// OzonePlatform::GetPlatformClipboard.
class MockClipboardClient {
 public:
  explicit MockClipboardClient(WaylandConnection* connection) {
    DCHECK(connection);
    // See comment above for reasoning to access the WaylandConnection
    // directly from here.
    delegate_ = connection->clipboard();

    DCHECK(delegate_);
  }
  ~MockClipboardClient() = default;

  // Fill the clipboard backing store with sample data.
  void SetData(const PlatformClipboard::Data& data,
               const std::string& mime_type,
               PlatformClipboard::OfferDataClosure callback) {
    data_types_[mime_type] = data;
    delegate_->OfferClipboardData(ClipboardBuffer::kCopyPaste, data_types_,
                                  std::move(callback));
  }

  void ReadData(const std::string& mime_type,
                PlatformClipboard::RequestDataClosure callback) {
    delegate_->RequestClipboardData(ClipboardBuffer::kCopyPaste, mime_type,
                                    &data_types_, std::move(callback));
  }

  bool IsSelectionOwner() {
    return delegate_->IsSelectionOwner(ClipboardBuffer::kCopyPaste);
  }

 private:
  PlatformClipboard* delegate_ = nullptr;
  PlatformClipboard::DataMap data_types_;

  DISALLOW_COPY_AND_ASSIGN(MockClipboardClient);
};

class WaylandDataDeviceManagerTest : public WaylandTest {
 public:
  WaylandDataDeviceManagerTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    Sync();

    data_device_manager_ = server_.data_device_manager();
    DCHECK(data_device_manager_);

    clipboard_client_ =
        std::make_unique<MockClipboardClient>(connection_.get());

    drop_handler_ = std::make_unique<MockDropHandler>();
    SetWmDropHandler(window_.get(), drop_handler_.get());
  }

 protected:
  wl::TestDataDeviceManager* data_device_manager_;
  std::unique_ptr<MockClipboardClient> clipboard_client_;
  std::unique_ptr<MockDropHandler> drop_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandDataDeviceManagerTest);
};

TEST_P(WaylandDataDeviceManagerTest, WriteToClipboard) {
  // The client writes data to the clipboard ...
  PlatformClipboard::Data data;
  data.assign(wl::kSampleClipboardText,
              wl::kSampleClipboardText + strlen(wl::kSampleClipboardText));
  clipboard_client_->SetData(data, wl::kTextMimeTypeUtf8,
                             base::BindOnce([]() {}));
  Sync();

  // ... and the server reads it.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::RunLoop* loop, PlatformClipboard::Data&& data) {
        std::string string_data(data.begin(), data.end());
        EXPECT_EQ(wl::kSampleClipboardText, string_data);
        loop->Quit();
      },
      &run_loop);
  data_device_manager_->data_source()->ReadData(wl::kTextMimeTypeUtf8,
                                                std::move(callback));
  run_loop.Run();
}

TEST_P(WaylandDataDeviceManagerTest, ReadFromClipboard) {
  // TODO(nickdiego): implement this in terms of an actual wl_surface that
  // gets focused and compositor sends data_device data to it.
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(wl::kTextMimeTypeUtf8,
                      ToClipboardData(std::string(wl::kSampleClipboardText)));
  data_device_manager_->data_device()->OnSelection(data_offer);
  Sync();

  // The client requests to reading clipboard data from the server.
  // The Server writes in some sample data, and we check it matches
  // expectation.
  auto callback =
      base::BindOnce([](const base::Optional<PlatformClipboard::Data>& data) {
        std::string string_data = std::string(data->begin(), data->end());
        EXPECT_EQ(wl::kSampleClipboardText, string_data);
      });
  clipboard_client_->ReadData(wl::kTextMimeTypeUtf8, std::move(callback));
  Sync();
}

TEST_P(WaylandDataDeviceManagerTest, ReadFromClipboardWithoutOffer) {
  // When no data offer is advertised and client requests clipboard data
  // from the server, the response callback should be gracefully called with
  // an empty string.
  auto callback =
      base::BindOnce([](const base::Optional<PlatformClipboard::Data>& data) {
        std::string string_data = std::string(data->begin(), data->end());
        EXPECT_EQ("", string_data);
      });
  clipboard_client_->ReadData(wl::kTextMimeTypeUtf8, std::move(callback));
}

TEST_P(WaylandDataDeviceManagerTest, IsSelectionOwner) {
  auto callback = base::BindOnce([]() {});
  PlatformClipboard::Data data;
  data.assign(wl::kSampleClipboardText,
              wl::kSampleClipboardText + strlen(wl::kSampleClipboardText));
  clipboard_client_->SetData(data, wl::kTextMimeTypeUtf8, std::move(callback));
  Sync();
  ASSERT_TRUE(clipboard_client_->IsSelectionOwner());

  // The compositor sends OnCancelled whenever another application
  // on the system sets a new selection. It means we are not the application
  // that owns the current selection data.
  data_device_manager_->data_source()->OnCancelled();
  Sync();

  ASSERT_FALSE(clipboard_client_->IsSelectionOwner());
}

TEST_P(WaylandDataDeviceManagerTest, StartDrag) {
  bool restored_focus = window_->has_pointer_focus();
  window_->set_pointer_focus(true);

  // The client starts dragging.
  OSExchangeData os_exchange_data;
  int operation = DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
  connection_->StartDrag(os_exchange_data, operation);

  WaylandDataSource::DragDataMap data;
  data[wl::kTextMimeTypeUtf8] = wl::kSampleTextForDragAndDrop;
  connection_->drag_data_source()->SetDragData(data);
  Sync();

  // The server reads the data and the callback gets it.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::RunLoop* loop, PlatformClipboard::Data&& data) {
        std::string result(data.begin(), data.end());
        EXPECT_EQ(wl::kSampleTextForDragAndDrop, result);
        loop->Quit();
      },
      &run_loop);
  data_device_manager_->data_source()->ReadData(wl::kTextMimeTypeUtf8,
                                                std::move(callback));
  run_loop.Run();
  window_->set_pointer_focus(restored_focus);
}

TEST_P(WaylandDataDeviceManagerTest, StartDragWithWrongMimeType) {
  bool restored_focus = window_->has_pointer_focus();
  window_->set_pointer_focus(true);

  // The client starts dragging offering data with wl::kTextMimeTypeUtf8
  // mime type.
  OSExchangeData os_exchange_data;
  int operation = DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
  connection_->StartDrag(os_exchange_data, operation);

  WaylandDataSource::DragDataMap data;
  data[wl::kTextMimeTypeUtf8] = wl::kSampleTextForDragAndDrop;
  connection_->drag_data_source()->SetDragData(data);
  Sync();

  // The server should get an empty data buffer in ReadData callback
  // when trying to read it.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::RunLoop* loop, PlatformClipboard::Data&& data) {
        std::string result(data.begin(), data.end());
        EXPECT_EQ("", result);
        loop->Quit();
      },
      &run_loop);
  data_device_manager_->data_source()->ReadData(ui::kMimeTypeText,
                                                std::move(callback));
  run_loop.Run();
  window_->set_pointer_focus(restored_focus);
}

TEST_P(WaylandDataDeviceManagerTest, ReceiveDrag) {
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(
      ui::kMimeTypeText,
      ToClipboardData(std::string(wl::kSampleTextForDragAndDrop)));

  gfx::Point entered_point(10, 10);
  // The server sends an enter event.
  data_device_manager_->data_device()->OnEnter(
      1002, surface_->resource(), wl_fixed_from_int(entered_point.x()),
      wl_fixed_from_int(entered_point.y()), data_offer);

  int64_t time =
      (ui::EventTimeForNow() - base::TimeTicks()).InMilliseconds() & UINT32_MAX;
  gfx::Point motion_point(11, 11);

  // The server sends an motion event.
  data_device_manager_->data_device()->OnMotion(
      time, wl_fixed_from_int(motion_point.x()),
      wl_fixed_from_int(motion_point.y()));

  Sync();

  auto callback = base::BindOnce([](const PlatformClipboard::Data& contents) {
    std::string result;
    result.assign(reinterpret_cast<std::string::const_pointer>(&contents[0]),
                  contents.size());
    EXPECT_EQ(wl::kSampleTextForDragAndDrop, result);
  });

  // The client requests the data and gets callback with it.
  connection_->RequestDragData(ui::kMimeTypeText, std::move(callback));
  Sync();

  data_device_manager_->data_device()->OnLeave();
}

TEST_P(WaylandDataDeviceManagerTest, DropSeveralMimeTypes) {
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(
      ui::kMimeTypeText,
      ToClipboardData(std::string(wl::kSampleTextForDragAndDrop)));
  data_offer->OnOffer(
      ui::kMimeTypeMozillaURL,
      ToClipboardData(base::UTF8ToUTF16("https://sample.com/\r\n"
                                        "Sample")));
  data_offer->OnOffer(
      ui::kMimeTypeURIList,
      ToClipboardData(std::string("file:///home/user/file\r\n")));

  EXPECT_CALL(*drop_handler_, OnDragEnter(_, _, _)).Times(1);
  gfx::Point entered_point(10, 10);
  data_device_manager_->data_device()->OnEnter(
      1002, surface_->resource(), wl_fixed_from_int(entered_point.x()),
      wl_fixed_from_int(entered_point.y()), data_offer);
  Sync();
  Mock::VerifyAndClearExpectations(drop_handler_.get());

  EXPECT_CALL(*drop_handler_, MockOnDragDrop()).Times(1);
  base::RunLoop loop;
  drop_handler_->SetOnDropClosure(loop.QuitClosure());
  data_device_manager_->data_device()->OnDrop();

  // Here we are expecting three data items, so there will be three roundtrips
  // to the Wayland and back.  Hence Sync() three times.
  Sync();
  Sync();
  Sync();
  loop.Run();
  Mock::VerifyAndClearExpectations(drop_handler_.get());

  EXPECT_TRUE(drop_handler_->dropped_data()->HasString());
  EXPECT_TRUE(drop_handler_->dropped_data()->HasFile());
  EXPECT_TRUE(drop_handler_->dropped_data()->HasURL(kFilenameToURLPolicy));

  data_device_manager_->data_device()->OnLeave();
}

// Tests URI validation for text/uri-list MIME type.  Log warnings rendered in
// the console when this test is running are the expected and valid side effect.
TEST_P(WaylandDataDeviceManagerTest, ValidateDroppedUriList) {
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
    data_offer->OnOffer(ui::kMimeTypeURIList, ToClipboardData(kCase.content));

    EXPECT_CALL(*drop_handler_, OnDragEnter(_, _, _)).Times(1);
    gfx::Point entered_point(10, 10);
    data_device_manager_->data_device()->OnEnter(
        1002, surface_->resource(), wl_fixed_from_int(entered_point.x()),
        wl_fixed_from_int(entered_point.y()), data_offer);
    Sync();
    Mock::VerifyAndClearExpectations(drop_handler_.get());

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
TEST_P(WaylandDataDeviceManagerTest, ValidateDroppedXMozUrl) {
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
    data_offer->OnOffer(ui::kMimeTypeMozillaURL,
                        ToClipboardData(base::UTF8ToUTF16(kCase.content)));

    EXPECT_CALL(*drop_handler_, OnDragEnter(_, _, _)).Times(1);
    gfx::Point entered_point(10, 10);
    data_device_manager_->data_device()->OnEnter(
        1002, surface_->resource(), wl_fixed_from_int(entered_point.x()),
        wl_fixed_from_int(entered_point.y()), data_offer);
    Sync();
    Mock::VerifyAndClearExpectations(drop_handler_.get());

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

INSTANTIATE_TEST_SUITE_P(XdgVersionV5Test,
                         WaylandDataDeviceManagerTest,
                         ::testing::Values(kXdgShellV5));

INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandDataDeviceManagerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
