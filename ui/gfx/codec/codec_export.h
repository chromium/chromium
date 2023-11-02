// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CODEC_CODEC_EXPORT_H_
#define UI_GFX_CODEC_CODEC_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(CODEC_IMPLEMENTATION)
#define CODEC_EXPORT __declspec(dllexport)
#else
#define CODEC_EXPORT __declspec(dllimport)
#endif  // defined(CODEC_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(CODEC_IMPLEMENTATION)
#define CODEC_EXPORT __attribute__((visibility("default")))
#else
#define CODEC_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define CODEC_EXPORT
#endif

#endif  // UI_GFX_CODEC_CODEC_EXPORT_H_
