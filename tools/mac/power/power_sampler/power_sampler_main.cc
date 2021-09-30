// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/sampler.h"
#include "tools/mac/power/power_sampler/sampling_controller.h"

int main(int argc, char** argv) {
  power_sampler::SamplingController controller;

  // TODO(siggi): Parse command line, add samplers and monitors to controller.
  //    Fire up a message loop and use it to drive events at the controller
  //    until the session is complete, then exit.

  return 0;
}
