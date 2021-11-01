// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"
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

}  // namespace

class WaylandClipboardTest : public WaylandTest {
 public:
  WaylandClipboardTest() = default;

  WaylandClipboardTest(const WaylandClipboardTest&) = delete;
  WaylandClipboardTest& operator=(const WaylandClipboardTest&) = delete;

  ~WaylandClipboardTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    ASSERT_TRUE(server_.data_device_manager());
    ASSERT_TRUE(GetParam().primary_selection_protocol ==
                    wl::PrimarySelectionProtocol::kNone ||
                server_.primary_selection_device_manager());

    clipboard_ = connection_->clipboard();
    ASSERT_TRUE(clipboard_);

    // Make sure clipboard instance for the available primary selection protocol
    // gets properly created, ie: the corresponding 'get_device' request is
    // issued, so server-side objects are created prior to test-case specific
    // calls, otherwise tests, such as ReadFromClipboard, would crash.
    ASSERT_EQ(GetBuffer() == ClipboardBuffer::kSelection,
              !!clipboard_->GetClipboard(ClipboardBuffer::kSelection));
    Sync();

    offered_data_.clear();
  }

 protected:
  wl::TestSelectionDevice* GetServerSelectionDevice() {
    return GetBuffer() == ClipboardBuffer::kSelection
               ? server_.primary_selection_device_manager()->device()
               : server_.data_device_manager()->data_device();
  }

  wl::TestSelectionSource* GetServerSelectionSource() {
    return GetBuffer() == ClipboardBuffer::kSelection
               ? server_.primary_selection_device_manager()->source()
               : server_.data_device_manager()->data_source();
  }

  ClipboardBuffer GetBuffer() const {
    return GetParam().primary_selection_protocol !=
                   wl::PrimarySelectionProtocol::kNone
               ? ClipboardBuffer::kSelection
               : ClipboardBuffer::kCopyPaste;
  }

  // Fill the clipboard backing store with sample data.
  void OfferData(ClipboardBuffer buffer,
                 const char* data,
                 const std::string& mime_type) {
    std::vector<uint8_t> data_vector(data, data + std::strlen(data));
    offered_data_[mime_type] = base::RefCountedBytes::TakeVector(&data_vector);

    base::MockCallback<PlatformClipboard::OfferDataClosure> offer_callback;
    EXPECT_CALL(offer_callback, Run()).Times(1);
    clipboard_->OfferClipboardData(buffer, offered_data_, offer_callback.Get());
  }

  WaylandClipboard* clipboard_ = nullptr;
  PlatformClipboard::DataMap offered_data_;
};

class CopyPasteOnlyClipboardTest : public WaylandTest {
 public:
  void SetUp() override {
    WaylandTest::SetUp();

    clipboard_ = connection_->clipboard();
    ASSERT_FALSE(clipboard_->IsSelectionBufferAvailable());

    ASSERT_EQ(wl::PrimarySelectionProtocol::kNone,
              GetParam().primary_selection_protocol);
    ASSERT_TRUE(server_.data_device_manager());
    ASSERT_FALSE(server_.primary_selection_device_manager());
  }

  void TearDown() override {
    WaylandTest::TearDown();
    clipboard_ = nullptr;
  }

 protected:
  WaylandClipboard* clipboard_ = nullptr;
};

TEST_P(WaylandClipboardTest, WriteToClipboard) {
  // 1. Offer sample text as selection data.
  OfferData(GetBuffer(), kSampleClipboardText, {kMimeTypeTextUtf8});
  Sync();

  // 2. Emulate an external client requesting to read the offered data and make
  // sure the appropriate string gets delivered.
  std::string delivered_text;
  base::MockCallback<wl::TestSelectionSource::ReadDataCallback> callback;
  EXPECT_CALL(callback, Run(_)).WillOnce([&](std::vector<uint8_t>&& data) {
    delivered_text = std::string(data.begin(), data.end());
  });
  GetServerSelectionSource()->ReadData(kMimeTypeTextUtf8, callback.Get());
  Sync();

  // 3. Ensure the requests/events are flushed and posted tasks get processed.
  // wl::TestSelection{Source,Offer} currently use ThreadPool task runners.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(kSampleClipboardText, delivered_text);
}

TEST_P(WaylandClipboardTest, ReadFromClipboard) {
  // 1. Emulate a selection data offer coming in.
  auto* device = GetServerSelectionDevice();
  auto* data_offer = device->OnDataOffer();
  data_offer->OnOffer(kMimeTypeTextUtf8,
                      ToClipboardData(std::string(kSampleClipboardText)));
  device->OnSelection(data_offer);
  Sync();

  // 2. Request to read the offered data and check whether the read text matches
  // the previously offered one.
  std::string text;
  base::MockCallback<PlatformClipboard::RequestDataClosure> callback;
  EXPECT_CALL(callback, Run(_)).WillOnce([&](PlatformClipboard::Data data) {
    ASSERT_TRUE(data);
    text = std::string(data->front_as<const char>(), data->size());
  });

  clipboard_->RequestClipboardData(GetBuffer(), kMimeTypeTextUtf8,
                                   callback.Get());
  Sync();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kSampleClipboardText, text);
}

// Regression test for crbug.com/1183939. Ensures unicode mime types take
// priority over text/plain when reading text.
TEST_P(WaylandClipboardTest, ReadFromClipboardPrioritizeUtf) {
  auto* data_offer = GetServerSelectionDevice()->OnDataOffer();
  data_offer->OnOffer(kMimeTypeText,
                      ToClipboardData(std::string("ascii_text")));
  data_offer->OnOffer(kMimeTypeTextUtf8,
                      ToClipboardData(std::string("utf8_text")));
  GetServerSelectionDevice()->OnSelection(data_offer);
  Sync();

  std::string text;
  base::MockCallback<PlatformClipboard::RequestDataClosure> callback;
  EXPECT_CALL(callback, Run(_)).WillOnce([&](PlatformClipboard::Data data) {
    ASSERT_TRUE(data);
    text = std::string(data->front_as<const char>(), data->size());
  });

  clipboard_->RequestClipboardData(GetBuffer(), kMimeTypeTextUtf8,
                                   callback.Get());
  Sync();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("utf8_text", text);
}

