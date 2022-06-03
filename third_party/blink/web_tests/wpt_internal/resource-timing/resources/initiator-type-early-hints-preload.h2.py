import os
import time


def handle_headers(frame, request, response):
    link_header = b"<blue-cacheable.png>; rel=preload; as=image"
    early_hints = [
        (b":status", b"103"),
        (b"link", link_header),
    ]
    response.writer.write_raw_header_frame(headers=early_hints,
                                           end_headers=True)

    # Simulate the response generation is taking time.
    time.sleep(0.2)
    response.status = 200
    response.headers[b"content-type"] = "text/html"
    response.headers[b"link"] = link_header
    response.write_status_headers()


def main(request, response):
    current_path = os.path.dirname(os.path.realpath(__file__))
    file_path = os.path.join(current_path,
                             "initiator-type-early-hints-preload.html")
    body = open(file_path, "r").read()
    response.writer.write_data(item=body, last=True)
