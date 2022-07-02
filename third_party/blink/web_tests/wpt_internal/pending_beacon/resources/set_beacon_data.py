import re


def main(request, response):
    """Store the given beacon's data keyed by uuid in the server.

  For GET request, this handler assumes everything comes after 'data=' is part
  of the data query.
    """
    if b'uuid' not in request.GET:
        response.status = 400
        return 'Must provide a UUID to store beacon data'
    uuid = request.GET.first(b'uuid')
    # We want raw text input for 'data' from url instead of byte-encoded one.
    data = re.search(r'data=(.+)$', request.url_parts.query).groups()[0]

    with request.server.stash.lock:
        request.server.stash.put(key=uuid, value=data, path='beacondata')
    return ((200, "OK"), [], "")
