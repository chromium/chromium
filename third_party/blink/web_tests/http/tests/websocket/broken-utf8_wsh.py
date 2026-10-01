from pywebsocket3 import common
from pywebsocket3 import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # '\xff' will never appear in UTF-8 encoded data.
    payload = b'This text should be ignored. \xff'
    request.connection.write(
        stream.create_header(common.OPCODE_TEXT, len(payload), 1, 0, 0, 0, 0)
        + payload
    )
