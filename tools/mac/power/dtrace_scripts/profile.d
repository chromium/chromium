// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script profiles the execution of a process and all its children.
// Execute like this:
//   sudo dtrace -s profile.d $(pgrep -x "Google Chrome")

// Profile with a high frequency that is prime to avoid unfortunate alignement
// with periods of repeating tasks internal to the process. The frequency was
// verified as supported by macOS Monterey running on Intel. See
// https://illumos.org/books/dtrace/chp-profile.html#chp-profile-5 for details.

profile-997/(pid == $1 || ppid == $1)/
{
  @[ustack(128)] = count();
}
