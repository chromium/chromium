import json
import time
from wptserve.utils import isomorphic_decode


def load_previous_header_from_stash(request, key, timeout):
    t0 = time.time()
    while time.time() - t0 < timeout:
        time.sleep(0.5)
        with request.server.stash.lock:
            data = request.server.stash.take(key)
            if data is not None:
                return data
    return None


def main(request, response):
    key = request.GET.get(b"token")
    previous_header = request.GET.get(b"previous_header")
    data = b""
    if previous_header:
        # Wait for 2 seconds for the browser to send the dictionary load request
        data = load_previous_header_from_stash(request, key, 2)
        if data is None:
            data = b'{"status": "FAIL"}'
    else:
        request_header = dict()
        for request_header_key in request.headers:
            request_header[isomorphic_decode(request_header_key)] = \
              isomorphic_decode(request.headers.get(request_header_key))
        data = json.dumps({
            'status': 'OK',
            'request_header': request_header
        }).encode()
        with request.server.stash.lock:
            request.server.stash.put(key, data)
    return (200, [], data)
