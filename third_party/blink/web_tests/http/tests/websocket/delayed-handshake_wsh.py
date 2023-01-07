# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time


def web_socket_do_extra_handshake(request):
    time.sleep(1)  # Wait for 1 second and then always accept.


def web_socket_transfer_data(request):
    pass  # do nothing
