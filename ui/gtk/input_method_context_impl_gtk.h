// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_INPUT_METHOD_CONTEXT_IMPL_GTK_H_
#define UI_GTK_INPUT_METHOD_CONTEXT_IMPL_GTK_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/buildflags.h"
#include "ui/base/glib/glib_integers.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_composer.h"
#endif

using GdkWindow = struct _GdkWindow;
using GtkIMContext = struct _GtkIMContext;
using GtkWidget = struct _GtkWidget;

namespace gtk {

class GtkUiPlatform;

// An implementation of LinuxInputMethodContext which uses GtkIMContext
// (gtk-immodule) as a bridge from/to underlying IMEs.
class COMPONENT_EXPORT(GTK) InputMethodContextImplGtk
    : public ui::LinuxInputMethodContext {
 public:
  explicit InputMethodContextImplGtk(
      ui::LinuxInputMethodContextDelegate* delegate,
      const GtkUiPlatform* platform);

  InputMethodContextImplGtk(const InputMethodContextImplGtk&) = delete;
  InputMethodContextImplGtk& operator=(const InputMethodContextImplGtk&) =
      delete;

  ~InputMethodContextImplGtk() override;

  // Overridden from ui::LinuxInputMethodContext
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override;
  bool IsPeekKeyEvent(const ui::KeyEvent& key_event) override;
  void SetCursorLocation(const gfx::Rect& rect) override;
  void Reset() override;
  void UpdateFocus(bool has_client,
                   ui::TextInputType old_type,
                   const TextInputClientAttributes& new_client_attributes,
                   ui::TextInputClient::FocusReason reason) override;
  void SetSurroundingText(const std::u16string& text,
                          const gfx::Range& text_range,
                          const gfx::Range& composition_range,
                          const gfx::Range& selection_range) override;
  ui::VirtualKeyboardController* GetVirtualKeyboardController() override;

#if BUILDFLAG(USE_XKBCOMMON)
  ui::XkbComposer& xkb_composer_for_testing() { return xkb_composer_; }
#endif

 private:
  // GtkIMContext event handlers.  They are shared among |gtk_context_simple_|
  // and |gtk_multicontext_|.
  void OnCommit(GtkIMContext* context, gchar* text);
  void OnPreeditChanged(GtkIMContext* context);
  void OnPreeditEnd(GtkIMContext* context);
  void OnPreeditStart(GtkIMContext* context);

  // Returns the IMContext depending on the currently connected input field
  // type.
  GtkIMContext* GetIMContext();

  GdkWindow* GetTargetWindow(const ui::KeyEvent& key_event);

#if BUILDFLAG(USE_XKBCOMMON)
  // Composes dead-key and Compose (Multi_key) sequences with the system
  // Compose table when `gtk_context` is GtkIMContextSimple, whose built-in
  // table is outdated in GTK3. Returns true if the key press was consumed.
  bool ComposeWithXkb(GtkIMContext* gtk_context,
                      unsigned int keyval,
                      int event_flags);
#endif

  // A set of callback functions.  Must not be nullptr.
  const raw_ptr<ui::LinuxInputMethodContextDelegate> delegate_;

  const raw_ptr<const GtkUiPlatform> platform_;

  // Tracks the input field type.
  ui::TextInputType type_ = ui::TEXT_INPUT_TYPE_NONE;

  // IME's input GTK context.
  ScopedGObject<GtkIMContext> gtk_context_ = nullptr;
  ScopedGObject<GtkIMContext> gtk_simple_context_ = nullptr;

  // These correspond to the GtkImContexts above.
  raw_ptr<GdkWindow> last_set_client_window_for_gtk_context_ = nullptr;
  raw_ptr<GdkWindow> last_set_client_window_for_gtk_simple_comtext_ = nullptr;

  // Last known caret bounds relative to the screen coordinates, in DIPs.
  // Effective only on non-simple context.
  gfx::Rect last_caret_bounds_;

  // Points to a local in DispatchKeyEvent() while GTK filters a key event, and
  // is null otherwise. OnCommit() and OnPreeditChanged() set the pointee to
  // true when the IME commits or changes its preedit string during the filter.
  // Those signals may also fire outside a filter call (e.g. asynchronous IME
  // results), in which case this is null.
  raw_ptr<bool> ime_result_during_filter_ = nullptr;

  std::vector<ScopedGSignal> signals_;

#if BUILDFLAG(USE_XKBCOMMON)
  ui::XkbComposer xkb_composer_;
#endif
};

}  // namespace gtk

#endif  // UI_GTK_INPUT_METHOD_CONTEXT_IMPL_GTK_H_
