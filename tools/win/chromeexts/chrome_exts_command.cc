// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/win/chromeexts/chrome_exts_command.h"

#include "base/check.h"

namespace tools {
namespace win {
namespace chromeexts {

ChromeExtsCommand::~ChromeExtsCommand() = default;

ChromeExtsCommand::ChromeExtsCommand() = default;

HRESULT ChromeExtsCommand::Initialize(IDebugClient* debug_client,
                                      const char* args) {
  DCHECK(debug_client);
  DCHECK(args);
  args_ = args;
  debug_client_ = debug_client;
  debug_control_ = GetDebugClientAs<IDebugControl>();
  if (!debug_control_) {
    return E_FAIL;
  }
  return S_OK;
}

HRESULT ChromeExtsCommand::Printf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  HRESULT hr = PrintV(format, ap);
  va_end(ap);
  return hr;
}

HRESULT ChromeExtsCommand::PrintV(const char* format, va_list ap) {
  return debug_control_->OutputVaList(DEBUG_OUTPUT_NORMAL, format, ap);
}

HRESULT ChromeExtsCommand::PrintErrorf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  HRESULT hr = PrintErrorV(format, ap);
  va_end(ap);
  return hr;
}

HRESULT ChromeExtsCommand::PrintErrorV(const char* format, va_list ap) {
  return debug_control_->OutputVaList(DEBUG_OUTPUT_ERROR, format, ap);
}

}  // namespace chromeexts
}  // namespace win
}  // namespace tools
