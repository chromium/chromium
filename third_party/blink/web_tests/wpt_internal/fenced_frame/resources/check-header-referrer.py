import os

from wptserve.utils import isomorphic_decode


def main(request, response):
    response.headers.set(b"supports-loading-mode", b"fenced-frame")

    script = u"""
        <script src="utils.js"></script>
        <script>
            writeValueToServer(KEYS["referrer.value"], "%s")
        </script>
    """ % (isomorphic_decode(request.headers.get(b"referer", b"")))
    return (200, [], script)
