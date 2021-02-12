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
#include "ui/events/base_event_utils.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/platform_clipboard.h"

using testing::_;
using testing::Mock;

namespace ui {

namespace {

constexpr char kSampleClipboardText[] = "This is a sample text for clipboard.";

template <typename StringType>
ui::PlatformClipboard::Data ToClipboardData(const StringType& data_string) {
  std::vector<uint8_t> data_vector;
  data_vector.assign(data_string.begin(), data_string.end());
  return scoped_refptr<base::RefCountedBytes>(
      base::RefCountedBytes::TakeVector(&data_vector));
}

}  // namespace

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
  void SetData(PlatformClipboard::Data data,
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

class WaylandClipboardTest : public WaylandTest {
 public:
  WaylandClipboardTest() = default;

  void SetUp() override {
    WaylandTest::SetUp();

    Sync();

    data_device_manager_ = server_.data_device_manager();
    DCHECK(data_device_manager_);

    clipboard_client_ =
        std::make_unique<MockClipboardClient>(connection_.get());
  }

 protected:
  wl::TestDataDeviceManager* data_device_manager_;
  std::unique_ptr<MockClipboardClient> clipboard_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandClipboardTest);
};

TEST_P(WaylandClipboardTest, WriteToClipboard) {
  // The client writes data to the clipboard ...
  std::vector<uint8_t> data_vector(
      kSampleClipboardText,
      kSampleClipboardText + strlen(kSampleClipboardText));
  clipboard_client_->SetData(
      scoped_refptr<base::RefCountedBytes>(
          base::RefCountedBytes::TakeVector(&data_vector)),
      {kMimeTypeTextUtf8}, base::BindOnce([]() {}));
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
  data_device_manager_->data_source()->ReadData(kMimeTypeTextUtf8,
                                                std::move(callback));
  run_loop.Run();
}

TEST_P(WaylandClipboardTest, ReadFromClipboard) {
  // TODO(nickdiego): implement this in terms of an actual wl_surface that
  // gets focused and compositor sends data_device data to it.
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(kMimeTypeTextUtf8,
                      ToClipboardData(std::string(kSampleClipboardText)));
  data_device_manager_->data_device()->OnSelection(data_offer);
  Sync();

  // The client requests to reading clipboard data from the server.
  // The Server writes in some sample data, and we check it matches
  // expectation.
  auto callback =
      base::BindOnce([](const base::Optional<PlatformClipboard::Data>& data) {
        auto& bytes = data->get()->data();
        std::string string_data = std::string(bytes.begin(), bytes.end());
        EXPECT_EQ(kSampleClipboardText, string_data);
      });
  clipboard_client_->ReadData(kMimeTypeTextUtf8, std::move(callback));
  Sync();
}

TEST_P(WaylandClipboardTest, ReadFromClipboardWithoutOffer) {
  // When no data offer is advertised and client requests clipboard data
  // from the server, the response callback should be gracefully called with
  // an empty string.
  auto callback =
      base::BindOnce([](const base::Optional<PlatformClipboard::Data>& data) {
        auto& bytes = data->get()->data();
        std::string string_data = std::string(bytes.begin(), bytes.end());
        EXPECT_EQ("", string_data);
      });
  clipboard_client_->ReadData(kMimeTypeTextUtf8, std::move(callback));
}

TEST_P(WaylandClipboardTest, IsSelectionOwner) {
  auto callback = base::BindOnce([]() {});
  std::vector<uint8_t> data_vector(
      kSampleClipboardText,
      kSampleClipboardText + strlen(kSampleClipboardText));
  clipboard_client_->SetData(
      scoped_refptr<base::RefCountedBytes>(
          base::RefCountedBytes::TakeVector(&data_vector)),
      {kMimeTypeTextUtf8}, std::move(callback));
  Sync();
  ASSERT_TRUE(clipboard_client_->IsSelectionOwner());

  // The compositor sends OnCancelled whenever another application
  // on the system sets a new selection. It means we are not the application
  // that owns the current selection data.
  data_device_manager_->data_source()->OnCancelled();
  Sync();

  ASSERT_FALSE(clipboard_client_->IsSelectionOwner());
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandClipboardTest,
                         ::testing::Values(kXdgShellStable));

INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandClipboardTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
