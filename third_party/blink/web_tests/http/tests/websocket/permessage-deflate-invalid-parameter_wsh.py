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
from mod_pywebsocket import handshake
from mod_pywebsocket.handshake.hybi import compute_accept_from_unicode


def web_socket_do_extra_handshake(request):
    resources = request.ws_resource.split('?', 1)
    parameters = None
    if len(resources) == 2:
        parameters = parse.unquote(resources[1])
    message_parameters = (b'; %s\r\n' % parameters.encode('utf-8') if parameters else b'\r\n')

    message = (b'HTTP/1.1 101 Switching Protocols\r\n'
               b'Upgrade: websocket\r\n'
               b'Connection: Upgrade\r\n'
               b'Sec-WebSocket-Accept: %s\r\n'
               b'Sec-WebSocket-Extensions: permessage-deflate'
               b'%s\r\n') % (compute_accept_from_unicode(request.headers_in['Sec-WebSocket-Key']), message_parameters)

    request.connection.write(message)
    # Prevents pywebsocket from sending its own handshake message.
    raise handshake.AbortedByUserException('Abort the connection')


def web_socket_transfer_data(request):
    pass
