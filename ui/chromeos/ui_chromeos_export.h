// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_UI_CHROMEOS_EXPORT_H_
#define UI_CHROMEOS_UI_CHROMEOS_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(UI_CHROMEOS_IMPLEMENTATION)
#define UI_CHROMEOS_EXPORT __attribute__((visibility("default")))
#else
#define UI_CHROMEOS_EXPORT
#endif

#else  // defined(COMPONENT_BUILD)
#define UI_CHROMEOS_EXPORT
#endif

#endif  // UI_CHROMEOS_UI_CHROMEOS_EXPORT_H_
