# Gemini Context

This file provides context for the Gemini AI code assistant.

## Important Directories

- `src`: Main source code.
- `tests`: E2E tests.
- `lib`: Generated files (do not edit).

## Git Workflow

- Do not commit, pull, or push unless explicitly asked.

## Common Commands

All commands below are intended to be run from `third_party/chromium-bidi` directory unless specified otherwise.

### Fix

To restore the project to a known good state, run the following commands in order:

1. **Build:** `autoninja -C ../../out/Default third_party/chromium-bidi:default third_party/chromium-bidi:webdriver_bidi_unittests third_party/chromium-bidi:webdriver_bidi_e2e_tests`
2. **Unit Tests:** `../../out/Default/bin/run_webdriver_bidi_unittests`
3. **Format:** `git cl format`
4. **Verify BidiMapper Import:** `node ../../out/Default/gen/third_party/chromium-bidi/src/bidiMapper/BidiMapper.js`. If this
   fails with `ERR_MODULE_NOT_FOUND`, it's likely due to a missing `.js` extension in
   an import statement in one of the TypeScript source files.

### Unit Tests

- Unit test source files are located in the `src` directory and have a `.test.ts`
  extension.
- They are co-located with the code they are testing.
- Run unit tests with `../../out/Default/bin/run_webdriver_bidi_unittests`. This command runs the tests
  in `../../out/Default/gen/third_party/chromium-bidi/src/` using the Node.js native test runner.
- Filter specific unit tests: `../../out/Default/bin/run_webdriver_bidi_unittests -- --test-name-pattern="<test_name>"`.
- When adding new tests, create a new `*.test.ts` file in the `src` directory next to
  the file you are testing. The build process will automatically pick it up.
- **Important**: Do not delete the test files you create. They are a part of the
  project.
- Use constants for test data like user context or browsing context IDs to improve
  readability and maintainability.

### E2E Tests

To run a specific E2E test, use the following command:

`../../out/Default/bin/run_webdriver_bidi_e2e_tests -- -k <test_name>`

### Fixing E2E Tests

To debug a failing E2E test, try running it with different environment variables:

- Set `HEADLESS` to `true`, `false`, or `old`.
- Set `CHROMEDRIVER` to `true` or `false`.

Each run should be done with `VERBOSE=true`. Inspect the latest log in the `logs`
directory for errors.

Note: E2E tests are slow, so run only the necessary tests.

### Fixing the build after a new command is added

When a new command is added to the WebDriver BiDi CDDL (for instance after running `tools/update-bidi-types.sh`), run the following steps to fix the build:

1.  **Run `autoninja -C ../../out/Default third_party/chromium-bidi:default`**. This will fail with a
    `Switch is not exhaustive` error.
2.  **Implement the parser for the new command's params:**
    1.  Add the new `parse...` method to the `BidiCommandParameterParser` interface
        in `src/bidiMapper/BidiParser.ts`.
    2.  Implement the new `parse...` method in `src/bidiMapper/BidiNoOpParser.ts`.
    3.  Implement the new `parse...` method in `src/bidiTab/BidiParser.ts`.
    4.  Add the new `parse...` function to the corresponding namespace in
        `src/protocol-parser/protocol-parser.ts`.
3.  **Add the new command to `src/bidiMapper/CommandProcessor.ts`**. Add a new `case`
    for the new command in the `switch` statement. First parse the command parameters
    and then throw an exception.
4.  **Run `autoninja -C ../../out/Default third_party/chromium-bidi:default` and `git cl format`** to verify the fix.
5.  **Do not run e2e tests for this kind of fixes.**
