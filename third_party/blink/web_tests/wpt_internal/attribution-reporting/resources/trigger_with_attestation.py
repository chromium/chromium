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


def main(request, response):
    trigger = request.headers.get('Test-Trigger-Header-Value')
    request_data = request.headers.get(
        "Sec-Attribution-Reporting-Private-State-Token")
    try:
        issuance_response = tt.issue_trust_token(issuer=issuer,
                                                 request_data=request_data,
                                                 key_id=0)

        response.headers.set("Attribution-Reporting-Register-Trigger", trigger)
        response.headers.set("Sec-Attribution-Reporting-Private-State-Token",
                             issuance_response.to_string())
        response.status = 200
    except Exception:
        response.status = 500
