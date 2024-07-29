// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/wayland/host/wayland_exchange_data_provider.h"

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/pickle.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace ui {

namespace {

template <typename StringType>
PlatformClipboard::Data ToClipboardData(const StringType& data_string) {
  return base::MakeRefCounted<base::RefCountedBytes>(
      base::as_byte_span(data_string));
}
}  // namespace

// Regression test for https://crbug.com/1284996.
TEST(WaylandExchangeDataProviderTest, ExtractPickledData) {
  WaylandExchangeDataProvider provider;
  std::string extracted;

  EXPECT_FALSE(provider.ExtractData(kMimeTypeText, &extracted));
  EXPECT_FALSE(
      provider.ExtractData(kMimeTypeDataTransferCustomData, &extracted));

  extracted.clear();
  provider.SetString(u"dnd-string");
  EXPECT_TRUE(provider.ExtractData(kMimeTypeText, &extracted));
  EXPECT_EQ("dnd-string", extracted);

  extracted.clear();
  base::Pickle pickle;
  pickle.WriteString("pickled-str");
  provider.SetPickledData(ClipboardFormatType::DataTransferCustomType(),
                          pickle);
  EXPECT_TRUE(
      provider.ExtractData(kMimeTypeDataTransferCustomData, &extracted));

  // Ensure Pickle "reconstruction" works as expected.
  std::string read_pickled_str;
  base::Pickle read_pickle =
      base::Pickle::WithData(base::as_byte_span(extracted));
  base::PickleIterator iter(read_pickle);
  ASSERT_TRUE(read_pickle.data());
  EXPECT_FALSE(iter.ReachedEnd());
  EXPECT_TRUE(iter.ReadString(&read_pickled_str));
  EXPECT_EQ("pickled-str", read_pickled_str);
}

TEST(WaylandExchangeDataProviderTest, FileContents) {
  constexpr std::string kName("filename");
  constexpr std::string kContents("contents");
  const std::string kMimeType("application/octet-stream;name=\"filename\"");

  WaylandExchangeDataProvider provider;
  provider.AddData(ToClipboardData(kContents), kMimeType);

  std::vector<std::string> mime_types = provider.BuildMimeTypesList();
  EXPECT_THAT(mime_types, ::testing::Contains(kMimeType));

  std::optional<OSExchangeDataProvider::FileContentsInfo> file_contents =
      provider.GetFileContents();
  EXPECT_TRUE(file_contents.has_value());
  EXPECT_EQ(kName, file_contents->filename.value());
  EXPECT_EQ(kContents, file_contents->file_contents);

  std::string extracted;
  EXPECT_TRUE(provider.ExtractData(kMimeType, &extracted));
  EXPECT_EQ(kContents, extracted);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST(WaylandExchangeDataProviderTest, AddAndExtractDataTransferEndpoint) {
  std::string kExpectedEncodedDte =
      "{\"endpoint_type\":\"url\","
      "\"off_the_record\":false,"
      "\"url\":\"https://www.google.com/\"}";
  const DataTransferEndpoint expected_dte =
      ui::DataTransferEndpoint(GURL("https://www.google.com"));

  WaylandExchangeDataProvider provider;
  std::string extracted;

  EXPECT_FALSE(provider.ExtractData(kMimeTypeDataTransferEndpoint, &extracted));

  extracted.clear();

  provider.AddData(ToClipboardData(kExpectedEncodedDte),
                   kMimeTypeDataTransferEndpoint);
  DataTransferEndpoint* actual_dte = provider.GetSource();
  EXPECT_TRUE(expected_dte.IsSameURLWith(*actual_dte));

  std::vector<std::string> mime_types = provider.BuildMimeTypesList();
  EXPECT_THAT(mime_types, ::testing::Contains(kMimeTypeDataTransferEndpoint));

  EXPECT_TRUE(provider.ExtractData(kMimeTypeDataTransferEndpoint, &extracted));
  EXPECT_EQ(kExpectedEncodedDte, extracted);
}

TEST(WaylandExchangeDataProviderTest, AddAndExtractMultipleData) {
  // Simulate WebUI tab dragging scenario.

  std::string kExpectedEncodedDte =
      "{\"endpoint_type\":\"url\","
      "\"off_the_record\":false,"
      "\"url\":\"chrome://tab-strip.top-chrome\"}";
  const DataTransferEndpoint expected_dte =
      ui::DataTransferEndpoint(GURL("chrome://tab-strip.top-chrome"));

  WaylandExchangeDataProvider provider;
  std::string extracted;

  EXPECT_FALSE(provider.ExtractData(kMimeTypeDataTransferEndpoint, &extracted));
  extracted.clear();
  EXPECT_FALSE(
      provider.ExtractData(kMimeTypeDataTransferCustomData, &extracted));
  extracted.clear();

  // Add DataTransferEndpoint.
  provider.AddData(ToClipboardData(kExpectedEncodedDte),
                   kMimeTypeDataTransferEndpoint);

  // Add pickled data.
  base::Pickle pickle;
  pickle.WriteString("pickled-str");
  provider.SetPickledData(ClipboardFormatType::DataTransferCustomType(),
                          pickle);

  DataTransferEndpoint* actual_dte = provider.GetSource();
  EXPECT_TRUE(expected_dte.IsSameURLWith(*actual_dte));

  std::vector<std::string> mime_types = provider.BuildMimeTypesList();
  EXPECT_THAT(mime_types, ::testing::Contains(kMimeTypeDataTransferEndpoint));
  EXPECT_THAT(mime_types, ::testing::Contains(kMimeTypeDataTransferCustomData));

  EXPECT_TRUE(provider.ExtractData(kMimeTypeDataTransferEndpoint, &extracted));
  EXPECT_EQ(kExpectedEncodedDte, extracted);

  extracted.clear();
  EXPECT_TRUE(
      provider.ExtractData(kMimeTypeDataTransferCustomData, &extracted));
  base::Pickle read_pickle =
      base::Pickle::WithData(base::as_byte_span(extracted));
  base::PickleIterator pickle_iter(read_pickle);
  ASSERT_TRUE(read_pickle.data());
  EXPECT_FALSE(pickle_iter.ReachedEnd());

  std::string read_pickled_str;
  EXPECT_TRUE(pickle_iter.ReadString(&read_pickled_str));
  EXPECT_EQ("pickled-str", read_pickled_str);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace ui
