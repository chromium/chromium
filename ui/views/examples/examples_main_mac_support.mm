// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_main_mac_support.h"

#include "base/apple/bundle_locations.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

namespace {

// Update BUILD.gn if the below changes.
const char kFrameworkName[] = "Views Examples Framework.framework";
const char kFrameworkVersion[] = "1.0.0.0";

base::FilePath GetFrameworkBundlePath() {
  // It's tempting to use +[NSBundle bundleWithIdentifier:], but it's really
  // slow (about 30ms on 10.5 and 10.6), despite Apple's documentation stating
  // that it may be more efficient than +bundleForClass:.  +bundleForClass:
  // itself takes 1-2ms.  Getting an NSBundle from a path, on the other hand,
  // essentially takes no time at all, at least when the bundle has already
  // been loaded as it will have been in this case.  The FilePath operations
  // needed to compute the framework's path are also effectively free, so that
  // is the approach that is used here.  NSBundle is also documented as being
  // not thread-safe, and thread safety may be a concern here.

  // Start out with the path to the running executable.
  base::FilePath path;
  base::PathService::Get(base::FILE_EXE, &path);

  // One step up to MacOS, another to Contents.
  path = path.DirName().DirName();
  DCHECK_EQ(path.BaseName().value(), "Contents");

  // |path| is Chromium.app/Contents, so go down to
  // Chromium.app/Contents/Frameworks/Chromium Framework.framework/Versions/X.
  path = path.Append("Frameworks")
             .Append(kFrameworkName)
             .Append("Versions")
             .Append(kFrameworkVersion);
  DCHECK_EQ(path.BaseName().value(), kFrameworkVersion);
  DCHECK_EQ(path.DirName().BaseName().value(), "Versions");
  DCHECK_EQ(path.DirName().DirName().BaseName().value(), kFrameworkName);
  DCHECK_EQ(path.DirName().DirName().DirName().BaseName().value(),
            "Frameworks");
  DCHECK_EQ(path.DirName()
                .DirName()
                .DirName()
                .DirName()
                .DirName()
                .BaseName()
                .Extension(),
            ".app");
  return path;
}

}  // namespace

void UpdateFrameworkBundlePath() {
  @autoreleasepool {
    base::apple::SetOverrideFrameworkBundlePath(GetFrameworkBundlePath());
  }
}
