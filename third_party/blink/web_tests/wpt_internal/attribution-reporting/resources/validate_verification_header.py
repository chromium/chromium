"""A Python file handler for WPT that validates that a token can be redeemed."""

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
    try:
        request_data = request.headers.get(
            "Test-Sec-Attribution-Reporting-Private-State-Token")
        tt.redeem_trust_token(issuer=issuer, request_data=request_data)
        response.status = 200
    except Exception:
        response.status = 500
