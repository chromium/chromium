// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/public/main.h"
#include "weblayer/shell/app/shell_main_params.h"

#if defined(OS_WIN)

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

#endif  // OS_POSIX
