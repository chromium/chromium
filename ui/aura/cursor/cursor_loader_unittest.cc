// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/cursor/cursor_loader.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"

namespace aura {

namespace {

using CursorLoaderTest = ::aura::test::AuraTestBase;
using ::ui::mojom::CursorType;

}  // namespace

TEST_F(CursorLoaderTest, InvisibleCursor) {
  CursorLoader cursor_loader;
  ui::Cursor invisible_cursor(CursorType::kNone);
  cursor_loader.SetPlatformCursor(&invisible_cursor);

  ASSERT_EQ(
      invisible_cursor.platform(),
      ui::CursorFactory::GetInstance()->GetDefaultCursor(CursorType::kNone));
}

}  // namespace aura
