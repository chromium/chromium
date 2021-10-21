import os


def main(request, response):
    response.headers.set(b"supports-loading-mode", b"fenced-frame")

    sec_fetch_dest_value_key = "00000000-0000-0000-0000-000000000012"
    script = u"""
        <script src="/wpt_internal/fenced_frame/resources/utils.js"></script>
        <script>
            writeValueToServer("%s", "%s")
        </script>
    """ % (sec_fetch_dest_value_key,
           request.headers.get(b"sec-fetch-dest", b"none"))
    return (200, [], script)
