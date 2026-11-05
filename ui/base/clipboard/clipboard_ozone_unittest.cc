// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_ozone.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/strings/string_view_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Not;

class FakePlatformClipboard : public PlatformClipboard {
 public:
  FakePlatformClipboard() = default;
  ~FakePlatformClipboard() override = default;

  void OfferClipboardData(ClipboardBuffer buffer,
                          const DataMap& data_map) override {
    data_map_[buffer] = data_map;
    is_owner_[buffer] = true;
    ++offer_count_[buffer];
    if (callback_) {
      callback_.Run(buffer);
    }
  }

  void RequestClipboardData(ClipboardBuffer buffer,
                            const std::string& mime_type,
                            RequestDataClosure callback) override {
    auto it = data_map_[buffer].find(mime_type);
    if (it != data_map_[buffer].end()) {
      std::move(callback).Run(it->second);
    } else {
      std::move(callback).Run(nullptr);
    }
  }

  void GetAvailableMimeTypes(ClipboardBuffer buffer,
                             GetMimeTypesClosure callback) override {
    std::vector<std::string> mime_types;
    for (const auto& item : data_map_[buffer]) {
      mime_types.push_back(item.first);
    }
    std::move(callback).Run(std::move(mime_types));
  }

  void IsSelectionOwner(ClipboardBuffer buffer,
                        IsSelectionOwnerClosure callback) override {
    std::move(callback).Run(is_owner_[buffer]);
  }

  void SetClipboardDataChangedCallback(
      ClipboardDataChangedCallback callback) override {
    callback_ = std::move(callback);
  }

  bool IsSelectionBufferAvailable() const override { return true; }

  const DataMap& GetDataMap(ClipboardBuffer buffer) const {
    static const base::NoDestructor<DataMap> kEmptyMap;
    auto it = data_map_.find(buffer);
    return it != data_map_.end() ? it->second : *kEmptyMap;
  }

  int GetOfferCount(ClipboardBuffer buffer) const {
    auto it = offer_count_.find(buffer);
    return it != offer_count_.end() ? it->second : 0;
  }

 private:
  base::flat_map<ClipboardBuffer, DataMap> data_map_;
  base::flat_map<ClipboardBuffer, bool> is_owner_;
  base::flat_map<ClipboardBuffer, int> offer_count_;
  ClipboardDataChangedCallback callback_;
};

void ExpectExactSecretHint(const PlatformClipboard::DataMap& data_map) {
  auto it = data_map.find(kMimeTypeLinuxPasswordManagerHint);
  ASSERT_NE(it, data_map.end());
  ASSERT_TRUE(it->second);
  // KDE Klipper and CopyQ compare the raw payload against exact 6-byte ASCII
  // "secret" with no trailing NUL terminator.
  EXPECT_THAT(it->second->as_vector(),
              ElementsAre('s', 'e', 'c', 'r', 'e', 't'));
}

}  // namespace

class ClipboardOzoneTest : public testing::Test {
 protected:
  void SetUp() override {
    auto* clipboard = new ClipboardOzone(&fake_platform_clipboard_);
    clipboard_ozone_ = clipboard;
    Clipboard::SetClipboardForCurrentThread(
        base::WrapUnique<Clipboard>(clipboard));
  }

  void TearDown() override {
    clipboard_ozone_ = nullptr;
    Clipboard::DestroyClipboardForCurrentThread();
  }

  Clipboard* clipboard() { return Clipboard::GetForCurrentThread(); }

  void WriteTextWithPrivacyTypes(std::string text,
                                 Clipboard::PrivacyTypes privacy_types) {
    ClipboardOzone::ObjectMap objects;
    ClipboardOzone::Data data =
        ClipboardOzone::TextData{.data = std::move(text)};
    const size_t index = data.index();
    objects[index] = ClipboardOzone::ObjectMapParams(std::move(data));
    clipboard_ozone_->WritePortableAndPlatformRepresentations(
        ClipboardBuffer::kCopyPaste, objects, /*raw_objects=*/{},
        /*platform_representations=*/{}, /*data_src=*/nullptr, privacy_types);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  FakePlatformClipboard fake_platform_clipboard_;
  raw_ptr<ClipboardOzone> clipboard_ozone_ = nullptr;
};

TEST_F(ClipboardOzoneTest, PublicTextMirrorsToSelectionWithoutPasswordHint) {
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"public text");
  }

  const auto& copy_paste_map =
      fake_platform_clipboard_.GetDataMap(ClipboardBuffer::kCopyPaste);
  ASSERT_TRUE(copy_paste_map.contains(kMimeTypePlainText));
  EXPECT_EQ(base::as_string_view(*copy_paste_map.at(kMimeTypePlainText)),
            "public text");
  EXPECT_FALSE(copy_paste_map.contains(kMimeTypeLinuxPasswordManagerHint));

  EXPECT_EQ(fake_platform_clipboard_.GetOfferCount(ClipboardBuffer::kSelection),
            1);
  const auto& selection_map =
      fake_platform_clipboard_.GetDataMap(ClipboardBuffer::kSelection);
  ASSERT_TRUE(selection_map.contains(kMimeTypePlainText));
  EXPECT_EQ(base::as_string_view(*selection_map.at(kMimeTypePlainText)),
            "public text");
  EXPECT_FALSE(selection_map.contains(kMimeTypeLinuxPasswordManagerHint));
}

