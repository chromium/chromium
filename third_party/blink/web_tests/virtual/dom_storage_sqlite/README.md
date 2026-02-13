# DOM Storage SQLite

Bug: https://crbug.com/377242771

This virtual test suite is set to expire in six months, but the flag should
eventually be enabled by default, at which point the suite can be removed.

The functionality is platform-agnostic but the implementation ultimately uses
platform-specific libraries. This suite is currently run only on Windows and
Linux to reduce load on bots while still providing coverage across OS families.