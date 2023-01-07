import time
from mod_pywebsocket import handshake
from mod_pywebsocket import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    while request.ws_stream.receive_message() is not None:
        pass


def web_socket_passive_closing_handshake(request):
    # Send a close frame.
    request.connection.write(stream.create_close_frame(b''))

    # Chromium will wait 2 seconds for the server to close the connection before
    # giving up and closing it itself. Add an extra second to avoid the test
    # being flaky.
    time.sleep(3)

    # Prevent pywebsocket from sending another close frame.
    raise handshake.AbortedByUserException('Abort the connection')
