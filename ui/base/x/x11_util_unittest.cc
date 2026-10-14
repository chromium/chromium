// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_util.h"

#include <cstdint>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

TEST(X11UtilTest, GetNativeWindowIconPicksLargestIconAndCopiesPixels) {
  x11::Connection* connection = x11::Connection::Get();
  x11::Window window = connection->CreateDummyWindow("X11UtilTest");

  // A 1x1 icon followed by a 2x2 icon whose pixel data ends exactly at the
  // end of the property.
  const std::vector<uint32_t> data = {
      1u, 1u, 0xff000001u,                                          // 1x1
      2u, 2u, 0xff0000aau, 0xff00bb00u, 0xffcc0000u, 0x80112233u};  // 2x2
  connection->SetArrayProperty(window, x11::GetAtom("_NET_WM_ICON"),
                               x11::Atom::CARDINAL, data);
  connection->Sync();

  gfx::ImageSkia icon = GetNativeWindowIcon(static_cast<intptr_t>(window));
  ASSERT_FALSE(icon.isNull());
  const SkBitmap& bitmap = icon.GetRepresentation(1.0f).GetBitmap();
  ASSERT_EQ(2, bitmap.width());
  ASSERT_EQ(2, bitmap.height());
  EXPECT_EQ(0xff0000aau, *bitmap.getAddr32(0, 0));
  EXPECT_EQ(0xff00bb00u, *bitmap.getAddr32(1, 0));
  EXPECT_EQ(0xffcc0000u, *bitmap.getAddr32(0, 1));
  EXPECT_EQ(0x80112233u, *bitmap.getAddr32(1, 1));

  connection->DestroyWindow({window});
  connection->Sync();
}

TEST(X11UtilTest, SetWindowClassHintWritesNulTerminatedNameAndClass) {
  x11::Connection* connection = x11::Connection::Get();
  x11::Window window = connection->CreateDummyWindow("X11UtilTest");
  std::vector<char> data;

  SetWindowClassHint(connection, window, "name", "Class");
  ASSERT_TRUE(connection->GetArrayProperty(window, x11::Atom::WM_CLASS, &data));
  EXPECT_EQ(data, (std::vector<char>{'n', 'a', 'm', 'e', '\0', 'C', 'l', 'a',
                                     's', 's', '\0'}));

  SetWindowClassHint(connection, window, "", "");
  ASSERT_TRUE(connection->GetArrayProperty(window, x11::Atom::WM_CLASS, &data));
  EXPECT_EQ(data, (std::vector<char>{'\0', '\0'}));

  connection->DestroyWindow({window});
  connection->Sync();
}

}  // namespace ui
