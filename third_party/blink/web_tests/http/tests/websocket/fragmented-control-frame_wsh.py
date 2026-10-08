from pywebsocket3 import common
from pywebsocket3 import msgutil  # noqa: F401
from pywebsocket3 import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # Fragmented control frame is prohibited. The client must abort the
    # connection.
    request.connection.write(
        stream.create_text_frame(
            'This message should be ignored.', opcode=common.OPCODE_PING, fin=0
        )
    )
