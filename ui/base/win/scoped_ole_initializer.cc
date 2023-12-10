// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/scoped_ole_initializer.h"

#include <ole2.h>

#include <ostream>

#include "base/check_op.h"
#include "base/win/resource_exhaustion.h"

namespace ui {

ScopedOleInitializer::ScopedOleInitializer() {
  hr_ = OleInitialize(NULL);
  DCHECK_NE(OLE_E_WRONGCOMPOBJ, hr_) << "Incompatible DLLs on machine";
  DCHECK_NE(RPC_E_CHANGED_MODE, hr_) << "Invalid COM thread model change";

  // OleInitialize is calling CoInitializeEx to initialize COM. CoInitializeEx
  // may call RegisterClassEx to get an ATOM. On failure, the call to
  // RegisterClassEx sets the last error code to ERROR_NOT_ENOUGH_MEMORY.
  // CoInitializeEx is retuning the converted error code
  // (a.k.a HRESULT_FROM_WIN32(...)). The following code handles the case
  // where HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY) is being returned by
  // CoInitializeEx. We assume they are due to ATOM exhaustion. This appears to
  // happen most often when the browser is being driven by automation tools,
  // though the underlying reason for this remains a mystery
  // (https://crbug.com/1470483). There is nothing that Chrome can do to
  // meaningfully run until the user restarts their session by signing out of
  // Windows or restarting their computer.
  if (hr_ == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY)) {
    base::win::OnResourceExhausted();
  }
}

ScopedOleInitializer::~ScopedOleInitializer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (SUCCEEDED(hr_))
    OleUninitialize();
}

}  // namespace ui
