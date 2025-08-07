# Gemini Context

This file provides context for the Gemini AI code assistant.

## Important Directories

- `src`: Main source code.
- `tests`: E2e tests.
- `lib`: Generated files (do not edit).

## Common Commands

### Fix

To restore the project to a known good state, run the following commands in order:

1.  **Build:** `npm run build`
2.  **Unit Tests:** `npm run unit`
3.  **Format:** `npm run format`
4.  **Verify BidiMapper Import:** `node lib/esm/bidiMapper/BidiMapper.js`. If this
    fails with `ERR_MODULE_NOT_FOUND`, it's likely due to a missing `.js`
    extension in an import statement in one of the TypeScript source files.

Do not commit, pull, or push unless explicitly asked.

### E2E Tests

To run a specific E2E test, use the following command:

`npm run e2e -- -k <test_name>`

### Fixing E2E Tests

To debug a failing E2E test, try running it with different environment variables:

- Set `HEADLESS` to `true`, `false`, or `old`.
- Set `CHROMEDRIVER` to `true` or `false`.

Each run should be done with `VERBOSE=true`. Inspect the latest log in the `logs` directory for errors.

Note: E2E tests are slow, so run only the necessary tests.
