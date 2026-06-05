// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_TESTS_ANGLE_THIRD_PARTY_MOCK_EGL_H_
#define TOOLS_CLANG_SPANIFY_TESTS_ANGLE_THIRD_PARTY_MOCK_EGL_H_

typedef float GLfloat;

// Simulate a dynamically loaded function pointer in ANGLE.
// This represents an external API (excluded from spanification).
extern void (*glVertexAttrib4fv)(int index, const GLfloat* v);

#endif  // TOOLS_CLANG_SPANIFY_TESTS_ANGLE_THIRD_PARTY_MOCK_EGL_H_
