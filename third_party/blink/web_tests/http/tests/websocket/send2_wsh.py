from pywebsocket3 import stream


def web_socket_do_extra_handshake(request):
    pass  # Always accept.


def web_socket_transfer_data(request):
    # send 2 messages in one packet.
    request.connection.write(
        stream.create_text_frame('first message')
        + stream.create_text_frame('second message')
    )
