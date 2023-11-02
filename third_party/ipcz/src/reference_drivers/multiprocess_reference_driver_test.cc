// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/multiprocess_reference_driver.h"

#include <sys/socket.h>
#include <sys/types.h>

#include "ipcz/ipcz.h"
#include "reference_drivers/file_descriptor.h"
#include "reference_drivers/wrapped_file_descriptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz::reference_drivers {
namespace {

TEST(MultiprocessReferenceDriverTest, SendDeactivated) {
  const IpczDriver& driver = kMultiprocessReferenceDriver;
  IpczDriverHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                                    IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                                    nullptr, &a, &b));

  // Activate and immediately deactivate the transport.
  auto handler = +[](IpczHandle, const void*, size_t, const IpczDriverHandle*,
                     size_t, uint32_t, const void*) { return IPCZ_RESULT_OK; };
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.ActivateTransport(a, IPCZ_INVALID_HANDLE, handler,
                                     IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.DeactivateTransport(a, IPCZ_NO_FLAGS, nullptr));

  int fds[2];
  EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));

  // This driver discards outgoing transmissions once the transport has been
  // deactivated. The attached file descriptor should not leak as a result of
  // this Transmit(), and if it does, the test will hang below.
  IpczDriverHandle handle =
      WrappedFileDescriptor::Create(FileDescriptor(fds[0]));
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.Transmit(a, nullptr, 0, &handle, 1, IPCZ_NO_FLAGS, nullptr));

  // Wait for the other side to be closed by the driver. Since this is a
  // blocking socket, a successful zero-length read() indicates EOF.
  FileDescriptor fd(fds[1]);
  uint8_t byte;
  EXPECT_EQ(0, read(fd.get(), &byte, 1));

  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(b, IPCZ_NO_FLAGS, nullptr));
}

}  // namespace
}  // namespace ipcz::reference_drivers
