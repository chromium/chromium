// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_PUBLIC_CPP_MESSAGE_CENTER_PUBLIC_EXPORT_H_
#define UI_MESSAGE_CENTER_PUBLIC_CPP_MESSAGE_CENTER_PUBLIC_EXPORT_H_

// Defines MESSAGE_CENTER_PUBLIC_EXPORT so that functionality implemented by the
// message_center module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MESSAGE_CENTER_PUBLIC_IMPLEMENTATION)
#define MESSAGE_CENTER_PUBLIC_EXPORT __declspec(dllexport)
#else
#define MESSAGE_CENTER_PUBLIC_EXPORT __declspec(dllimport)
#endif  // defined(MESSAGE_CENTER_PUBLIC_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MESSAGE_CENTER_PUBLIC_IMPLEMENTATION)
#define MESSAGE_CENTER_PUBLIC_EXPORT __attribute__((visibility("default")))
#else
#define MESSAGE_CENTER_PUBLIC_EXPORT
#endif  // defined(MESSAGE_CENTER_PUBLIC_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define MESSAGE_CENTER_PUBLIC_EXPORT
#endif

#endif  // UI_MESSAGE_CENTER_PUBLIC_CPP_MESSAGE_CENTER_PUBLIC_EXPORT_H_
