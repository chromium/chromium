# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs
import six


def web_socket_do_extra_handshake(request):
    request.ws_extension_processors = []
    request.received_bytes = 0


def web_socket_transfer_data(request):
    while True:
        line = request.ws_stream.receive_message()
        if line is None:
            return
        if isinstance(line, six.text_type):
            request.received_bytes += len(codecs.encode(line, 'utf-8'))
        else:
            request.received_bytes += len(line)


def web_socket_passive_closing_handshake(request):
    return 1000, 'received %d bytes' % request.received_bytes
