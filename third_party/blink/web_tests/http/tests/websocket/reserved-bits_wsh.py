import re
from mod_pywebsocket import common
from mod_pywebsocket import stream
from mod_pywebsocket import msgutil

bit = 0


def web_socket_do_extra_handshake(request):
    match = re.search(r'\?compressed=(true|false)&bitNumber=(\d)$',
                      request.ws_resource)
    if match is None:
        msgutil.send_message(request,
                             'FAIL: Query value is incorrect or missing')
        return

    global bit
    compressed = match.group(1)
    bit = int(match.group(2))
    request.ws_extension_processors = []  # using no extension response


def web_socket_transfer_data(request):
    text = b'This message should be ignored.'
    opcode = common.OPCODE_TEXT
    if bit == 1:
        frame = stream.create_header(opcode, len(text), 1, 1, 0, 0, 0) + text
    elif bit == 2:
        frame = stream.create_header(opcode, len(text), 1, 0, 1, 0, 0) + text
    elif bit == 3:
        frame = stream.create_header(opcode, len(text), 1, 0, 0, 1, 0) + text
    else:
        frame = stream.create_text_frame('FAIL: Invalid bit number: %d' % bit)
    request.connection.write(frame)
