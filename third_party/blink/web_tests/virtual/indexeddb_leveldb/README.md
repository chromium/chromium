# IndexedDB (legacy) LevelDB backing store

Bug: https://crbug.com/40253999

Chromium is in the process of migrating off of the LevelDB implementation and to
a SQLite-based replacement. This virtual test suite replaces the former virtual
suite that force-enabled the SQLite store, which was called indexeddb_sqlite.

The default in Chromium is currently (still) to use LevelDB, but due to an
experimental entry in testing/variations/fieldtrial_testing_config.json, the
default for WPT is SQLite. Therefore this virtual suite is now necessary to
ensure continued coverage of the LevelDB store during what is expected to be a
protracted rollout of the SQLite store.

This virtual test suite is not set to automatically expire, but can be removed
when the LevelDB store code is eventually deleted.

The functionality is platform-agnostic but the implementation ultimately uses
platform-specific libraries. This suite is currently run only on Windows and
Linux to reduce load on bots while still providing coverage across OS families.
