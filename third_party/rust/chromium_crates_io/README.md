GN rules for all third-party Rust crates are found in these directories:

* `safe`: Crates that are "rule-of-two safe", they can be used in any process.
* `sandbox`: Crates that may expose Undefined Behaviour, they must be used inside a sandboxed process such as the renderer or a utility process.
* `test`: Crates that are approved for use in tests, they can only be used from `testonly` GN targets.
