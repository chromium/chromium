// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_CURSOR_SHAPE_CLIENT_H_
#define UI_AURA_CLIENT_CURSOR_SHAPE_CLIENT_H_

#include <optional>

#include "ui/aura/aura_export.h"

namespace ui {
class Cursor;
struct CursorData;
}  // namespace ui

namespace aura::client {

// An interface to query information related to a cursor.
class AURA_EXPORT CursorShapeClient {
 public:
  virtual ~CursorShapeClient();

  virtual std::optional<ui::CursorData> GetCursorData(
      const ui::Cursor& cursor) const = 0;
};

AURA_EXPORT void SetCursorShapeClient(CursorShapeClient* client);
AURA_EXPORT const CursorShapeClient& GetCursorShapeClient();

}  // namespace aura::client

#endif  // UI_AURA_CLIENT_CURSOR_SHAPE_CLIENT_H_
