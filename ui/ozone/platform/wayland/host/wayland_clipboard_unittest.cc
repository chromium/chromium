// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
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
  return base::RefCountedBytes::TakeVector(&data_vector);
}

}  // namespace

class WaylandClipboardTest : public WaylandTest {
 public:
  WaylandClipboardTest() = default;

  void SetUp() override {
    WaylandTest::SetUp();

    Sync();

    data_device_manager_ = server_.data_device_manager();
    ASSERT_TRUE(data_device_manager_);

    clipboard_ = connection_->clipboard();
    ASSERT_TRUE(clipboard_);

    offered_data_.clear();
  }

 protected:
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

  wl::TestDataDeviceManager* data_device_manager_;
  PlatformClipboard* clipboard_;
  PlatformClipboard::DataMap offered_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandClipboardTest);
};

TEST_P(WaylandClipboardTest, WriteToClipboard) {
  // The client writes data to the clipboard ...
  OfferData(ClipboardBuffer::kCopyPaste, kSampleClipboardText,
            {kMimeTypeTextUtf8});
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
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(kMimeTypeTextUtf8,
                      ToClipboardData(std::string(kSampleClipboardText)));
  data_device_manager_->data_device()->OnSelection(data_offer);
  Sync();

  // The client requests to read the clipboard data from the server. The Server
  // writes in some sample data, and we check it matches expectation.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::OnceClosure quit_closure,
         const base::Optional<PlatformClipboard::Data>& data) {
        auto& bytes = data->get()->data();
        std::string string_data = std::string(bytes.begin(), bytes.end());
        EXPECT_EQ(kSampleClipboardText, string_data);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure());

  PlatformClipboard::DataMap read_data_;
  clipboard_->RequestClipboardData(ClipboardBuffer::kCopyPaste,
                                   kMimeTypeTextUtf8, &read_data_,
                                   std::move(callback));
  Sync();
  run_loop.Run();
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
  PlatformClipboard::DataMap read_data_;
  clipboard_->RequestClipboardData(ClipboardBuffer::kCopyPaste,
                                   kMimeTypeTextUtf8, &read_data_,
                                   std::move(callback));
}

TEST_P(WaylandClipboardTest, IsSelectionOwner) {
  OfferData(ClipboardBuffer::kCopyPaste, kSampleClipboardText,
            {kMimeTypeTextUtf8});
  Sync();
  ASSERT_TRUE(clipboard_->IsSelectionOwner(ClipboardBuffer::kCopyPaste));

  // The compositor sends OnCancelled whenever another application
  // on the system sets a new selection. It means we are not the application
  // that owns the current selection data.
  data_device_manager_->data_source()->OnCancelled();
  Sync();

  ASSERT_FALSE(clipboard_->IsSelectionOwner(ClipboardBuffer::kCopyPaste));
}

// Ensures WaylandClipboard correctly handles overlapping read requests for
// different clipboard buffers.
TEST_P(WaylandClipboardTest, OverlapReadingFromDifferentBuffers) {
  // Offer a piece of text in kCopyPaste clipboard buffer.
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(kMimeTypeTextUtf8,
                      ToClipboardData(std::string(kSampleClipboardText)));
  data_device_manager_->data_device()->OnSelection(data_offer);
  Sync();

  // Post a read request for kSelection buffer, which will start its execution
  // after kCopyPaste request (below) starts.
  PlatformClipboard::DataMap selection_data_;
  base::MockCallback<PlatformClipboard::RequestDataClosure> selection_callback;
  EXPECT_CALL(selection_callback, Run(_)).Times(1);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformClipboard::RequestClipboardData,
                     base::Unretained(clipboard_), ClipboardBuffer::kSelection,
                     kMimeTypeTextUtf8, base::Unretained(&selection_data_),
                     selection_callback.Get()));

  // Instantly start a clipboard read request for kCopyPaste buffer (the actual
  // data transfer will take place asynchronously. See WaylandDataDevice impl)
  // and ensure read callback is called and |copy_paste_data_| is filled as
  // expected, regardless any other request that may arrive in the meantime.
  PlatformClipboard::DataMap copy_paste_data_;
  base::RunLoop run_loop;
  clipboard_->RequestClipboardData(
      ClipboardBuffer::kCopyPaste, kMimeTypeTextUtf8, &copy_paste_data_,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             const base::Optional<PlatformClipboard::Data>& data) {
            ASSERT_TRUE(data.has_value());
            EXPECT_EQ(kSampleClipboardText,
                      std::string(data->get()->front_as<const char>(),
                                  data->get()->size()));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  Sync();
  run_loop.Run();

  EXPECT_TRUE(selection_data_.empty());

  EXPECT_EQ(1u, copy_paste_data_.size());
  EXPECT_TRUE(base::Contains(copy_paste_data_, kMimeTypeTextUtf8));
  auto contents = copy_paste_data_[kMimeTypeTextUtf8];
  EXPECT_EQ(kSampleClipboardText,
            std::string(contents->front_as<const char>(), contents->size()));
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandClipboardTest,
                         ::testing::Values(kXdgShellStable));

INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandClipboardTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
