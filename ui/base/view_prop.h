// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_VIEW_PROP_H_
#define UI_BASE_VIEW_PROP_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"

#if !BUILDFLAG(IS_WIN) && !defined(USE_AURA)
#error view_prop.h is only for windows and aura builds.
#endif

namespace ui {

// ViewProp maintains a key/value pair for a particular view. ViewProp is
// designed as a replacement for the Win32's SetProp, but does not make use of
// window manager memory. ViewProp shares similar semantics as SetProp, the
// value for a particular view/key pair comes from the last ViewProp created.
class COMPONENT_EXPORT(UI_BASE) ViewProp {
 public:
  // Associates data with a view/key pair. If a ViewProp has already been
  // created for the specified pair |data| replaces the current value.
  //
  // ViewProp does *not* make a copy of the char*, the pointer is used for
  // sorting.
  ViewProp(gfx::AcceleratedWidget view, const char* key, void* data);

  ViewProp(const ViewProp&) = delete;
  ViewProp& operator=(const ViewProp&) = delete;

  ~ViewProp();

  // Returns the value associated with the view/key pair, or NULL if there is
  // none.
  static void* GetValue(gfx::AcceleratedWidget view, const char* key);

  // Returns the key.
  const char* Key() const;

 private:
  class Data;

  // Stores the actual data.
  scoped_refptr<Data> data_;
};

}  // namespace ui

#endif  // UI_BASE_VIEW_PROP_H_
