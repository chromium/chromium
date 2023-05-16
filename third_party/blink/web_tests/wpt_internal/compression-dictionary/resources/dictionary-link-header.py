def main(request, response):
    token = request.GET[b"token"]
    cross_origin_path = request.GET.get(b"cross-origin-path", b'')
    link_header_value = '<{}dict.py?token={}>; rel="dictionary"'.format(
        cross_origin_path.decode(), token.decode())
    response.headers.set(b"Link", link_header_value.encode())
    response.headers.set(b"Content-Type", b"text/html")
    # Generate this token with the given commands:
    #   ./tools/origin_trials/generate_token.py \
    #       https://localhost:8444 \
    #       CompressionDictionaryTransport \
    #       --expire-timestamp=2000000000
    response.headers.set(
        b"Origin-Trial",
        b"Axf5ME7tVCxmufN6plYlnRsYj+FbEYqlQMnslKv5wRP73qCVf90tgGLFWmVEY/duYzvJCXc1pUumbPp0JnquAAYAAABveyJvcmlnaW4iOiAiaHR0cHM6Ly93ZWItcGxhdGZvcm0udGVzdDo4NDQ0IiwgImZlYXR1cmUiOiAiQ29tcHJlc3Npb25EaWN0aW9uYXJ5VHJhbnNwb3J0IiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9"
    )

    data = b""
    response.headers.set(b"Content-Length", str(len(data)).encode())
    return (200, [], data)
