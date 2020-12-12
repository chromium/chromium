def main(request, response):
    """Send a response with the Origin-Agent-Cluster and Origin-Trial headers, in
    the order specified by the "headerOrder" query parameter, which can be
    either "oiot" or "otoi" (see ../README.md).

    The response will listen for message and messageerror events and echo them
    back to the parent. See external/wpt's
    /origin-isolation/resources/helpers.mjs for how these handlers are used.
    """

    token = "ArQvBL/jhDJ62HaUm/ak0dIUYDjZAfeCQTXwa92cOrHZbL7R+bhb3qrVO2pHWkgJPgvIzvLX5m3wfaUJfOKY0Q4AAABqeyJvcmlnaW4iOiAiaHR0cHM6Ly93d3cud2ViLXBsYXRmb3JtLnRlc3Q6ODQ0NCIsICJmZWF0dXJlIjogIk9yaWdpbklzb2xhdGlvbkhlYWRlciIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ=="

    header_order = request.GET.first(b"headerOrder")
    if header_order == b"otoi":
        response.headers.set(b"Origin-Trial", token)
        response.headers.set(b"Origin-Agent-Cluster", b"?1")
    elif header_order == b"oiot":
        response.headers.set(b"Origin-Agent-Cluster", b"?1")
        response.headers.set(b"Origin-Trial", token)
    else:
        raise AssertionError("Invalid headerOrder")

    response.headers.set(b"Content-Type", b"text/html")

    return """
    <!DOCTYPE html>
    <meta charset="utf-8">
    <title>Helper page for origin isolation tests</title>

    <script type="module">
    window.onmessage = e => {
      if (e.data.constructor === WebAssembly.Module) {
        parent.postMessage("WebAssembly.Module message received", "*");
      } else if (e.data.command === "set document.domain") {
        document.domain = e.data.newDocumentDomain;
        parent.postMessage("document.domain is set", "*");
      }
    };

    window.onmessageerror = () => {
      parent.postMessage("messageerror", "*");
    };
    </script>
    """