TEST_P(WaylandClipboardTest, ReadFromClipboardWithoutOffer) {
  // When no data offer is advertised and client requests clipboard data from
  // the server, the response callback should be gracefully called with null
  // data.
  auto callback = base::BindOnce(
      [](const PlatformClipboard::Data& data) { ASSERT_FALSE(data); });
  clipboard_->RequestClipboardData(GetBuffer(), kMimeTypeTextUtf8,
                                   std::move(callback));
}

TEST_P(WaylandClipboardTest, IsSelectionOwner) {
  OfferData(GetBuffer(), kSampleClipboardText, {kMimeTypeTextUtf8});
  Sync();
  ASSERT_TRUE(clipboard_->IsSelectionOwner(GetBuffer()));

  // The compositor sends OnCancelled whenever another application on the system
  // sets a new selection. It means we are not the application that owns the
  // current selection data.
  GetServerSelectionSource()->OnCancelled();
  Sync();

  ASSERT_FALSE(clipboard_->IsSelectionOwner(GetBuffer()));
}

// Ensures WaylandClipboard correctly handles overlapping read requests for
// different clipboard buffers.
TEST_P(WaylandClipboardTest, OverlapReadingFromDifferentBuffers) {
  // Offer a sample text as selection data.
  auto* data_offer = GetServerSelectionDevice()->OnDataOffer();
  data_offer->OnOffer(kMimeTypeTextUtf8,
                      ToClipboardData(std::string(kSampleClipboardText)));
  GetServerSelectionDevice()->OnSelection(data_offer);
  Sync();

  // Post a read request for the other buffer, which will start its execution
  // after the request above.
  auto other_buffer = GetBuffer() == ClipboardBuffer::kSelection
                          ? ClipboardBuffer::kCopyPaste
                          : ClipboardBuffer::kSelection;
  base::MockCallback<PlatformClipboard::RequestDataClosure> callback;
  EXPECT_CALL(callback, Run(Eq(nullptr))).Times(1);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
    text = std::string(data->front_as<const char>(), data->size());
  });
  clipboard_->RequestClipboardData(GetBuffer(), kMimeTypeTextUtf8,
                                   got_text.Get());

  Sync();
  base::RunLoop().RunUntilIdle();
  Sync();

  EXPECT_EQ(kSampleClipboardText, text);
}

// Ensures clipboard change callback is fired only once per read/write.
TEST_P(WaylandClipboardTest, ClipboardChangeNotifications) {
  base::MockCallback<PlatformClipboard::ClipboardDataChangedCallback>
      clipboard_changed_callback;
  clipboard_->SetClipboardDataChangedCallback(clipboard_changed_callback.Get());
  const auto buffer = GetBuffer();

  // 1. For selection offered by an external application.
  EXPECT_CALL(clipboard_changed_callback, Run(buffer)).Times(1);
  auto* data_offer = GetServerSelectionDevice()->OnDataOffer();
  data_offer->OnOffer(kMimeTypeTextUtf8,
                      ToClipboardData(std::string(kSampleClipboardText)));
  GetServerSelectionDevice()->OnSelection(data_offer);
  Sync();
  EXPECT_FALSE(clipboard_->IsSelectionOwner(buffer));

  // 2. For selection offered by Chromium.
  EXPECT_CALL(clipboard_changed_callback, Run(buffer)).Times(1);
  OfferData(buffer, kSampleClipboardText, {kMimeTypeTextUtf8});
  Sync();
  EXPECT_TRUE(clipboard_->IsSelectionOwner(buffer));
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
TEST_P(CopyPasteOnlyClipboardTest, OverlappingReadRequests) {
  // Create an selection data offer containing plain and html mime types.
  auto* data_device = server_.data_device_manager()->data_device();
  auto* data_offer = data_device->OnDataOffer();
  data_offer->OnOffer(kMimeTypeText, ToClipboardData(std::string("text")));
  data_offer->OnOffer(kMimeTypeHTML, ToClipboardData(std::string("html")));
  data_device->OnSelection(data_offer);
  Sync();

  // Schedule a clipboard read task (for text/html mime type). As read requests
  // are processed asynchronously, this will actually start when the request
  // below is already under processing, thus emulating 2 "overlapping" requests.
  std::string html;
  base::MockCallback<PlatformClipboard::RequestDataClosure> got_html;
  EXPECT_CALL(got_html, Run(_)).WillOnce([&](PlatformClipboard::Data data) {
    html = std::string(data->front_as<const char>(), data->size());
  });
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformClipboard::RequestClipboardData,
                     base::Unretained(clipboard_), ClipboardBuffer::kCopyPaste,
                     kMimeTypeHTML, got_html.Get()));

  // Instantly start a read request for text/plain mime type.
  std::string text;
  base::MockCallback<PlatformClipboard::RequestDataClosure> got_text;
  EXPECT_CALL(got_text, Run(_)).WillOnce([&](PlatformClipboard::Data data) {
    text = std::string(data->front_as<const char>(), data->size());
  });
  clipboard_->RequestClipboardData(ClipboardBuffer::kCopyPaste, kMimeTypeText,
                                   got_text.Get());

  Sync();
  base::RunLoop().RunUntilIdle();
  Sync();

  // Ensures both requests were processed correctly.
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
