# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def web_socket_do_extra_handshake(request):
    request.extra_headers.append(('foo', ''))


def web_socket_transfer_data(request):
    pass
