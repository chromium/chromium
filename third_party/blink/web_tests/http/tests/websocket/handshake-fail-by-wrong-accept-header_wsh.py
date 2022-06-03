from mod_pywebsocket import handshake


def web_socket_do_extra_handshake(request):
    message = (b'HTTP/1.1 101 Switching Protocols\r\n'
               b'Upgrade: websocket\r\n'
               b'Connection: Upgrade\r\n'
               b'Sec-WebSocket-Accept: XXXXthisiswrongXXXX\r\n'
               b'\r\n')
    request.connection.write(message)
    # Prevents pywebsocket from sending its own handshake message.
    raise handshake.AbortedByUserException('Abort the connection')


def web_socket_transfer_data(request):
    pass
