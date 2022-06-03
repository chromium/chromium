import struct

from mod_pywebsocket import msgutil
from mod_pywebsocket import stream
from mod_pywebsocket import util


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # Wait for a close frame sent from the client.
    close_frame = request.ws_stream.receive_bytes(6)

    msgutil.send_message(request, 'Client should ignore this message')

    # Send only first two bytes of the received frame. The remaining four bytes
    # are "masking key", which changes every time the test runs.

    message = "close_frame[:2]='%s'" % util.hexify(close_frame[:2])
    data = struct.pack('!H', 1000) + message.encode('UTF-8')

    request.connection.write(stream.create_close_frame(data))

    # Tell pywebsocket we have sent a close frame to the client, so it can close
    # the connection.
    request.server_terminated = True
