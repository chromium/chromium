# This is a workaround for lack of preflight support in the test server.
def main(request, response):
    return (200, [], b"")