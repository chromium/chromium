# Device Bound Session Credentials (DBSC) tests

These are run as virtual tests due to several required args.

## --enable-features=EnableBoundSessionCredentialsSoftwareKeysForManualTesting

DBSC requires hardware such as a TPM to run appropriately. This command line
argument replaces that requirement with a software version so tests don't need
the hardware.

## --enable-features=DeviceBoundSessions

This is a temporary condition until the overall DBSC feature is enabled by
default.