TEST_F(ClipboardOzoneTest,
       MarkAsConfidentialWritesSecretHintAndSkipsPrimarySelection) {
  // Seed primary selection first to ensure a confidential copy neither mirrors
  // the password into middle-click paste nor overwrites existing selection.
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"existing primary selection");
  }
  ASSERT_EQ(fake_platform_clipboard_.GetOfferCount(ClipboardBuffer::kSelection),
            1);

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"hunter2");
    writer.MarkAsConfidential();
  }

  const auto& copy_paste_map =
      fake_platform_clipboard_.GetDataMap(ClipboardBuffer::kCopyPaste);
  ASSERT_TRUE(copy_paste_map.contains(kMimeTypePlainText));
  EXPECT_EQ(base::as_string_view(*copy_paste_map.at(kMimeTypePlainText)),
            "hunter2");
  ExpectExactSecretHint(copy_paste_map);

  // Primary selection must not be updated for confidential (`kNoDisplay`)
  // writes.
  EXPECT_EQ(fake_platform_clipboard_.GetOfferCount(ClipboardBuffer::kSelection),
            1);
  EXPECT_EQ(
      clipboard_test_util::ReadText(clipboard(), ClipboardBuffer::kSelection,
                                    /*data_dst=*/nullptr),
      u"existing primary selection");

  // Web-exposed standard types must not leak `x-kde-passwordManagerHint`.
  const std::u16string password_hint_type16 =
      base::UTF8ToUTF16(std::string_view(kMimeTypeLinuxPasswordManagerHint));
  const std::vector<std::u16string> available_types =
      clipboard_test_util::ReadAvailableTypes(clipboard(),
                                              ClipboardBuffer::kCopyPaste,
                                              /*data_dst=*/nullptr);
  EXPECT_THAT(available_types, Contains(kMimeTypePlainText16));
  EXPECT_THAT(available_types, Not(Contains(password_hint_type16)));

  base::test::TestFuture<std::vector<std::u16string>> standard_formats;
  clipboard()->GetStandardFormats(ClipboardBuffer::kCopyPaste,
                                  /*data_dst=*/std::nullopt,
                                  standard_formats.GetCallback());
  EXPECT_THAT(standard_formats.Get(), Contains(kMimeTypePlainText16));
  EXPECT_THAT(standard_formats.Get(), Not(Contains(password_hint_type16)));
}

TEST_F(ClipboardOzoneTest, MarkAsOffTheRecordWritesSecretHintOnBothBuffers) {
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"incognito text");
    writer.MarkAsOffTheRecord();
  }

  const auto& copy_paste_map =
      fake_platform_clipboard_.GetDataMap(ClipboardBuffer::kCopyPaste);
  ASSERT_TRUE(copy_paste_map.contains(kMimeTypePlainText));
  EXPECT_EQ(base::as_string_view(*copy_paste_map.at(kMimeTypePlainText)),
            "incognito text");
  ExpectExactSecretHint(copy_paste_map);

  // Off-the-record text copies keep primary selection mirroring and also attach
  // the password-manager hint on `kSelection`.
  EXPECT_EQ(fake_platform_clipboard_.GetOfferCount(ClipboardBuffer::kSelection),
            1);
  const auto& selection_map =
      fake_platform_clipboard_.GetDataMap(ClipboardBuffer::kSelection);
  ASSERT_TRUE(selection_map.contains(kMimeTypePlainText));
  EXPECT_EQ(base::as_string_view(*selection_map.at(kMimeTypePlainText)),
            "incognito text");
  ExpectExactSecretHint(selection_map);
}

TEST_F(ClipboardOzoneTest, NoCloudClipboardOnlyDoesNotWritePasswordHint) {
  WriteTextWithPrivacyTypes("cloud-excluded text",
                            Clipboard::PrivacyTypes::kNoCloudClipboard);

  const auto& copy_paste_map =
      fake_platform_clipboard_.GetDataMap(ClipboardBuffer::kCopyPaste);
  ASSERT_TRUE(copy_paste_map.contains(kMimeTypePlainText));
  EXPECT_FALSE(copy_paste_map.contains(kMimeTypeLinuxPasswordManagerHint));

  const auto& selection_map =
      fake_platform_clipboard_.GetDataMap(ClipboardBuffer::kSelection);
  ASSERT_TRUE(selection_map.contains(kMimeTypePlainText));
  EXPECT_FALSE(selection_map.contains(kMimeTypeLinuxPasswordManagerHint));
}

}  // namespace ui
