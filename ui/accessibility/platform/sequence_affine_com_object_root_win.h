// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_SEQUENCE_AFFINE_COM_OBJECT_ROOT_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_SEQUENCE_AFFINE_COM_OBJECT_ROOT_WIN_H_

#include <atlcom.h>

#include "base/sequence_checker.h"

namespace ui {

// A CComObjectRoot for a single-threaded object that uses a `SEQUENCE_CHECKER`
// to assert that reference counting is performed on the correct sequence.
class SequenceAffineComObjectRoot
    : public CComObjectRootEx<CComSingleThreadModel> {
 public:
  // CComObjectRootEx (non-virtual) overrides:
  ULONG InternalAddRef() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return CComObjectRootEx<CComSingleThreadModel>::InternalAddRef();
  }

  ULONG InternalRelease() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return CComObjectRootEx<CComSingleThreadModel>::InternalRelease();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_SEQUENCE_AFFINE_COM_OBJECT_ROOT_WIN_H_
