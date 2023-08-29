// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_FIND_WINDOW_H_
#define UI_AURA_TEST_FIND_WINDOW_H_

#include <string>

#include "base/functional/bind.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace aura {

class Env;
class Window;

namespace test {

// Searches the given Window's hierarchy, including the root Window parameter
// itself, for one matching the given predicate.
//
// The predicate returns true to indicate a match (this Window pointer will be
// returned) or false to continue searching. If there is no match, this function
// returns null.
//
// The callback must not mutate the window hierarchy.
template <typename CallbackFunc>
Window* FindFirstWindowMatching(Window* window, const CallbackFunc& cb) {
  if (cb(window)) {
    return window;
  }
  for (Window* cur : window->children()) {
    if (Window* found = FindFirstWindowMatching(cur, cb)) {
      return found;
    }
  }
  return nullptr;
}

// Like the above variant but searches the hierarchies of all windows in the
// environment.
template <typename CallbackFunc>
Window* FindFirstWindowMatching(Env* env, const CallbackFunc& cb) {
  for (WindowTreeHost* host : env->window_tree_hosts()) {
    if (Window* found = FindFirstWindowMatching(host->window(), cb)) {
      return found;
    }
  }
  return nullptr;
}

// Searches all Windows in the environment and returns a pointer to the first
// one found with the matching title. Returns null if not found.
inline Window* FindWindowWithTitle(Env* env, const std::u16string& title) {
  return FindFirstWindowMatching(env, [title](Window* window) -> bool {
    return window->GetTitle() == title;
  });
}

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_FIND_WINDOW_H_
