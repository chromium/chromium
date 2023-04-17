"""Test reporting origin server used for two reasons:

  1. It is a workaround for lack of preflight support in the test server.
  2. Allows to stash source IDs and poll to know when a source registration
     has completed.
"""

from wptserve.stash import Stash
import json

# Key used to access the source IDs in the stash.
SOURCES = "9250f93f-2c05-4aae-83b9-2817b0e18b4c"

REQUESTS = "9250f93f-2c05-4aae-83b9-2817b0e18b4d"


def store_source(stash: Stash, source_id: str) -> None:
    """Stores the source id in the stash, indicating that it's been processed."""
    with stash.lock:
        sources_set = stash.take(SOURCES)
        if not sources_set:
            sources_set = {source_id}
        else:
            sources_set.add(source_id)
        stash.put(SOURCES, sources_set)
    return None


def has_source(stash: Stash, source_id: str) -> bool:
    """Looks into the stash and returns True if the requested source id was
     previously registered. Returns False otherwise."""
    with stash.lock:
        sources_set = stash.take(SOURCES)
        if not sources_set:
            return False
        if source_id not in sources_set:
            return False

        sources_set.remove(source_id)
        stash.put(SOURCES, sources_set)
    return True


headers = [
    b"attribution-reporting-eligible",
    b"attribution-reporting-support",
    b"referer",
]


def store_request(request) -> None:
    obj = {
        "method": request.method,
        "url": request.url,
    }
    for header in headers:
        value = request.headers.get(header)
        if value is not None:
            obj[str(header, "utf-8")] = str(value, "utf-8")
    with request.server.stash.lock:
        requests = request.server.stash.take(REQUESTS)
        if not requests:
            requests = []
        requests.append(obj)
        request.server.stash.put(REQUESTS, requests)
    return None


def get_requests(request) -> str:
    with request.server.stash.lock:
        return json.dumps(request.server.stash.take(REQUESTS))


def main(request, response):
    """
    For most requests, simply returns a 200. Actual source/trigger registration
    headers are piped using the `pipe` query param.

    If a `store-source-id` param is set, it returns a redirection to itself with
    a `redirect-store-source-id` param set. Upon receiving the redirection call,
    it stores the source id in a stash indicating that the source which was
    registered in the first request is now complete.

    If a `check-source-id` param is set, it will look for the source id in the
    stash. If it is not available it will return a 404. Otherwise, it returns a
    200.

    If a `clear-stash` param is set, it will clear the stash.
    """
    if request.GET.get(b"clear-stash"):
        request.stash.take(SOURCES)
        return

    # We dont want to redirect preflight requests. The cors headers are piped
    # so we can simply return a 200 and redirect the following request
    if request.method == "OPTIONS":
        response.status = 200
        return

    if request.GET.get(b"get-requests"):
        return get_requests(request)

    if request.GET.get(b"store-request"):
        store_request(request)
        return ""

    source_id = request.GET.get(b"store-source-id")
    if source_id:
        location = (request.url_parts.path + "?redirect-store-source-id=" +
                    str(source_id, 'utf-8'))

        # We do not want to forward all piped headers as this might result in
        # registering the source multiple times. However, if there are cors
        # headers we want to forward them.
        pipe = str(request.GET.get(b"pipe"), 'utf-8').split("|")
        cors_headers = list(filter(lambda x: "Access-Control-Allow" in x,
                                   pipe))
        if len(cors_headers):
            location += "&pipe=" + "|".join(cors_headers)

        response.status = 302
        response.headers.set(b"Location", location)
        return

    redirect_store_source_id = request.GET.get(b"redirect-store-source-id")
    if redirect_store_source_id:
        store_source(request.server.stash, redirect_store_source_id)
        response.status = 200
        return

    check_source_id = request.GET.get(b"check-source-id")
    if check_source_id:
        if has_source(request.server.stash, check_source_id):
            response.status = 200
        else:
            response.status = 404
        return

    response.status = 200
