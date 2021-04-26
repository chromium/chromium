// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_MAIN_H_
#define WEBLAYER_PUBLIC_MAIN_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace weblayer {

class MainDelegate {
 public:
  virtual void PreMainMessageLoopRun() = 0;
  virtual void PostMainMessageLoopRun() = 0;
  virtual void SetMainMessageLoopQuitClosure(
      base::OnceClosure quit_closure) = 0;
};

struct MainParams {
  MainParams();
  MainParams(const MainParams& other);
  ~MainParams();

  MainDelegate* delegate;

  // If set, logging will redirect to this file.
  base::FilePath log_filename;

  // The name of the file that has the PAK data.
  std::string pak_name;
};

int Main(MainParams params
#if defined(OS_WIN)
#if !defined(WIN_CONSOLE_APP)
         ,
         HINSTANCE instance
#endif
#else
         ,
         int argc,
         const char** argv
#endif
);

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_MAIN_H_
