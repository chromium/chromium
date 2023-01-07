// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODES_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODES_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "ui/events/keycodes/keyboard_codes_win.h"
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "ui/events/keycodes/keyboard_codes_posix.h"
#endif

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODES_H_
