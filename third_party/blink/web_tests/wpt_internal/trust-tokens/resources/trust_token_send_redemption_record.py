"""A Python file handler for WPT that handles `send-redemption-record` requests.
"""

import os
import sys

wpt_internal_dir = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if wpt_internal_dir not in sys.path:
    sys.path.insert(0, wpt_internal_dir)


def main(request, response):
    redemption_record = request.headers.get("Sec-Redemption-Record").decode(
        "utf-8")
    if redemption_record:
        response.status = 200
        # Add a response body for the iframe E2E test to read
        response.content = redemption_record
        # Return the redeption response to test the value
        return redemption_record
    else:
        response.status = 400
        # Add a response body for the iframe E2E test to read
        response.content = "Trust token RR failed."
