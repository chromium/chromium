from six.moves.urllib import parse
import time
import threading

lock = threading.Lock()
connections = set()
next_test_id = 0


def web_socket_do_extra_handshake(request):
    query_string = request.ws_resource.split('?', 1)
    if len(query_string) == 1:
        return
    params = parse.parse_qs(query_string[1])
    mode = params['mode'][0]
    if mode == 'new_test':
        new_test(request)
    elif mode == 'do_test':
        do_test(request, params)


def new_test(request):
    """Allocate a unique test id."""
    global lock, next_test_id
    with lock:
        request.response = str(next_test_id)
        next_test_id += 1


def do_test(request, params):
    """Check that no other connection is happening at the same time."""
    global lock, connections
    id = params['id'][0]
    with lock:
        if id in connections:
            request.response = 'FAIL'
            return
        connections.add(id)
    time.sleep(0.05)
    with lock:
        connections.remove(id)
    request.response = 'PASS'


def web_socket_transfer_data(request):
    response = request.response
    request.ws_stream.send_message(response)
