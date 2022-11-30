# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mod_pywebsocket import common
from mod_pywebsocket.extensions import PerMessageDeflateExtensionProcessor
from mod_pywebsocket.stream import create_header


def _get_permessage_deflate_extension_processor(request):
    for extension_processor in request.ws_extension_processors:
        if isinstance(extension_processor, PerMessageDeflateExtensionProcessor):
            return extension_processor
    return None


def web_socket_do_extra_handshake(request):
    processor = _get_permessage_deflate_extension_processor(request)
    assert processor is not None
    # Remove extension processors other than
    # PerMessageDeflateExtensionProcessor to avoid conflict.
    request.ws_extension_processors = [processor]


def web_socket_transfer_data(request):
    line = request.ws_stream.receive_message()
    # Hello
    payload = b'\xf2\x48\xcd\xc9\xc9\x07\x00\x00\x00\xff\xff'
    # Strip \x00\x00\xff\xff
    stripped = payload[:-4]

    header = create_header(
        common.OPCODE_TEXT,
        len(payload),
        fin=0,
        rsv1=1,
        rsv2=0,
        rsv3=0,
        mask=False)
    request.ws_stream._write(header + payload)

    header = create_header(
        common.OPCODE_CONTINUATION,
        len(stripped),
        fin=1,
        rsv1=0,
        rsv2=0,
        rsv3=0,
        mask=False)
    request.ws_stream._write(header + stripped)


# vi:sts=4 sw=4 et
