// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SHELL_DIALOGS_EXPORT_H_
#define UI_SHELL_DIALOGS_SHELL_DIALOGS_EXPORT_H_

// Defines SHELL_DIALOGS_EXPORT so that functionality implemented by the Shell
// Dialogs module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SHELL_DIALOGS_IMPLEMENTATION)
#define SHELL_DIALOGS_EXPORT __declspec(dllexport)
#else
#define SHELL_DIALOGS_EXPORT __declspec(dllimport)
#endif  // defined(SHELL_DIALOGS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(SHELL_DIALOGS_IMPLEMENTATION)
#define SHELL_DIALOGS_EXPORT __attribute__((visibility("default")))
#else
#define SHELL_DIALOGS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define SHELL_DIALOGS_EXPORT
#endif

#endif  // UI_SHELL_DIALOGS_SHELL_DIALOGS_EXPORT_H_

