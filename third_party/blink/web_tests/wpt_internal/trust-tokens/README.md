# Trust Token Tests

## Table of Contents

1. [Overview](#overview)
1. [General Guidelines](#general-guidelines)
1. [File Descriptions](#file-descriptions)
1. [Running Tests](#running-tests)

## Overview

The trust token WPTs in this directory are JavaScript tests that interact with a Python implementation of a trust token issuer.
[Python file handlers](https://web-platform-tests.org/writing-tests/python-handlers/index.html) implement the server-side logic for trust token issuance and redemption.
The supported issuer protocol is `TrustTokenV3VOPRF`.

Please refer to the [trust token API explainer](https://github.com/WICG/trust-token-api) for details about the API.

## General Guidelines

- These tests are currently internal only. New tests should be added to `wpt_internal/trust-tokens/`. When trust tokens are implemented by other browser vendors, these files can be transitioned to `third_party/blink/web_tests/external/wpt/trust-tokens/`
- All tests should be [JavaScript tests](https://web-platform-tests.org/writing-tests/testharness.html)
- The key commitment and other arguments to the test runner are defined in `third_party/blink/web_tests/VirtualTestSuites`. The contents of the key commitment are as follows:
```json
{
    "https://web-platform.test:8444": {
        "TrustTokenV3VOPRF": {
            "protocol_version": "TrustTokenV3VOPRF",
            "id": 1,
            "batchsize": 1,
            "keys": {
                "0": {
                    "Y": "AAAAAASqh8oivosFN46xxx7zIK10bh07Younm5hZ90HgglQqOFUC8l2/VSlsOlReOHJ2CrfJ6CG1adnTkKJhZ0BtbSPWBwviQtdl64MWJc7sSg9HPvWfTjDigX5ihbzihG8V8aA=",
                    // The timestamp here is equivalent to
                    // Friday, December 31, 9999 11:59:59 PM GMT
                    "expiry": "253402300799000000"
                }
            }
        }
    }
}
```

## File Descriptions

- `resources/hash_to_field.py`
  - Helper module used to implement [hash-to-scalar](https://github.com/WICG/trust-token-api/blob/main/ISSUER_PROTOCOL.md#serializationhashing-1)
- `resources/trust_token_issuance.py`
  - Python file handler for token issuance
  - Generates a valid response including a DLEQ proof, which is verified by Chromium
  - The response is stripped from the `Sec-Private-State-Token` header by the browser and is not accessible to JavaScript
- `resources/trust_token_redemption.py`
  - Python file handler for token redemption
  - The redemption record in the response is an arbitrary byte string, and it is also stripped from the `Sec-Private-State-Token` header by the browser
- `resources/trust_token_send_redemption_record.py`
  - Python file handler for `send-redemption-record` requests
  - Checks for the presence of the `Sec-Redemption-Record` header
- `resources/trust_token_voprf.py`
  - Helper module that implements the server side of the issuer protocol
- `trust-token-api-e2e.https.html`
  - JavaScript WPT that tests the browser's interaction with the issuer
  - Tests `token-request`, `token-redemption`, and `send-redemption-record`

## Running Tests

The WPTs run as [virtual tests](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/VirtualTestSuites). This enables the issuer's key commitment to be passed to Chromium via a command line argument.

```bash
# Build web tests
autoninja -C out/Default blink_tests

# Run a single test
third_party/blink/tools/run_web_tests.py -t Default virtual/trust-tokens/wpt_internal/trust-tokens/trust-token-api-e2e.https.html
```

See the [web tests doc](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_tests.md#running-the-tests) for more details on using the test runner.
