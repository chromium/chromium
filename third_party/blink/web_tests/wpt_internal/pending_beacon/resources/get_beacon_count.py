# Gets the count of beacons the server has seen.
def main(request, response):
    if b"uuid" in request.GET:
        uuid = request.GET.first(b"uuid")
    else:
        response.status = 400
        return "Must provide a UUID for beacon count handlers"

    count = request.server.stash.take(key=uuid, path="beaconcount")
    if count is None:
        count = 0
    # The stash is read-once/write-once, so after reading the count, it has to be
    # put back.
    request.server.stash.put(key=uuid, value=count, path="beaconcount")
    return [(b"Content-Type", b"text/plain")], str(count)
