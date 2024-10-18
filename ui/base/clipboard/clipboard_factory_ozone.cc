// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/clipboard_ozone.h"
#include "ui/base/ui_base_switches.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

Clipboard* Clipboard::Create() {
  // On Linux Desktop, Ozone's Clipboard impl is always used.
  // On Linux builds of ash-chrome, use platform-backed implementation iff
  // use-system-clipboard command line switch is passed.
  const bool use_ozone_impl =
#if !BUILDFLAG(IS_CHROMEOS)
      true;
#else
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseSystemClipboard);
#endif

  if (use_ozone_impl && OzonePlatform::GetInstance()->GetPlatformClipboard())
    return new ClipboardOzone;
  return new ClipboardNonBacked;
}

}  // namespace ui
