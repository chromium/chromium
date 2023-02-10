// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fontconfig/fontconfig.h>
#include <ftw.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include <cassert>
#include <string>

#include "third_party/test_fonts/fontconfig/fontconfig_util_linux.h"

// GIANT WARNING: The point of this file is to front-load construction of the
// font cache [which takes 600ms] from test run time to compile time. This saves
// 600ms on each test shard which uses the font cache into compile time. The
// problem is that fontconfig cache construction is not intended to be
// deterministic. This executable tries to set some external state to ensure
// determinism. We have no way of guaranteeing that this produces correct
// results, or even has the intended effect.
int main() {
  auto sysroot = test_fonts::GetSysrootDir();

  // This is the MD5 hash of "/test_fonts", which is used as the key of the
  // fontconfig cache.
  //     $ echo -n /test_fonts | md5sum
  //     fb5c91b2895aa445d23aebf7f9e2189c  -
  static const char kCacheKey[] = "fb5c91b2895aa445d23aebf7f9e2189c";

  // fontconfig writes the mtime of the test_fonts directory into the cache. It
  // presumably checks this later to ensure that the cache is still up to date.
  // We set the mtime to an arbitrary, fixed time in the past.
  std::string test_fonts_file_path = sysroot + "/test_fonts";
  struct stat old_times;
  struct utimbuf new_times;

  stat(test_fonts_file_path.c_str(), &old_times);
  new_times.actime = old_times.st_atime;
  // Use an arbitrary, fixed time.
  new_times.modtime = 123456789;
  utime(test_fonts_file_path.c_str(), &new_times);

  std::string fontconfig_caches = sysroot + "/fontconfig_caches";

  // Delete directory before generating fontconfig caches. This will notify
  // future fontconfig_caches changes.
  auto callback = [](const char* fpath, const struct stat* sb, int typeflag,
                     struct FTW* ftwbuf) { return remove(fpath); };
  nftw(fontconfig_caches.c_str(), callback, 128, FTW_DEPTH | FTW_PHYS);

  test_fonts::SetUpFontconfig();
  FcInit();
  FcFini();

  // Check existence of intended fontconfig cache file.
  auto cache = fontconfig_caches + "/" + kCacheKey + "-le64.cache-9";
  bool cache_exists = access(cache.c_str(), F_OK) == 0;
  return !cache_exists;
}
