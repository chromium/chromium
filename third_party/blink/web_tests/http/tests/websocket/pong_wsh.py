from six.moves.urllib import parse
from mod_pywebsocket import common
from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    send_payload = ''
    r = request.ws_resource.split('?', 1)
    if len(r) == 2:
        params = parse.parse_qs(r[1])
        if 'payload' in params:
            send_payload = params['payload'][0]

    msgutil.send_ping(request, send_payload)

    # We need to use an internal function to detect a pong frame from the
    # client.
    opcode, recv_payload, final, reserved1, reserved2, reserved3 = \
        request.ws_stream._receive_frame()
    if (opcode == common.OPCODE_PONG and recv_payload.decode('UTF-8') == send_payload and final and not reserved1 and not reserved2
            and not reserved3):
        msgutil.send_message(request, 'PASS')
    else:
        msgutil.send_message(
            request,
            'FAIL: Received unexpected frame: opcode = %r, payload = %r, '
            'final = %r, reserved1 = %r, reserved2 = %r, reserved3 = %r'
            % (opcode, recv_payload, final, reserved1, reserved2, reserved3))
