from pywebsocket3 import common
from pywebsocket3 import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # A new frame is arrived before the previous fragmented frame has finished.
    request.connection.write(
        stream.create_text_frame(
            'This message ', opcode=common.OPCODE_TEXT, fin=0
        )
    )
    request.connection.write(
        stream.create_text_frame(
            'should be ignored.', opcode=common.OPCODE_TEXT, fin=1
        )
    )  # Not OPCODE_CONTINUATION.
