// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary ShowOptions {
  // Relative url of the contents to show.
  required DOMString url;

  // Whether the user can close the window, defaults to false.
  boolean userCanClose;
};

// Use the <code>chrome.loginScreenUi</code> API to show and hide custom
// login UI.
[platforms=("chromeos"),
 implemented_in="chrome/browser/ash/extensions/login_screen_ui/login_screen_ui_api.h"]
interface LoginScreenUi {
  // Opens a window, which is visible on top of the login and lock screen.
  // |options|: Options for the custom login UI window.
  // |Returns|: Callback that does not take arguments.
  static Promise<undefined> show(ShowOptions options);

  // Closes the login/lock screen UI window previously opened by this
  // extension.
  // |Returns|: Callback that does not take arguments.
  static Promise<undefined> close();
};

partial interface Browser {
  static attribute LoginScreenUi loginScreenUi;
};
