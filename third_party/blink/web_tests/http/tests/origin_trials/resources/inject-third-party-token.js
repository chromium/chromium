// Generate this token with the given commands:
// generate_token.py http://localhost:8000 FrobulateThirdParty --version 3 --is-third-party --expire-timestamp=2000000000
const third_party_token = "A2sI42GtBtrjupmY4DvSO3OlTDHgLuhXEy+To7Qm3s9WlW3wI+x/26MeVUQ4iczVMpM+IlbENy/1+BJkGtEzkAYAAABxeyJvcmlnaW4iOiAiaHR0cDovL2xvY2FsaG9zdDo4MDAwIiwgImlzVGhpcmRQYXJ0eSI6IHRydWUsICJmZWF0dXJlIjogIkZyb2J1bGF0ZVRoaXJkUGFydHkiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=";

// This token is for "localhost", which is a different origin from the
// "127.0.0.1" origin used to run all http tests.
const tokenElement = document.createElement('meta');
tokenElement.httpEquiv = 'origin-trial';
tokenElement.content = third_party_token;
document.head.appendChild(tokenElement);
