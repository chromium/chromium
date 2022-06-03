from six.moves import range

from mod_pywebsocket import common
from mod_pywebsocket import stream
from mod_pywebsocket import util


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # pyformat: disable
    messages_to_send = [[b'Hello, ', b'world!'], [b'', b'Hello, ', b'', b'world!', b''], [b'', b'', b''],
                        [util.pack_byte(i) for i in range(256)]]
    # pyformat: enable
    for message_list in messages_to_send:
        for index, message in enumerate(message_list):
            # FIXME: Should use better API to send binary messages when
            # pywebsocket supports it.
            if index == 0:
                opcode = common.OPCODE_BINARY
            else:
                opcode = common.OPCODE_CONTINUATION
            if index < len(message_list) - 1:
                final = 0
            else:
                final = 1
            header = stream.create_header(opcode,
                                          len(message), final, 0, 0, 0, 0)
            request.connection.write(header + message)
