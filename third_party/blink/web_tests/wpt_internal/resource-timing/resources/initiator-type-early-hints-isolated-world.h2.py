import os
import re
import time

LABELS = [b"main-world", b"isolated-world-fetch", b"isolated-world-image"]


def handle_headers(frame, request, response):
    token = request.GET.first(b"token")
    assert re.fullmatch(rb"[0-9a-f-]+", token)
    early_hints = [(b":status", b"103")]
    for label in LABELS:
        early_hints.append(
            (
                b"link",
                b"<blue-cacheable.png?%s&token=%s>; rel=preload; as=image"
                % (label, token),
            )
        )
    response.writer.write_raw_header_frame(
        headers=early_hints, end_headers=True
    )

    # Simulate the response generation is taking time.
    time.sleep(0.2)
    response.status = 200
    response.headers[b"content-type"] = "text/html"
    # Unlike the Early Hints response, the final response has no Link headers,
    # so that the document doesn't preload the resources itself.
    response.write_status_headers()


def main(request, response):
    current_path = os.path.dirname(os.path.realpath(__file__))
    file_path = os.path.join(
        current_path, "initiator-type-early-hints-isolated-world.html"
    )
    body = open(file_path, "r").read()
    response.writer.write_data(item=body, last=True)
