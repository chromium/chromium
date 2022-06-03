def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    request.ws_stream._send_pong(b"{payload: 'yes'}")
    request.ws_stream.send_message('sent pong')
    line = request.ws_stream.receive_message()
