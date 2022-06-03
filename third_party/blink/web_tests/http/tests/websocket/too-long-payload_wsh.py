import struct
import time
from mod_pywebsocket import common
from mod_pywebsocket import util


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    length = 0x8000000000000000

    # pywebsocket refuses to send a frame with too long payload.
    # Thus, we need to build a frame manually.
    header = util.pack_byte(0x80 | common.OPCODE_TEXT)  # 0x80 is for "fin" bit.
    header += util.pack_byte(127)
    header += struct.pack('!Q', length)
    request.connection.write(header)

    # Send data indefinitely to simulate a real (broken) server sending a big
    # frame. A client should ignore these bytes and abort the connection.
    while True:
        request.connection.write(b'X' * 4096)
        time.sleep(1)
