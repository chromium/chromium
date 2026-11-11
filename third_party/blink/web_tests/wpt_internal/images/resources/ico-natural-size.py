import struct
import time
from pathlib import Path


FILES = {
    b"mixed-aspect": "rust-ico-mixed-aspect-4x2.ico",
    b"reported-thin": "rust-ico-mixed-aspect-256x2.ico",
    b"reported-wide": "rust-ico-mixed-aspect-256x64.ico",
    b"multi-resolution": "2entries.ico",
    b"full-canvas": "rust-ico-full-canvas.ico",
}
DIRECTORY_HEADER = struct.Struct("<HHH")
DIRECTORY_ENTRY = struct.Struct("<BBBBHHII")


def main(request, response):
    key = request.GET.first(b"token", None)
    if request.GET.first(b"action", None) == b"release":
        if key is None:
            return 400, [], b"Missing stream token"
        request.server.stash.put(key, True)
        return 200, [], b"Released"

    image = request.GET.first(b"image", None)
    if image not in FILES:
        return 400, [], b"Unknown ICO fixture"
    data = (
        Path(__file__).resolve().parents[3]
        / "images"
        / "resources"
        / FILES[image]
    ).read_bytes()
    headers = [
        (b"Content-Type", b"image/x-icon"),
        (b"Cache-Control", b"no-store"),
        (b"X-Content-Type-Options", b"nosniff"),
        (b"Content-Length", str(len(data)).encode()),
    ]
    if b"stream" not in request.GET:
        return 200, headers, data
    if key is None:
        return 400, [], b"Missing stream token"

    assert DIRECTORY_HEADER.unpack_from(data)[2] == 2
    partial_end = DIRECTORY_ENTRY.unpack_from(
        data, DIRECTORY_HEADER.size + DIRECTORY_ENTRY.size
    )[-1]
    response.status = 200
    response.headers.update(headers)
    response.write_status_headers()
    if not response.writer.write_content(data[:partial_end]):
        raise ConnectionError("Failed to send the partial ICO response")
    deadline = time.monotonic() + 10
    while not request.server.stash.take(key):
        if time.monotonic() >= deadline:
            raise TimeoutError("The ICO test did not release the response")
        time.sleep(0.01)
    if not response.writer.write_content(data[partial_end:]):
        raise ConnectionError("Failed to complete the ICO response")
