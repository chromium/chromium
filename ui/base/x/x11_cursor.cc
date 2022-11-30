// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor.h"

#include "base/memory/scoped_refptr.h"
#include "ui/base/x/x11_cursor_loader.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

// static
scoped_refptr<X11Cursor> X11Cursor::FromPlatformCursor(
    scoped_refptr<PlatformCursor> platform_cursor) {
  return base::WrapRefCounted(static_cast<X11Cursor*>(platform_cursor.get()));
}

X11Cursor::X11Cursor() = default;

X11Cursor::X11Cursor(x11::Cursor cursor) : loaded_(true), xcursor_(cursor) {}

void X11Cursor::OnCursorLoaded(Callback callback) {
  if (loaded_)
    std::move(callback).Run(xcursor_);
  else
    callbacks_.push_back(std::move(callback));
}

void X11Cursor::SetCursor(x11::Cursor cursor) {
  DCHECK(!loaded_);
  loaded_ = true;
  xcursor_ = cursor;
  for (auto& callback : callbacks_)
    std::move(callback).Run(cursor);
  callbacks_.clear();
}

x11::Cursor X11Cursor::ReleaseCursor() {
  return std::exchange(xcursor_, x11::Cursor::None);
}

X11Cursor::~X11Cursor() {
  if (xcursor_ != x11::Cursor::None)
    x11::Connection::Get()->FreeCursor({xcursor_});
}

}  // namespace ui
