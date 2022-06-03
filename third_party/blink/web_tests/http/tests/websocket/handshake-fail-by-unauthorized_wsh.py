from mod_pywebsocket import handshake


def web_socket_do_extra_handshake(request):
    message = b'HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm="Access to staging site"\r\n\r\n'
    request.connection.write(message)
    # Prevents pywebsocket from sending its own handshake message.
    raise handshake.AbortedByUserException('Abort the connection')


def web_socket_transfer_data(request):
    pass
