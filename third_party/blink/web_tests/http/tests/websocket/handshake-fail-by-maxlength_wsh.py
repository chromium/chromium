# Copyright (C) Research In Motion Limited 2011. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
# disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
# following disclaimer in the documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS “AS IS” AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from mod_pywebsocket import handshake
from mod_pywebsocket.handshake.hybi import compute_accept_from_unicode


def web_socket_do_extra_handshake(request):
    # This will cause the handshake to fail because it pushes the length of the
    # status line past 1024 characters
    msg = (b'%s'
           b'HTTP/1.1 101 Switching Protocols\r\n'
           b'Upgrade: websocket\r\n'
           b'Connection: Upgrade\r\n'
           b'Sec-WebSocket-Accept: %s\r\n'
           b'\r\n') % (b'.' * 1024, compute_accept_from_unicode(request.headers_in['Sec-WebSocket-Key']))

    request.connection.write(msg)
    # Prevents pywebsocket from sending its own handshake message.
    raise handshake.AbortedByUserException('abort the connection')


def web_socket_transfer_data(request):
    pass
