"""A Python file handler for WPT that handles token redemption requests."""

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
    request_data = request.headers.get("Sec-Private-State-Token").decode(
        "utf-8")
    redemption_response = tt.redeem_trust_token(issuer=issuer,
                                                request_data=request_data)

    response.headers.set("Sec-Private-State-Token",
                         redemption_response.to_string())
    response.status = 200
    # Return the redeption response to test the value
    return redemption_response.to_string()
