// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_WINDOW_IMPL_H_
#define UI_GFX_WIN_WINDOW_IMPL_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/win/msg_util.h"

namespace gfx {

// An interface implemented by classes that use message maps.
// ProcessWindowMessage is implemented by the BEGIN_MESSAGE_MAP_EX macro.
class MessageMapInterface {
 public:
  // Processes one message from the window's message queue.
  virtual BOOL ProcessWindowMessage(HWND window,
                                    UINT message,
                                    WPARAM w_param,
                                    LPARAM l_param,
                                    LRESULT& result,
                                    DWORD msg_map_id = 0) = 0;
};

///////////////////////////////////////////////////////////////////////////////
//
// WindowImpl
//  A convenience class that encapsulates the details of creating and
//  destroying a HWND.  This class also hosts the windows procedure used by all
//  Windows.
//
///////////////////////////////////////////////////////////////////////////////
class GFX_EXPORT WindowImpl : public MessageMapInterface {
 public:
  // |debugging_id| is reported with crashes to help attribute the code that
  // created the WindowImpl.
  explicit WindowImpl(const std::string& debugging_id = std::string());
  virtual ~WindowImpl();

  // Causes all generated windows classes to be unregistered at exit.
  // This can cause result in errors for tests that don't destroy all instances
  // of windows, but is necessary if the tests unload the classes WndProc.
  static void UnregisterClassesAtExit();

  // Initializes the Window with a parent and an initial desired size.
  void Init(HWND parent, const gfx::Rect& bounds);

  // Returns the default window icon to use for windows of this type.
  virtual HICON GetDefaultWindowIcon() const;
  virtual HICON GetSmallWindowIcon() const;

  // Returns the HWND associated with this Window.
  HWND hwnd() const { return hwnd_; }

  // Sets the window styles. This is ONLY used when the window is created.
  // In other words, if you invoke this after invoking Init, nothing happens.
  void set_window_style(DWORD style) { window_style_ = style; }
  DWORD window_style() const { return window_style_; }

  // Sets the extended window styles. See comment about |set_window_style|.
  void set_window_ex_style(DWORD style) { window_ex_style_ = style; }
  DWORD window_ex_style() const { return window_ex_style_; }

  // Sets the class style to use. The default is CS_DBLCLKS.
  void set_initial_class_style(UINT class_style) {
    // We dynamically generate the class name, so don't register it globally!
    DCHECK_EQ((class_style & CS_GLOBALCLASS), 0u);
    class_style_ = class_style;
  }
  UINT initial_class_style() const { return class_style_; }

  const std::string& debugging_id() const { return debugging_id_; }

 protected:
  // Handles the WndProc callback for this object.
  virtual LRESULT OnWndProc(UINT message, WPARAM w_param, LPARAM l_param);

  // Subclasses must call this method from their destructors to ensure that
  // this object is properly disassociated from the HWND during destruction,
  // otherwise it's possible this object may still exist while a subclass is
  // destroyed.
  void ClearUserData();

 private:
  friend class ClassRegistrar;

  // The window procedure used by all Windows.
  static LRESULT CALLBACK WndProc(HWND window,
                                  UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param);

  // Gets the window class atom to use when creating the corresponding HWND.
  // If necessary, this registers the window class.
  ATOM GetWindowClassAtom();

  // All classes registered by WindowImpl start with this name.
  static const wchar_t* const kBaseClassName;

  const std::string debugging_id_;

  // Window Styles used when creating the window.
  DWORD window_style_ = 0;

  // Window Extended Styles used when creating the window.
  DWORD window_ex_style_ = 0;

  // Style of the class to use.
  UINT class_style_;

  // Our hwnd.
  HWND hwnd_ = nullptr;

  // For debugging.
  // TODO(sky): nuke this when get crash data.
  bool got_create_ = false;
  bool got_valid_hwnd_ = false;
  bool* destroyed_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowImpl);
};

}  // namespace gfx

#endif  // UI_GFX_WIN_WINDOW_IMPL_H_
