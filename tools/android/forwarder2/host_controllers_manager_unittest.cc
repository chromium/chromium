// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/host_controllers_manager.h"

#include <cstdio>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace forwarder2 {

namespace {

int UnusedGetExitNotifierFD() {
  return 0;
}

base::FilePath CreateScript(const std::string script_contents) {
  base::FilePath script_file;
  FILE* script_file_handle = base::CreateAndOpenTemporaryFile(&script_file);
  base::WriteFile(script_file, script_contents.c_str(),
                  script_contents.length());
  base::CloseFile(script_file_handle);
  base::SetPosixFilePermissions(script_file,
                                base::FILE_PERMISSION_READ_BY_USER |
                                    base::FILE_PERMISSION_EXECUTE_BY_USER);
  return script_file;
}

}  // anonymous namespace

// Ensure that we don't start the adb binary with superfluous file descriptors
// from the parent process.
TEST(HostControllersManagerTest, AdbNoExtraFds) {
  HostControllersManager manager(base::BindRepeating(&UnusedGetExitNotifierFD));
  base::FilePath unrelated_file;
  base::ScopedFILE open_unrelated_file(
      CreateAndOpenTemporaryFile(&unrelated_file));
  const int unrelated_fd = fileno(open_unrelated_file.get());
  base::FilePath adb_script =
      CreateScript(base::StringPrintf("#! /bin/sh\n"
                                      "test ! -e /proc/$$/fd/%d\n",
                                      unrelated_fd));
  const std::string serial("0123456789abcdef");
  const std::string map_call(
      "forward tcp:12345 localabstract:chrome_device_forwarder");
  std::string unused_output;
  ASSERT_TRUE(manager.Adb(adb_script.value(), serial, map_call, &unused_output))
      << "File descriptor " << unrelated_fd << " leaked to child process";
}

// Ensure that we don't mangle the argument order.
TEST(HostControllersManagerTest, AdbArgumentSequence) {
  HostControllersManager manager(base::BindRepeating(&UnusedGetExitNotifierFD));
  base::FilePath adb_script =
      CreateScript(base::StringPrintf("#! /bin/sh\n"
                                      "echo -n \"$@\"\n"));
  const std::string serial("0123456789abcdef");
  const std::string unmap_call("forward --remove tcp:12345");
  std::string output;
  ASSERT_TRUE(manager.Adb(adb_script.value(), serial, unmap_call, &output));
  ASSERT_STREQ("-s 0123456789abcdef forward --remove tcp:12345",
               output.c_str());
}

}  // namespace forwarder2
