# Read 32 messages and verify that they are not fragmented.
# This can be removed if the "reassemble small messages" feature is removed. See
# https://crbug.com/1086273.

from mod_pywebsocket import common
from mod_pywebsocket import msgutil

NUMBER_OF_MESSAGES = 32


def web_socket_do_extra_handshake(request):
    # Disable permessage-deflate because it may reassemble messages.
    request.ws_extension_processors = []


def web_socket_transfer_data(request):
    for i in range(NUMBER_OF_MESSAGES):
        # We need to use an internal function to verify that the frame has the
        # "final" flag set.
        opcode, recv_payload, final, reserved1, reserved2, reserved3 = \
            request.ws_stream._receive_frame()

        # We assume that the browser will not send any control messages.
        if opcode != common.OPCODE_BINARY:
            msgutil.send_message(request, 'FAIL: message %r was not opcode binary' % i)
            return

        if not final:
            msgutil.send_message(request, 'FAIL: message %r was fragmented' % i)
            return

        msgutil.send_message(request, 'OK: message %r not fragmented' % i)
