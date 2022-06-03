from mod_pywebsocket import common
from mod_pywebsocket import handshake
from mod_pywebsocket import stream
from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # pywebsocket does not mask message by default. We need to build a frame
    # manually to mask it.
    request.connection.write(
        stream.create_text_frame('First message', mask=False))

    request.connection.write(
        stream.create_text_frame(
            'Fragmented ', opcode=common.OPCODE_TEXT, fin=0, mask=False))
    request.connection.write(
        stream.create_text_frame(
            'message', opcode=common.OPCODE_CONTINUATION, fin=1, mask=False))

    request.connection.write(stream.create_text_frame('', mask=False))

    msgutil.send_message(request, 'END')

    # Wait for the client to start closing handshake. To receive a close frame,
    # we must use an internal method of request.ws_stream.
    opcode, payload, final, reserved1, reserved2, reserved3 = \
        request.ws_stream._receive_frame()
    assert opcode == common.OPCODE_CLOSE
    assert final
    assert not reserved1
    assert not reserved2
    assert not reserved3

    # Send a masked close frame. Clients should be able to handle this frame
    # and the WebSocket object should be closed cleanly.
    request.connection.write(stream.create_close_frame(b'', mask=False))

    # Prevents pywebsocket from starting its own closing handshake.
    raise handshake.AbortedByUserException('Abort the connection')
