// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/io_surface.h"

#include "components/viz/common/resources/shared_image_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

namespace {

TEST(IOSurface, OddSizeMultiPlanar) {
  base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface =
      CreateIOSurface(gfx::Size(101, 99), viz::MultiPlaneFormat::kNV12);
  DCHECK(io_surface);
  // Plane sizes are rounded up.
  // https://crbug.com/1226056
  EXPECT_EQ(IOSurfaceGetWidthOfPlane(io_surface.get(), 1), 51u);
  EXPECT_EQ(IOSurfaceGetHeightOfPlane(io_surface.get(), 1), 50u);
}

TEST(IOSurface, MachPortRetainDeadName) {
  const mach_port_t task = mach_task_self();

  // Create a port and give it send rights so that it will transition to a dead
  // name when the receive right is removed.
  mach_port_t port = MACH_PORT_NULL;
  ASSERT_EQ(KERN_SUCCESS,
            mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port));
  ASSERT_EQ(KERN_SUCCESS,
            mach_port_insert_right(task, port, port, MACH_MSG_TYPE_MAKE_SEND));

  // Remove the receive right.
  ASSERT_EQ(KERN_SUCCESS,
            mach_port_mod_refs(task, port, MACH_PORT_RIGHT_RECEIVE, -1));
  mach_port_type_t port_type = MACH_PORT_TYPE_NONE;
  ASSERT_EQ(KERN_SUCCESS, mach_port_type(task, port, &port_type));
  ASSERT_TRUE(port_type & MACH_PORT_TYPE_DEAD_NAME)
      << "port should have transitioned to a dead name";

  // Attempting to retain a dead name fails, `Retain(port)` should return NULL.
  mach_port_t result = internal::IOSurfaceMachPortTraits::Retain(port);
  EXPECT_EQ(result, static_cast<mach_port_t>(MACH_PORT_NULL))
      << "IOSurface should not retain a dead port";
}

}  // namespace

}  // namespace gfx
