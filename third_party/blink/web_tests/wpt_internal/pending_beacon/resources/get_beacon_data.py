def main(request, response):
    """Get the beacon data keyed by uuid."""
    if b'uuid' not in request.GET:
        response.status = 400
        return 'Must provide a UUID to look up beacon data'
    uuid = request.GET.first(b'uuid')

    with request.server.stash.lock:
        data = request.server.stash.take(key=uuid, path='beacondata')
        # data is already of type string.
        return [(b'Content-Type', b'text/plain')], data
