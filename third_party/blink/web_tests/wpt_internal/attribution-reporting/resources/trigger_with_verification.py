"""A Python file handler for WPT that handles trigger registration that includes
   a blind message to be signed."""

import importlib
import os
import sys

wpt_internal_dir = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if wpt_internal_dir not in sys.path:
    sys.path.insert(0, wpt_internal_dir)

tt = importlib.import_module("trust-tokens.resources.trust_token_voprf")
issuer = tt.create_trust_token_issuer()


# Parse the blind messages from the structured header list received.
def parse_header(request_data):
    return [
        string.strip("\"")
        for string in request_data.decode('utf-8').split(', ')
    ]


# Build the structured header list from the signatures.
def build_header(signatures):
    return ", ".join(["\"" + signature + "\"" for signature in signatures])


def main(request, response):
    trigger = request.headers.get('Test-Trigger-Header-Value')
    request_data = request.headers.get(
        "Sec-Attribution-Reporting-Private-State-Token")
    try:
        blind_signatures = []
        for blind_message in parse_header(request_data):
            issuance_response = tt.issue_trust_token(
                issuer=issuer,
                request_data=blind_message.encode('utf-8'),
                key_id=0)
            blind_signatures.append(issuance_response.to_string())

        response.headers.set("Attribution-Reporting-Register-Trigger", trigger)
        response.headers.set("Sec-Attribution-Reporting-Private-State-Token",
                             build_header(blind_signatures))
        response.status = 200
    except Exception:
        response.status = 500
