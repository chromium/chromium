from mod_pywebsocket import common
from mod_pywebsocket import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    payload1 = b'Invalid continuation frame to be ignored.'
    payload2 = b'Valid frame after closing should be disposed.'
    request.connection.write(
        stream.create_header(common.OPCODE_CONTINUATION,
                             len(payload1), 1, 0, 0, 0, 0) + payload1)
    request.connection.write(
        stream.create_header(common.OPCODE_TEXT, len(payload2), 1, 0, 0, 0, 0) +
        payload2)
