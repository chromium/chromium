import six
from six.moves import range

from mod_pywebsocket import common
from mod_pywebsocket import msgutil
from mod_pywebsocket import util


def web_socket_do_extra_handshake(request):
    pass  # Always accept.


def web_socket_transfer_data(request):
    expected_messages = [b'Hello, world!', b'', all_distinct_bytes()]

    for test_number, expected_message in enumerate(expected_messages):
        message = msgutil.receive_message(request)
        if type(message) == six.binary_type and message == expected_message:
            msgutil.send_message(request, 'PASS: Message #%d.' % test_number)
        else:
            msgutil.send_message(
                request, 'FAIL: Message #%d: Received unexpected message: %r' %
                (test_number, message))


def all_distinct_bytes():
    return b''.join([util.pack_byte(i) for i in range(256)])
