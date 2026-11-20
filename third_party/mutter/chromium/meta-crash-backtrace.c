/*
 * Copyright 2026 The Chromium Authors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * Prints a backtrace to stderr when mutter crashes.
 *
 * testing/xvfb.py supervises mutter and restarts it on crash, but CI bots
 * don't keep core dumps, so the only trace left in the test log is
 * "*** mutter died with signal ... ***" from testing/xvfb.py. Print the
 * crashing thread's stack to stderr so the crash can be diagnosed. Frames are
 * printed as "module(symbol+offset)[address]" or "module(+offset)[address]";
 * the latter can be symbolized offline with
 * llvm-symbolizer --obj=<module> <offset> against the binaries in the test's
 * CAS input.
 */

#include <execinfo.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define MAX_FRAMES 128

static const int crash_signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };

static const char *
signal_name (int signum)
{
  switch (signum)
    {
    case SIGSEGV:
      return "SIGSEGV";
    case SIGBUS:
      return "SIGBUS";
    case SIGILL:
      return "SIGILL";
    case SIGFPE:
      return "SIGFPE";
    case SIGABRT:
      return "SIGABRT";
    default:
      return "UNKNOWN";
    }
}

static void
write_str (const char *str)
{
  ssize_t ignored = write (STDERR_FILENO, str, strlen (str));
  (void) ignored;
}

static void
crash_handler (int signum)
{
  void *frames[MAX_FRAMES];
  int n_frames;

  write_str ("\nmutter received signal ");
  write_str (signal_name (signum));
  write_str (", backtrace:\n");

  /* backtrace_symbols_fd() doesn't allocate, which matters when the heap is
   * corrupted. */
  n_frames = backtrace (frames, MAX_FRAMES);
  backtrace_symbols_fd (frames, n_frames, STDERR_FILENO);
  write_str ("end of mutter backtrace\n");

  /* SA_RESETHAND restored the default action; re-raise so that the exit
   * status still reports the signal. */
  raise (signum);
}

__attribute__ ((constructor)) static void
install_crash_handler (void)
{
  static char alt_stack[64 * 1024];
  stack_t ss = { 0 };
  struct sigaction sa = { 0 };
  void *frame;
  size_t i;

  /* The first backtrace() call loads libgcc_s, which allocates. Do it now
   * rather than in the signal handler. */
  backtrace (&frame, 1);

  /* Handle stack overflows too. */
  ss.ss_sp = alt_stack;
  ss.ss_size = sizeof (alt_stack);
  sigaltstack (&ss, NULL);

  sa.sa_handler = crash_handler;
  sa.sa_flags = SA_RESETHAND | SA_ONSTACK;
  sigemptyset (&sa.sa_mask);
  for (i = 0; i < sizeof (crash_signals) / sizeof (crash_signals[0]); i++)
    sigaction (crash_signals[i], &sa, NULL);
}
