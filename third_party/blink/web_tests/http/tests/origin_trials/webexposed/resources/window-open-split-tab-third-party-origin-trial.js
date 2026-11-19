// This token is for "localhost", which is a different origin from the
// "127.0.0.1" origin used to run all http tests. Generate it with:
//
// clang-format off
// tools/origin_trials/generate_token.py http://localhost:8000 WindowOpenSplitTab --is-third-party --expire-timestamp=2000000000
// clang-format on
const third_party_token =
    'A8xov9NTWQTyhs9RFw99WGCVZsb9qvT6xpiNduG8ivdQY9/HcuqBd7GFDhcHb9212hwqwiDZpgDKk6xKbbovQw8AAABweyJvcmlnaW4iOiAiaHR0cDovL2xvY2FsaG9zdDo4MDAwIiwgImZlYXR1cmUiOiAiV2luZG93T3BlblNwbGl0VGFiIiwgImV4cGlyeSI6IDIwMDAwMDAwMDAsICJpc1RoaXJkUGFydHkiOiB0cnVlfQ==';

const tokenElement = document.createElement('meta');
tokenElement.httpEquiv = 'origin-trial';
tokenElement.content = third_party_token;
document.head.appendChild(tokenElement);
