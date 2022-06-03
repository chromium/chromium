import re
from mod_pywebsocket import common
from mod_pywebsocket import stream
from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    match = re.search(r'\?opcode=(\d+)$', request.ws_resource)
    if match is None:
        msgutil.send_message(request,
                             'FAIL: Query value is incorrect or missing')
        return

    opcode = int(match.group(1))
    payload = b'This text should be ignored. (opcode = %d)' % opcode
    request.connection.write(
        stream.create_header(opcode, len(payload), 1, 0, 0, 0, 0) + payload)
