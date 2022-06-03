# Copyright 2013, Google Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from six.moves.urllib import parse
import zlib
from mod_pywebsocket import common
from mod_pywebsocket import util
from mod_pywebsocket.extensions import PerMessageDeflateExtensionProcessor
from mod_pywebsocket.extensions import ExtensionProcessorInterface
from mod_pywebsocket.common import ExtensionParameter

_GOODBYE_MESSAGE = u'Goodbye'
_ENABLE_MESSAGE = u'EnableCompression'
_DISABLE_MESSAGE = u'DisableCompression'
_bfinal = False
_client_max_window_bits = 15


def _get_permessage_deflate_extension_processor(request):
    for extension_processor in request.ws_extension_processors:
        if isinstance(extension_processor, PerMessageDeflateExtensionProcessor):
            return extension_processor
    return None


def web_socket_do_extra_handshake(request):
    global _bfinal
    global _client_max_window_bits
    processor = _get_permessage_deflate_extension_processor(request)
    # Remove extension processors other than
    # PerMessageDeflateExtensionProcessor to avoid conflict.
    request.ws_extension_processors = [processor]
    if not processor:
        return
    r = request.ws_resource.split('?', 1)
    if len(r) == 1:
        return
    parameters = parse.parse_qs(r[1], keep_blank_values=True)
    if 'client_max_window_bits' in parameters:
        window_bits = int(parameters['client_max_window_bits'][0])
        processor.set_client_max_window_bits(window_bits)
        _client_max_window_bits = window_bits
    if 'client_no_context_takeover' in parameters:
        processor.set_client_no_context_takeover(True)
    if 'set_bfinal' in parameters:
        _bfinal = True


def receive(request):
    stream = request.ws_stream
    possibly_compressed_body = b''
    compress = False
    while True:
        frame = stream._receive_frame_as_frame_object()
        if frame.opcode == common.OPCODE_CLOSE:
            message = stream._get_message_from_frame(frame)
            stream._process_close_message(message)
            return (False, None)
        compress = compress or frame.rsv1
        possibly_compressed_body += frame.payload
        if frame.fin:
            break
    if compress:
        return (compress, possibly_compressed_body + b'\x00\x00\xff\xff')
    else:
        return (compress, possibly_compressed_body)


def web_socket_transfer_data(request):
    processor = _get_permessage_deflate_extension_processor(request)
    processor.set_bfinal(_bfinal)
    inflater = util._Inflater(_client_max_window_bits)
    while True:
        compress, possibly_compressed_body = receive(request)
        body = None
        if possibly_compressed_body is None:
            return
        if compress:
            inflater.append(possibly_compressed_body)
            body = inflater.decompress(-1)
        else:
            body = possibly_compressed_body

        text = body.decode('utf-8')
        if processor:
            if text == _ENABLE_MESSAGE:
                processor.enable_outgoing_compression()
            elif text == _DISABLE_MESSAGE:
                processor.disable_outgoing_compression()
        request.ws_stream.send_message(text, binary=False)
        if text == _GOODBYE_MESSAGE:
            return


# vi:sts=4 sw=4 et
