// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/win/chromeexts/chrome_exts_command.h"

#include <stdio.h>

#include <string>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"

namespace tools {
namespace win {
namespace chromeexts {

ChromeExtsCommand::~ChromeExtsCommand() = default;

ChromeExtsCommand::ChromeExtsCommand() = default;

HRESULT ChromeExtsCommand::Initialize(IDebugClient* debug_client,
                                      const char* args) {
  DCHECK(debug_client);
  DCHECK(args);

  debug_client_ = debug_client;
  debug_control_ = GetDebugClientAs<IDebugControl>();
  if (!debug_control_) {
    return E_FAIL;
  }

  // base::CommandLine assumes the first token to be the command itself. The
  // windbg args do not include this and must be included manually.
  command_line_.ParseFromString(std::wstring(L"cmd ") +
                                base::ASCIIToWide(args));

  return S_OK;
}

HRESULT ChromeExtsCommand::Printf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  HRESULT hr = PrintV(format, ap);
  va_end(ap);
  return hr;
}

HRESULT ChromeExtsCommand::PrintfWithIndent(int indent_level,
                                            const char* format,
                                            ...) {
  for (int i = 0; i < indent_level; i++) {
    Printf("  ");
  }

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

HRESULT ChromeExtsCommand::PrintDml(const char* dml_string) {
  auto debug_control4 = GetDebugClientAs<IDebugControl4>();
  if (!debug_control4) {
    // Fall back to plain text if IDebugControl4 is not available.
    return debug_control_->Output(DEBUG_OUTPUT_NORMAL, "%s", dml_string);
  }
  return debug_control4->ControlledOutput(
      DEBUG_OUTCTL_ALL_CLIENTS | DEBUG_OUTCTL_DML, DEBUG_OUTPUT_NORMAL, "%s",
      dml_string);
}

HRESULT ChromeExtsCommand::PrintDmlf(const char* format, ...) {
  char buffer[4096];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);
  buffer[sizeof(buffer) - 1] = '\0';
  return PrintDml(buffer);
}

}  // namespace chromeexts
}  // namespace win
}  // namespace tools
