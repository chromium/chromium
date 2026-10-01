from pywebsocket3 import common
from pywebsocket3 import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # All control frames must have a payload length of 125 bytes or less.
    message = 'X' * 126
    request.connection.write(
        stream.create_text_frame(message, opcode=common.OPCODE_PING, fin=1)
    )
