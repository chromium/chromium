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
#include "base/strings/utf_string_conversions.h"
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

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandClipboardTest);
};

TEST_P(WaylandClipboardTest, WriteToClipboard) {
  // The client writes data to the clipboard ...
  OfferData(GetBuffer(), kSampleClipboardText, {kMimeTypeTextUtf8});
  Sync();

  // ... and the server reads it.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::RunLoop* loop, std::vector<uint8_t>&& data) {
        std::string string_data(data.begin(), data.end());
        EXPECT_EQ(kSampleClipboardText, string_data);
        loop->Quit();
      },
      &run_loop);

  GetServerSelectionSource()->ReadData(kMimeTypeTextUtf8, std::move(callback));
  run_loop.Run();
}

TEST_P(WaylandClipboardTest, ReadFromClipboard) {
  auto* device = GetServerSelectionDevice();
  auto* data_offer = device->OnDataOffer();
  data_offer->OnOffer(kMimeTypeTextUtf8,
                      ToClipboardData(std::string(kSampleClipboardText)));
  device->OnSelection(data_offer);
  Sync();

  // The client requests to read the clipboard data from the server. The Server
  // writes in some sample data, and we check it matches expectation.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::OnceClosure quit_closure, const PlatformClipboard::Data& data) {
        ASSERT_TRUE(data.get());
        std::string string_data(data->front_as<const char>(), data->size());
        EXPECT_EQ(kSampleClipboardText, string_data);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure());

  clipboard_->RequestClipboardData(GetBuffer(), kMimeTypeTextUtf8,
                                   std::move(callback));
  Sync();
  run_loop.Run();
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

  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::OnceClosure quit_closure, const PlatformClipboard::Data& data) {
        std::string str(data->front_as<const char>(), data->size());
        EXPECT_EQ("utf8_text", str);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure());

  clipboard_->RequestClipboardData(GetBuffer(), kMimeTypeTextUtf8,
                                   std::move(callback));
  Sync();
  run_loop.Run();
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
  // Offer a piece of text in kCopyPaste clipboard buffer.
  auto* data_offer = GetServerSelectionDevice()->OnDataOffer();
  data_offer->OnOffer(kMimeTypeTextUtf8,
                      ToClipboardData(std::string(kSampleClipboardText)));
  GetServerSelectionDevice()->OnSelection(data_offer);
  Sync();

  // Post a read request for the other buffer, which will start its execution
  // after the request above starts.
  auto other_buffer = GetBuffer() == ClipboardBuffer::kSelection
                          ? ClipboardBuffer::kCopyPaste
                          : ClipboardBuffer::kSelection;
  base::MockCallback<PlatformClipboard::RequestDataClosure> callback;
  EXPECT_CALL(callback, Run(_)).Times(1);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PlatformClipboard::RequestClipboardData,
                                base::Unretained(clipboard_), other_buffer,
                                kMimeTypeTextUtf8, callback.Get()));

  // Instantly start a clipboard read request for kCopyPaste buffer (the actual
  // data transfer will take place asynchronously. See WaylandDataDevice impl)
  // and ensure read callback is called with the expected resulting data,
  // regardless any other request that may arrive in the meantime.
  base::RunLoop run_loop;
  clipboard_->RequestClipboardData(
      GetBuffer(), kMimeTypeTextUtf8,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             const PlatformClipboard::Data& data) {
            ASSERT_TRUE(data.get());
            EXPECT_EQ(kSampleClipboardText,
                      std::string(data->front_as<const char>(), data->size()));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  Sync();
  run_loop.Run();
  Sync();
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

// TODO(crbug.com/1204670): Add test cases specific for CopyPaste-only
// clipboards, i.e.: No primary selection protocol available.

}  // namespace ui
