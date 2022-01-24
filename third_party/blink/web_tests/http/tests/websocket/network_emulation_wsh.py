from six.moves.urllib import parse
import six

broadcaster_request = None
listener_request = None


def get_role(request):
    """Look up the "role" query parameter in the URL."""
    query = request.ws_resource.split('?', 1)
    if len(query) == 1:
        return None
    param = parse.parse_qs(query[1])
    if 'role' not in param:
        return None
    return param['role'][0]


def web_socket_do_extra_handshake(request):
    global broadcaster_request, listener_request
    if get_role(request) == 'broadcaster':
        broadcaster_request = request
    elif get_role(request) == 'listener':
        listener_request = request


def web_socket_transfer_data(request):
    global broadcaster_request, listener_request
    while True:
        line = request.ws_stream.receive_message()
        if line is None:
            return
        binary = not isinstance(line, six.text_type)
        broadcaster_request.ws_stream.send_message(line, binary=binary)
        listener_request.ws_stream.send_message(line, binary=binary)
