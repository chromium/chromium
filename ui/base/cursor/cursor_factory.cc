// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_factory.h"

#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"

namespace ui {

namespace {

CursorFactory* g_instance = nullptr;

}  // namespace

CursorFactory::CursorFactory() {
  DCHECK(!g_instance) << "There should only be a single CursorFactory.";
  g_instance = this;
}

CursorFactory::~CursorFactory() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

CursorFactory* CursorFactory::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

base::Optional<PlatformCursor> CursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

PlatformCursor CursorFactory::CreateImageCursor(mojom::CursorType type,
                                                const SkBitmap& bitmap,
                                                const gfx::Point& hotspot) {
  NOTIMPLEMENTED();
  return 0;
}

PlatformCursor CursorFactory::CreateAnimatedCursor(
    mojom::CursorType type,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    int frame_delay_ms) {
  NOTIMPLEMENTED();
  return 0;
}

void CursorFactory::RefImageCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void CursorFactory::UnrefImageCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void CursorFactory::ObserveThemeChanges() {
  NOTIMPLEMENTED();
}

}  // namespace ui
