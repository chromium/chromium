# Test suite for OpaqueResponseBlockingV02 (ORB v0.2)

Since this feature is Fetch-related, this suite tests all WPT fetch tests
with `--enable-features=OpaqueResponseBlockingV01,OpaqueResponseBlockingV02`.
Tests which are expected to behave differently for ORB v0.1 have separate
expectations.
