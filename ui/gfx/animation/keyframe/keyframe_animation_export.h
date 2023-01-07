// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_KEYFRAME_ANIMATION_EXPORT_H_
#define UI_GFX_ANIMATION_KEYFRAME_KEYFRAME_ANIMATION_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GFX_KEYFRAME_ANIMATION_IMPLEMENTATION)
#define GFX_KEYFRAME_ANIMATION_EXPORT __declspec(dllexport)
#else
#define GFX_KEYFRAME_ANIMATION_EXPORT __declspec(dllimport)
#endif  // defined(CC_ANIMATION_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(GFX_KEYFRAME_ANIMATION_IMPLEMENTATION)
#define GFX_KEYFRAME_ANIMATION_EXPORT __attribute__((visibility("default")))
#else
#define GFX_KEYFRAME_ANIMATION_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define GFX_KEYFRAME_ANIMATION_EXPORT
#endif

#endif  // UI_GFX_ANIMATION_KEYFRAME_KEYFRAME_ANIMATION_EXPORT_H_
