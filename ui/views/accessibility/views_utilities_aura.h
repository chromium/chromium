// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEWS_UTILITIES_AURA_H_
#define UI_VIEWS_ACCESSIBILITY_VIEWS_UTILITIES_AURA_H_

namespace aura {
class Window;
}

namespace views {

// Return the parent of |window|, first checking to see if it has a
// transient parent. This allows us to walk up the aura::Window
// hierarchy when it spans multiple window tree hosts, each with
// their own native window.
aura::Window* GetWindowParentIncludingTransient(aura::Window* window);

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEWS_UTILITIES_AURA_H_
