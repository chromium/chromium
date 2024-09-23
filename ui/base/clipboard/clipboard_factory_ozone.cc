// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/clipboard_ozone.h"
#include "ui/base/ui_base_switches.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

Clipboard* Clipboard::Create() {
  // On Linux Desktop and Lacros, Ozone's Clipboard impl is always used.
  // On Linux builds of ash-chrome, use platform-backed implementation iff
  // use-system-clipboard command line switch is passed.
  const bool use_ozone_impl =
#if !BUILDFLAG(IS_CHROMEOS_ASH)
      true;
#else
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseSystemClipboard);
#endif

  if (use_ozone_impl && OzonePlatform::GetInstance()->GetPlatformClipboard())
    return new ClipboardOzone;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  NOTREACHED() << "System clipboard integration should be in place.";
#else
  return new ClipboardNonBacked;
#endif
}

}  // namespace ui
