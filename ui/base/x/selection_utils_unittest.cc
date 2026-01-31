// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_utils.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/xproto.h"

namespace ui {
namespace {

TEST(SelectionUtilsTest, SelectionFormatMap_Contains) {
  SelectionFormatMap m;
  EXPECT_FALSE(m.contains(x11::Atom::PRIMARY));
  EXPECT_FALSE(m.contains(x11::Atom::SECONDARY));
  m.Insert(x11::Atom::PRIMARY,
           base::MakeRefCounted<base::RefCountedString>("test"));
  EXPECT_TRUE(m.contains(x11::Atom::PRIMARY));
  EXPECT_FALSE(m.contains(x11::Atom::SECONDARY));
}

}  // namespace
}  // namespace ui
