// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVATE_MEMBERSHIP_BASE_PRIVATE_MEMBERSHIP_EXPORT_H_
#define PRIVATE_MEMBERSHIP_BASE_PRIVATE_MEMBERSHIP_EXPORT_H_

// PRIVATE_MEMBERSHIP_EXPORT is used to mark symbols as imported or
// exported when private_membership is built or used as a shared library.
// When private_membership is built as a static library the
// PRIVATE_MEMBERSHIP_EXPORT macro expands to nothing.
//
// This export macros doesn't support Windows. There will be additional
// component build work to support Windows (see crbug.com/1269714).

#ifdef PRIVATE_MEMBERSHIP_ENABLE_SYMBOL_EXPORT

#if __has_attribute(visibility)
#define PRIVATE_MEMBERSHIP_EXPORT __attribute__((visibility("default")))
#endif

#endif  // PRIVATE_MEMBERSHIP_ENABLE_SYMBOL_EXPORT

#ifndef PRIVATE_MEMBERSHIP_EXPORT
#define PRIVATE_MEMBERSHIP_EXPORT
#endif

#endif  // PRIVATE_MEMBERSHIP_BASE_PRIVATE_MEMBERSHIP_EXPORT_H_
