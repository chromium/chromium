"""A page that echoes the Referrer header value via BroadcastChannel.
"""


def main(request, response):
    referrer = request.headers.get(b"referer")

    if referrer is None:
        referrer = b"(none)"

    html = u'''
<html>
<head>
<title>Echo referrer</title>
</head>
<body>
<script>
const bc = new BroadcastChannel('prerender-channel');
bc.postMessage({referrer: '%s'});
</script>
</body>
</html>
'''
    return (200, [("Content-Type", b"text/html")],
            html % referrer.decode("utf-8"))
