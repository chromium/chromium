# Shared Binding Tests

We generate multiple versions of JS bindings, e.g. lite, modules, etc., that
need to work in various execution contexts, e.g. modules, service workers, etc.

Tests in shared/ are shared across all bindings versions.

## Sharing tests

`classic-test-helper.js` and `module-test-helper.js` adds the classes
used by the tests to globalThis. This allows the tests code to be used to
test both versions of the bindings.

The tests themselves use the classes exposed by the helper scripts so they are
usable as classic scripts from HTML pages and as imported modules in ES modules.
