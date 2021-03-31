// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_compat.h"

#include <dlfcn.h>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_annotations.h"
#include "ui/gtk/gtk_stubs.h"

namespace gtk {

// IMPORTANT: All functions in this file that call dlsym()'ed
// functions should be annotated with DISABLE_CFI_ICALL.

namespace {

void* DlOpen(const char* library_name) {
  void* library = dlopen(library_name, RTLD_LAZY | RTLD_GLOBAL);
  CHECK(library);
  return library;
}

void* DlSym(void* library, const char* name) {
  void* symbol = dlsym(library, name);
  CHECK(symbol);
  return symbol;
}

template <typename T>
auto DlCast(void* symbol) {
  return reinterpret_cast<T*>(symbol);
}

void* GetLibGdkPixbuf() {
  static void* libgdk_pixbuf = DlOpen("libgdk_pixbuf-2.0.so.0");
  return libgdk_pixbuf;
}

void* GetLibGdk3() {
  static void* libgdk3 = DlOpen("libgdk-3.so.0");
  return libgdk3;
}

void* GetLibGtk3() {
  static void* libgtk3 = DlOpen("libgtk-3.so.0");
  return libgtk3;
}

void* GetLibGtk4() {
  static void* libgtk4 = DlOpen("libgtk-4.so.1");
  return libgtk4;
}

void* GetLibGtk() {
  if (GtkCheckVersion(4))
    return GetLibGtk4();
  return GetLibGtk3();
}

}  // namespace

bool LoadGtk(int gtk_version) {
  if (gtk_version < 4) {
    ui_gtk::InitializeGdk_pixbuf(GetLibGdkPixbuf());
    ui_gtk::InitializeGdk(GetLibGdk3());
    ui_gtk::InitializeGtk(GetLibGtk3());
  } else {
    // In GTK4, libgtk provides all gdk_*, gsk_*, and gtk_* symbols.
    ui_gtk::InitializeGdk(GetLibGtk4());
    ui_gtk::InitializeGsk(GetLibGtk4());
    ui_gtk::InitializeGtk(GetLibGtk4());
  }
  return true;
}

bool GtkCheckVersion(int major, int minor, int micro) {
  static auto version =
      std::make_tuple(gtk_get_major_version(), gtk_get_minor_version(),
                      gtk_get_micro_version());
  return version >= std::make_tuple(major, minor, micro);
}

DISABLE_CFI_ICALL
void GtkInit(const std::vector<std::string>& args) {
  static void* gtk_init = DlSym(GetLibGtk(), "gtk_init");
  if (GtkCheckVersion(4)) {
    DlCast<void()>(gtk_init)();
  } else {
    // gtk_init() modifies argv, so make a copy first.
    size_t args_chars = 0;
    for (const auto& arg : args)
      args_chars += arg.size() + 1;
    std::vector<char> args_copy(args_chars);
    std::vector<char*> argv;
    char* dst = args_copy.data();
    for (const auto& arg : args) {
      argv.push_back(strcpy(dst, arg.c_str()));
      dst += arg.size() + 1;
    }

    int gtk_argc = argv.size();
    char** gtk_argv = argv.data();
    {
      // http://crbug.com/423873
      ANNOTATE_SCOPED_MEMORY_LEAK;
      DlCast<void(int*, char***)>(gtk_init)(&gtk_argc, &gtk_argv);
    }
  }
}

}  // namespace gtk
