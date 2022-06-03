import time
import json


def main(request, response):
    op = request.GET.first(b"op")
    id = request.GET.first(b"id")
    timeout = 1.0

    if op == b"retrieve":
        t0 = time.time()
        while time.time() - t0 < timeout:
            time.sleep(0.1)
            with request.server.stash.lock:
                value = request.server.stash.take(key=id)
            if value is not None:
                return [("Content-Type", "application/json")], value
        return [("Content-Type", "application/json")], json.dumps({'error': 'No such report.', 'id': id})

    # save report
    with request.server.stash.lock:
        request.server.stash.take(key=id)
        request.server.stash.put(key=id, value=json.dumps({'url': request.url}))

    # return acknowledgement report
    return [("Content-Type", "text/plain")], b"Recorded report " + id
