// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script profiles the execution of a process and all its children.
// Execute like this:
//   sudo dtrace -s profile.d $(pgrep -x "Google Chrome")

// Note on results produced:
//
// This script will produce a data file suitable to be converted to a pprof by
// export_dtrace.py. The flamegraph generated will present time spent on-cpu as
// a fraction of the total. More time spent on-cpu does not necessarily mean
// more power consumed. A CPU running at a low frequency will take a long time
// to execute work but might do it in a more power-efficient way than if it was
// running at a higher frequency. The same can be said about core selection in
// a big.LITTLE style architecture.

// Profile with a high frequency that is prime to avoid unfortunate alignement
// with periods of repeating tasks internal to the process. The frequency was
// verified as supported by macOS Monterey running on Intel. See
// illumos.org/books/dtrace/chp-profile.html#chp-profile-5 for details.
profile-997/(pid == $1 || ppid == $1)/
{
  @[ustack(512)] = count();
}

// Future work:
// Currently the frequency data is not filled in within the |curcpu| variable
// on macOS. The |cpu| variable is correctly filled in so applying some notion
// of per-core type weight will be integrated into this script eventually.
