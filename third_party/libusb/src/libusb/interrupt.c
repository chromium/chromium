// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "libusbi.h"

int API_EXPORTED libusb_interrupt_handle_event(struct libusb_context* ctx) {
  unsigned char dummy = 1;
  USBI_GET_CONTEXT(ctx);
  return usbi_write(ctx->ctrl_pipe[1], &dummy, sizeof(dummy));
}
