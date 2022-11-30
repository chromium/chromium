// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/touch_input.h"
#include "base/win/win_util.h"

namespace ui {

BOOL GetTouchInputInfoWrapper(HTOUCHINPUT handle,
                              UINT count,
                              PTOUCHINPUT pointer,
                              int size) {
  static const auto get_touch_input_info_func =
      reinterpret_cast<decltype(&::GetTouchInputInfo)>(
          base::win::GetUser32FunctionPointer("GetTouchInputInfo"));
  if (get_touch_input_info_func)
    return get_touch_input_info_func(handle, count, pointer, size);
  return FALSE;
}

}  // namespace ui
