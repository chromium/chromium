// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/multinode_test.h"

#include "ipcz/ipcz.h"
#include "reference_drivers/single_process_reference_driver.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::test {

namespace {

const IpczDriver* GetDriverImpl(MultinodeTest::DriverMode mode) {
  switch (mode) {
    case MultinodeTest::DriverMode::kSync:
      return &reference_drivers::kSingleProcessReferenceDriver;

    default:
      // Other modes not yet supported.
      return nullptr;
  }
}

}  // namespace

MultinodeTest::MultinodeTest() = default;

MultinodeTest::~MultinodeTest() = default;

const IpczDriver& MultinodeTest::GetDriver(DriverMode mode) const {
  const IpczDriver* driver = GetDriverImpl(mode);
  ABSL_ASSERT(driver);
  return *driver;
}

IpczHandle MultinodeTest::CreateBrokerNode(DriverMode mode) {
  IpczHandle node;
  ipcz().CreateNode(&GetDriver(mode), IPCZ_INVALID_DRIVER_HANDLE,
                    IPCZ_CREATE_NODE_AS_BROKER, nullptr, &node);
  return node;
}

IpczHandle MultinodeTest::CreateNonBrokerNode(DriverMode mode) {
  IpczHandle node;
  ipcz().CreateNode(&GetDriver(mode), IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                    nullptr, &node);
  return node;
}

void MultinodeTest::CreateBrokerToNonBrokerTransports(
    DriverMode mode,
    IpczDriverHandle* transport0,
    IpczDriverHandle* transport1) {
  // TODO: Support other DriverModes.
  ABSL_ASSERT(mode == DriverMode::kSync);
  IpczResult result = GetDriver(mode).CreateTransports(
      IPCZ_INVALID_DRIVER_HANDLE, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
      nullptr, transport0, transport1);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
}

}  // namespace ipcz::test
