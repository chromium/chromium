// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_utils.h"

#include <stddef.h>

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/xproto.h"

namespace ui {
namespace {

TEST(SelectionUtilsTest, RefCountedMemoryToString16Empty) {
  const auto memory = base::MakeRefCounted<base::RefCountedBytes>();
  EXPECT_TRUE(RefCountedMemoryToString16(memory).empty());
}

TEST(SelectionUtilsTest, RefCountedMemoryToString16PreservesCodeUnits) {
  const std::u16string text = {u'A', u'\0', u'\u2603', u'\xD83D', u'\xDE00'};
  const auto memory =
      base::MakeRefCounted<base::RefCountedBytes>(base::as_byte_span(text));
  EXPECT_EQ(text, RefCountedMemoryToString16(memory));
}

TEST(SelectionUtilsTest, RefCountedMemoryToString16RejectsOddByteCount) {
  for (size_t byte_count : {1u, 3u, 67u}) {
    SCOPED_TRACE(byte_count);
    const auto memory = base::MakeRefCounted<base::RefCountedString>(
        std::string(byte_count, 'x'));
    EXPECT_TRUE(RefCountedMemoryToString16(memory).empty());
  }
}

TEST(SelectionUtilsTest, SelectionFormatMap_Contains) {
  SelectionFormatMap m;
  EXPECT_FALSE(m.contains(x11::Atom::PRIMARY));
  EXPECT_FALSE(m.contains(x11::Atom::SECONDARY));
  m.Insert(x11::Atom::PRIMARY,
           base::MakeRefCounted<base::RefCountedString>("test"));
  EXPECT_TRUE(m.contains(x11::Atom::PRIMARY));
  EXPECT_FALSE(m.contains(x11::Atom::SECONDARY));
}

TEST(SelectionUtilsTest, SelectionData_TakeBytesReleasesUnderlyingMemory) {
  SelectionData empty;
  EXPECT_EQ(nullptr, empty.TakeBytes());

  SelectionData data(x11::Atom::STRING,
                     base::MakeRefCounted<base::RefCountedString>("secret"));
  scoped_refptr<base::RefCountedBytes> taken = data.TakeBytes();
  ASSERT_TRUE(taken);
  EXPECT_EQ(base::as_byte_span(std::string_view("secret")), base::span(*taken));
  EXPECT_TRUE(data.GetSpan().empty());
  EXPECT_EQ(nullptr, data.TakeBytes());
}

}  // namespace
}  // namespace ui
