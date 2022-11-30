// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GLIB_GLIB_CAST_H_
#define UI_BASE_GLIB_GLIB_CAST_H_

#include <glib-object.h>

#include "base/check.h"

template <typename T, typename U>
T* GlibCast(U* instance, GType g_type) {
  // Ensure |g_type| is used so the compiler doesn't optimize it away
  // in release builds.
  if (auto* type_class = g_type_class_ref(g_type))
    g_type_class_unref(type_class);
  DCHECK(G_TYPE_CHECK_INSTANCE_TYPE(instance, g_type));
  return reinterpret_cast<T*>(instance);
}

#endif  // UI_BASE_GLIB_GLIB_CAST_H_
