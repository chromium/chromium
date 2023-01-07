// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ChromeDebug {
  internal enum ProcessCategory {
    Browser,
    Renderer,
    Gpu,
    Plugin,
    DelegateExecute,
    MetroViewer,
    Service,
    Other
  }

  // Defines an extension method for the ProcessCategory enum which converts the enum value into
  // the group title.
  internal static class ProcessCategoryExtensions {
    public static string ToGroupTitle(this ProcessCategory category) {
      switch (category) {
      case ProcessCategory.DelegateExecute:
      return "Delegate Execute";
      case ProcessCategory.MetroViewer:
      return "Metro Viewer";
      default:
      return category.ToString();
      }
    }
  }
}
