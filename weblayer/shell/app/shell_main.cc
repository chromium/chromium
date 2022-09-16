// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "weblayer/public/main.h"
#include "weblayer/shell/app/shell_main_params.h"

#if BUILDFLAG(IS_WIN)

#if defined(WIN_CONSOLE_APP)
int main() {
  return weblayer::Main(weblayer::CreateMainParams());
#else
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  return weblayer::Main(weblayer::CreateMainParams(), instance);
#endif
}

#else

int main(int argc, const char** argv) {
  return weblayer::Main(weblayer::CreateMainParams(), argc, argv);
}

#endif  // BUILDFLAG(IS_WIN)
