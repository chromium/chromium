This suite runs tests for the interaction between WebSockets and BFCache with `--disable-blink-features=DisconnectWebSocketOnBFCache` to ensure the legacy behavior is covered.

This can be removed once the feature is fully shipped and the flag is removed.

See crbug.com/467838624.
