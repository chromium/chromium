from pywebsocket3 import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # pywebsocket does not mask message by default. We need to build a frame
    # manually to mask it.
    request.connection.write(
        stream.create_text_frame('The Masked Message', mask=True)
    )
