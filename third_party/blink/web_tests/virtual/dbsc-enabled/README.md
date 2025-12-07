# Device Bound Session Credentials (DBSC) tests

These are run as virtual tests due to several required args.

## --enable-features=EnableBoundSessionCredentialsSoftwareKeysForManualTesting

DBSC requires hardware such as a TPM to run appropriately. This command line
argument replaces that requirement with a software version so tests don't need
the hardware.

## --disable-features=DeviceBoundSessionsRefreshQuota

DBSC has a refresh quota for production users to prevent sites from refreshing
too often. We don't want tests to have this condition, so they can test
refreshes unconditionally. This argument removes that constraint.

## --enable-features=DeviceBoundSessions

This is a temporary condition until the overall DBSC feature is enabled by
default.