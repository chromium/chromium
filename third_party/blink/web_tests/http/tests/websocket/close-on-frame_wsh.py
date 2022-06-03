# Wait for a frame to be received from the client and then immediately close.

from mod_pywebsocket import common
from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # We need to use an internal function to read one frame without
    # waiting for the rest of the message.
    opcode, recv_payload, final, reserved1, reserved2, reserved3 = \
            request.ws_stream._receive_frame()
