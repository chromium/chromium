from mod_pywebsocket import common
from mod_pywebsocket import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    payload1 = b'This first text should be received.'
    payload2 = b'This second text '
    payload3 = b'should be received, too.'

    # send ''
    request.connection.write(
        stream.create_header(common.OPCODE_TEXT, 0, 1, 0, 0, 0, 0))

    # send payload1
    request.connection.write(
        stream.create_header(common.OPCODE_TEXT, len(payload1), 1, 0, 0, 0, 0) +
        payload1)

    # send '' + ''
    request.connection.write(
        stream.create_header(common.OPCODE_TEXT, 0, 0, 0, 0, 0, 0))
    request.connection.write(
        stream.create_header(common.OPCODE_CONTINUATION, 0, 1, 0, 0, 0, 0))

    # send payload2 + '' + payload3
    request.connection.write(
        stream.create_header(common.OPCODE_TEXT, len(payload2), 0, 0, 0, 0, 0) +
        payload2)
    request.connection.write(
        stream.create_header(common.OPCODE_CONTINUATION, 0, 0, 0, 0, 0, 0))
    request.connection.write(
        stream.create_header(common.OPCODE_CONTINUATION,
                             len(payload3), 1, 0, 0, 0, 0) + payload3)
