# IndexedDB SQLite backing store

Bug: https://crbug.com/40253999

This virtual test suite is not set to automatically expire, but the flag should
eventually be enabled by default, at which point the suite can be removed.

The functionality is platform-agnostic but the implementation ultimately uses
platform-specific libraries. This suite is currently run only on Windows and
Linux to reduce load on bots while still providing coverage across OS families.
