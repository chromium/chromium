// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_TYPES_H_
#define UI_GTK_GTK_TYPES_H_

#include "ui/gtk/gtk_buildflags.h"

// This file provides types that are only available in specific versions of GTK.

extern "C" {
#if BUILDFLAG(GTK_VERSION) == 3
using GskRenderNode = struct _GskRenderNode;
enum GskRenderNodeType : int;
GskRenderNodeType gsk_render_node_get_node_type(GskRenderNode* node);
#endif
}

#endif  // UI_GTK_GTK_TYPES_H_
