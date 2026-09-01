> [!NOTE]
> If you are viewing this on GitHub, the source was moved to [Chromium](https://chromium.googlesource.com/chromium/src/+/main/third_party/chromium-bidi). Please contribute there.

# WebDriver BiDi for Chromium [![chromium-bidi on npm](https://img.shields.io/npm/v/chromium-bidi)](https://www.npmjs.com/package/chromium-bidi)

This is an implementation of the
[WebDriver BiDi](https://w3c.github.io/webdriver-bidi/) protocol with some
extensions (**BiDi+**)
for Chromium, implemented as a JavaScript layer translating between BiDi and CDP,
running inside a Chrome tab.

Current status can be checked
at [WPT WebDriver BiDi status](https://wpt.fyi/results/webdriver/tests/bidi).

## Performance Benchmarks

The project continuously monitors the performance and overhead of the WebDriver BiDi implementation.

- **Dashboard:** [Chromium-BiDi Performance Benchmarks](https://googlechromelabs.github.io/chromium-bidi/bench/)
- **Details:** Refer to [docs/benchmark.md](docs/benchmark.md) for detailed information about the benchmarking infrastructure, methodology, and statistical analysis.

Note that performance data can be sensitive to CI environment fluctuations, especially on macOS.

## BiDi+

**"BiDi+"** is an extension of the WebDriver BiDi protocol. In addition to [WebDriver BiDi](https://w3c.github.io/webdriver-bidi/) it has:

### Command `goog:cdp.sendCommand`

```cddl
CdpSendCommandCommand = {
  method: "goog:cdp.sendCommand",
  params: CdpSendCommandParameters,
}

CdpSendCommandParameters = {
   method: text,
   params: any,
   session?: text,
}

CdpSendCommandResult = {
   result: any,
   session: text,
}
```

The command runs the
described [CDP command](https://chromedevtools.github.io/devtools-protocol)
and returns the result.

### Command `goog:cdp.getSession`

```cddl
CdpGetSessionCommand = {
   method: "goog:cdp.getSession",
   params: CdpGetSessionParameters,
}

CdpGetSessionParameters = {
   context: BrowsingContext,
}

CdpGetSessionResult = {
   session: text,
}
```

The command returns the default CDP session for the selected browsing context.

### Command `goog:cdp.resolveRealm`

```cddl
CdpResolveRealmCommand = {
   method: "goog:cdp.resolveRealm",
   params: CdpResolveRealmParameters,
}

CdpResolveRealmParameters = {
   realm: Script.Realm,
}

CdpResolveRealmResult = {
   executionContextId: text,
}
```

The command returns resolves a BiDi realm to its CDP execution context ID.

### Events `goog:cdp`

```cddl
CdpEventReceivedEvent = {
   method: "goog:cdp.<CDP Event Name>",
   params: CdpEventReceivedParameters,
}

CdpEventReceivedParameters = {
   event: text,
   params: any,
   session: text,
}
```

The event contains a CDP event.

### Field `goog:channel`

Each command can be extended with a `goog:channel`:

```cddl
Command = {
   id: js-uint,
   "goog:channel"?: text,
   CommandData,
   Extensible,
}
```

If provided and non-empty string, the very same `goog:channel` is added to the response:

```cddl
CommandResponse = {
   id: js-uint,
   "goog:channel"?: text,
   result: ResultData,
   Extensible,
}

ErrorResponse = {
  id: js-uint / null,
  "goog:channel"?: text,
  error: ErrorCode,
  message: text,
  ?stacktrace: text,
  Extensible
}
```

When client uses
commands [`session.subscribe`](https://w3c.github.io/webdriver-bidi/#command-session-subscribe)
and [`session.unsubscribe`](https://w3c.github.io/webdriver-bidi/#command-session-unsubscribe)
with `goog:channel`, the subscriptions are handled per channel, and the corresponding
`goog:channel` filed is added to the event message:

```cddl
Event = {
  "goog:channel"?: text,
  EventData,
  Extensible,
}
```

## Dev Setup

All commands below are intended to be run from the `third_party/chromium-bidi` directory unless specified otherwise.

### Build Setup

The project uses Chromium build toolchains (`gn` and `ninja`/`autoninja`) for compiling TypeScript and bundling.

1. Fetch the toolchains and sync dependencies using `gclient` (requires `depot_tools` installed and in your PATH):
   ```sh
   gclient sync
   ```
2. Generate the Ninja build configuration:
   ```sh
   gn gen --root=../.. ../../out/Default
   ```
3. Build the project:
   ```sh
   autoninja -C ../../out/Default third_party/chromium-bidi:default
   ```
   To build the test runner targets as well:
   ```sh
   autoninja -C ../../out/Default third_party/chromium-bidi:webdriver_bidi_unittests third_party/chromium-bidi:webdriver_bidi_e2e_tests
   ```

### Code Formatting & Linting

We use a suite of tools to format and lint the codebase:

- [keep-sorted](https://github.com/google/keep-sorted) to automatically sort lists, imports, and keys.
- [ESLint](https://eslint.org/) to lint JavaScript and TypeScript files.
- [Prettier](https://prettier.io/) to format JavaScript, TypeScript, JSON, and Markdown files.
- [Ruff](https://docs.astral.sh/ruff/) to lint and format Python files.

#### Running Presubmit Checks

Presubmit checks run automatically before upload/commit and can be executed manually via:

```sh
git cl presubmit
```

#### Auto-formatting Code

> [!NOTE]
> `git cl format` formats Python and C++ files, but does **not** format TypeScript/JavaScript files. Use **Prettier** and **ESLint** for TypeScript and JavaScript code.

To auto-format and lint files:

- **JavaScript / TypeScript / JSON / Markdown (Prettier):**
  ```sh
  ./tools/node.py node_modules/prettier/bin/prettier.cjs --cache --write .
  ```
- **JavaScript / TypeScript (ESLint auto-fix):**
  ```sh
  ./tools/node.py node_modules/eslint/bin/eslint.js --cache --fix .
  ```
- **Python (Ruff / git cl format):**
  ```sh
  ruff check --fix . && ruff format .
  ```
  (or via `git cl format --python`)
- **keep-sorted:**
  ```sh
  find src tests docs examples -type f | xargs keep-sorted --mode=fix
  ```
  (or in git: `git ls-files | xargs keep-sorted --mode=fix`)

### Starting WebDriver BiDi Server

First, build the target:

```sh
autoninja -C ../../out/Default third_party/chromium-bidi:default
```

Run the server:

```sh
./tools/node.py tools/run-bidi-server.mjs --gen-dir ../../out/Default/gen/third_party/chromium-bidi
```

By default, the server runs on port `8080`. Use the `PORT=` environment variable or `--port=` argument to run it on another port:

```sh
PORT=8081 ./tools/node.py tools/run-bidi-server.mjs --gen-dir ../../out/Default/gen/third_party/chromium-bidi
./tools/node.py tools/run-bidi-server.mjs --gen-dir ../../out/Default/gen/third_party/chromium-bidi --port=8081
```

Use the `DEBUG` environment variable to see debug info:

```sh
DEBUG=* ./tools/node.py tools/run-bidi-server.mjs --gen-dir ../../out/Default/gen/third_party/chromium-bidi
```

Use the `DEBUG_DEPTH` (default: `10`) environment variable to see debug deeply nested objects:

```sh
DEBUG_DEPTH=100 DEBUG=* ./tools/node.py tools/run-bidi-server.mjs --gen-dir ../../out/Default/gen/third_party/chromium-bidi
```

Use the `CHANNEL=...` environment variable with one of the following values to run
the specific Chrome channel: `stable`, `beta`, `canary`, `dev`, `local`. Default is
`local`. The `local` channel means the pinned in `.browser` Chrome version will be
downloaded if it is not yet in cache. Otherwise, the requested Chrome version should
be installed.

```sh
CHANNEL=dev ./tools/node.py tools/run-bidi-server.mjs --gen-dir ../../out/Default/gen/third_party/chromium-bidi
```

Use the CLI argument `--verbose` to have CDP events printed to the console. Note: you have to enable debugging output `bidi:mapper:debug:*` as well.

```sh
DEBUG=bidi:mapper:debug:* ./tools/node.py tools/run-bidi-server.mjs --gen-dir ../../out/Default/gen/third_party/chromium-bidi --verbose
```

or

```sh
DEBUG=* ./tools/node.py tools/run-bidi-server.mjs --gen-dir ../../out/Default/gen/third_party/chromium-bidi --verbose
```

To run the browser in headful mode:

```sh
./tools/node.py tools/run-bidi-server.mjs --gen-dir ../../out/Default/gen/third_party/chromium-bidi --port=8081 --headless=false
```

## Running

Testing in Chromium uses GN `script_test` targets:

- `third_party/chromium-bidi:webdriver_bidi_unittests`
- `third_party/chromium-bidi:webdriver_bidi_e2e_tests`

When built, these targets produce executable runner wrappers in the build directory (`out/Default/bin/`).

### Unit tests

First, build the unit test target:

```sh
autoninja -C ../../out/Default third_party/chromium-bidi:webdriver_bidi_unittests
```

Run all unit tests:

```sh
../../out/Default/bin/run_webdriver_bidi_unittests
```

Filter unit tests by test name:

```sh
../../out/Default/bin/run_webdriver_bidi_unittests -- --test-name-pattern="<test_name>"
```

Filter unit tests by file path:

```sh
../../out/Default/bin/run_webdriver_bidi_unittests -- --test-path-pattern="<path_pattern>"
```

Filter unit tests using ResultDB / Chromium test filter (`--test-filter` / `--isolated-script-test-filter` / `--gtest_filter`):

```sh
../../out/Default/bin/run_webdriver_bidi_unittests --test-filter=':chromium-bidi!mocha:src/utils/:assert.test.ts#assert:should not throw an error when the predicate is truthy'
```

Multiple tests can be separated with `::` or `:`:

```sh
../../out/Default/bin/run_webdriver_bidi_unittests --test-filter=':chromium-bidi!mocha:src/utils/:assert.test.ts#assert:should not throw an error when the predicate is truthy:::chromium-bidi!mocha:src/utils/:DefaultMap.test.ts#DefaultMap:sets and gets properly'
```

Or using filter files (`--test-filter-file` / `--isolated-script-test-filter-file`):

```sh
../../out/Default/bin/run_webdriver_bidi_unittests --test-filter-file=path/to/filter_file.txt
```

> [!NOTE]
> When running in `zsh` or other shells that interpret `!`, `#`, or `[`/`]`, make sure to wrap the filter argument in single quotes `'...'` to prevent history expansion (`event not found`) or glob expansion (`no matches found`).
>
> When running from the Chromium repository root (`src/`), use `out/Default/bin/run_webdriver_bidi_unittests`.

### E2E tests

The e2e tests serve the following purposes:

1. Brief checks of the scenarios (the detailed check is done in WPT)
2. Test Chromium-specific behavior nuances
3. Add a simple setup for engaging the specific command

The E2E tests are written using Python (`pytest`), in order to more-or-less align with the web-platform-tests.
Python dependencies are managed automatically via `vpython3` (part of `depot_tools`).

#### Running

First, build the e2e test target:

```sh
autoninja -C ../../out/Default third_party/chromium-bidi:webdriver_bidi_e2e_tests
```

The E2E tests automatically start and connect to the BiDi server.

Run all E2E tests:

```sh
../../out/Default/bin/run_webdriver_bidi_e2e_tests
```

Filter E2E tests using ResultDB / Chromium test filter (`--test-filter` / `--isolated-script-test-filter` / `--gtest_filter`):

```sh
../../out/Default/bin/run_webdriver_bidi_e2e_tests --test-filter=':chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_add_same_characteristic_uuid_twice'
```

Legacy pytest node IDs, wildcards (`*`), and multiple `::` or `:` separated test IDs are also supported:

```sh
../../out/Default/bin/run_webdriver_bidi_e2e_tests --isolated-script-test-filter='tests/bluetooth/test_characteristic_emulation.py::test_bluetooth_add_same_characteristic_uuid_twice::tests/browser/test_create_user_context.py::test_browser_create_user_context_proxy[True]'
```

Or using filter files (`--test-filter-file` / `--isolated-script-test-filter-file`):

```sh
../../out/Default/bin/run_webdriver_bidi_e2e_tests --test-filter-file=path/to/filter_file.txt
```

> [!NOTE]
> When running in `zsh` or other shells that interpret `!`, `#`, or `[`/`]`, make sure to wrap the filter argument in single quotes `'...'` to prevent history expansion (`event not found`) or glob expansion (`no matches found`).
>
> When running from the Chromium repository root (`src/`), use `out/Default/bin/run_webdriver_bidi_e2e_tests`.

Additionally the output is recorded under `./logs/<DATE>.e2e.log`, which will contain
both the PyTest logs and in the event of `FAILED` test all the Chromium-BiDi logs.

If you need to see the logs for all tests run the command with `VERBOSE=true`:

```sh
VERBOSE=true ../../out/Default/bin/run_webdriver_bidi_e2e_tests
```

Pass a test file path to run only the selected file:

```sh
../../out/Default/bin/run_webdriver_bidi_e2e_tests -- tests/<PathOrFile>
```

Run a specific test using the `-k` filter:

```sh
../../out/Default/bin/run_webdriver_bidi_e2e_tests -- -k <TestName>
```

Use `CHROMEDRIVER` environment variable to run tests in `chromedriver` instead of NodeJS runner:

```shell
CHROMEDRIVER=true ../../out/Default/bin/run_webdriver_bidi_e2e_tests
```

Use the `PORT` environment variable to connect to another port:

```sh
PORT=8081 ../../out/Default/bin/run_webdriver_bidi_e2e_tests
```

Use `HEADLESS` to run the tests in headless (new or old) or headful modes.
Values: `true`, `old`, `false`, default: `true`.

```sh
HEADLESS=true ../../out/Default/bin/run_webdriver_bidi_e2e_tests
```

> [!NOTE]
> When running from the Chromium repository root (`src/`), use `out/Default/bin/run_webdriver_bidi_e2e_tests`.

#### Updating snapshots

```sh
../../out/Default/bin/run_webdriver_bidi_e2e_tests -- --snapshot-update true
```

See https://github.com/tophat/syrupy for more information.

### Local http server

E2E tests use local http
server [`pytest-httpserver`](https://pytest-httpserver.readthedocs.io/), which is run
automatically with the tests. However,
sometimes it is useful to run the http server outside the test
case, for example for manual debugging. This can be done by running:

```sh
vpython3 -vpython-spec .vpython3 tools/run_local_http_server.py
```

### Examples

Refer to [examples/README.md](examples/README.md).

## WPT (Web Platform Tests)

WPT tests for WebDriver BiDi are located in Chromium under `third_party/blink/web_tests/external/wpt/webdriver/tests/bidi/`.

First, build the WPT target:

```sh
autoninja -C ../../out/Default headless_shell_wpt
```

To run all BiDi WPT tests in Chromium:

```sh
../../third_party/blink/tools/run_wpt_tests.py -t Default --no-manifest-update external/wpt/webdriver/tests/bidi/
```

To run a specific test:

```sh
../../third_party/blink/tools/run_wpt_tests.py -t Default --no-manifest-update external/wpt/webdriver/tests/bidi/session/status/status.py
```

## How does it work?

The architecture is described in the
[WebDriver BiDi in Chrome Context implementation plan](https://docs.google.com/document/d/1VfQ9tv0wPSnb5TI-MOobjoQ5CXLnJJx9F_PxOMQc8kY)
.

There are 2 main modules:

1. backend WS server in `src`. It runs webSocket server, and for each ws connection
   runs an instance of browser with BiDi Mapper.
2. front-end BiDi Mapper in `src/bidiMapper`. Gets BiDi commands from the backend,
   and map them to CDP commands.

## Contributing

The `chromium-bidi` source code lives in the Chromium repository under `third_party/chromium-bidi`.
Contributions should follow the [Chromium Contributing Guide](https://chromium.googlesource.com/chromium/src/+/main/docs/contributing.md).

The BiDi commands are processed in `src/bidiMapper/CommandProcessor.ts`. To add a
new command, add it to `_processCommand`, write and call the module processor for it.

### Updating Node dependencies

> [!NOTE]
> This does not work on Cog workspaces.

1. Check and bump dependencies:
   - Check outdated: `npm outdated`
   - Bulk upgrade `package.json` to latest: `npx npm-check-updates -u && npm install --ignore-scripts`
   - Upgrade specific package: `npm install --ignore-scripts <package>@latest`
   - Or update within semver ranges: `npm update --ignore-scripts`
2. Build and run tests to ensure dependencies work properly:
   ```sh
   autoninja -C ../../out/Default third_party/chromium-bidi:default third_party/chromium-bidi:webdriver_bidi_unittests third_party/chromium-bidi:webdriver_bidi_e2e_tests
   ../../out/Default/bin/run_webdriver_bidi_unittests
   ../../out/Default/bin/run_webdriver_bidi_e2e_tests
   ```
3. If production dependencies (in `dependencies` of `package.json`) were added or updated, update `README.chromium` and third-party license notices:
   ```sh
   ./tools/append_notices.py
   ```
4. Upload the filtered `node_modules` to Google Cloud Storage and update `DEPS`:
   ```sh
   ./tools/update_node_modules.mjs --force
   ```
5. Upload a CL with `package.json`, `package-lock.json`, `DEPS`, and any updated `README.chromium` / `licenses/` via `git cl upload` and submit for review.

### Publish new `npm` release

TODO(crbug.com/540164671): describe the process.

### Syncing from Chromium to GitHub

[Chromium (`third_party/chromium-bidi`)](https://chromium.googlesource.com/chromium/src/+/main/third_party/chromium-bidi) is the source of truth, and changes are synced out to the GitHub mirror at [GoogleChromeLabs/chromium-bidi](https://github.com/GoogleChromeLabs/chromium-bidi) using [Copybara](https://goto.google.com/copybara).

The configuration file is located at [`third_party/chromium-bidi/copy.bara.sky`](copy.bara.sky).

TODO(crbug.com/549520316): Automate the sync process.

#### Running Copybara manually

> [!NOTE]
> The Copybara sync takes ~10 minutes to run as it iteratively processes commits from Chromium history.

1. **Prerequisites:**
   - Set up the `copybara` CLI alias (see [go/copybara-setup](https://goto.google.com/copybara-setup)).
   - Ensure your credentials for pushing to GitHub are configured via SSH.

2. **Launch directory:**
   Run Copybara from the **root of the Chromium repository** (`src/`):

   ```sh
   cd /path/to/chromium/src
   ```

3. **Sync:**
   ```sh
   copybara third_party/chromium-bidi/copy.bara.sky default
   ```

## Update CDDL types

### Prerequisites

- **cddlconv**: We use [cddlconv](https://github.com/google/cddlconv) to generate our WebDriverBiDi types.
  1. Install [Rust](https://rustup.rs/).
  2. Run `cargo install cddlconv@0.1.10`
- **parse5**: [parse5](https://github.com/inikulin/parse5) is required by the `webdriver-bidi` specification repository to extract CDDL definitions from specifications.
  1. Run `npm install -g parse5`

Run the following steps from the `third_party/chromium-bidi` directory:

1. (Optional) If you want to add a new specification, add it to the `tools/update-bidi-types.sh` script.
2. Run the `tools/update-bidi-types.sh` script.
3. Build the project (`autoninja -C ../../out/Default third_party/chromium-bidi:default`). If a new WebDriver BiDi command was added, compilation will fail with `Switch is not exhaustive. Cases not matched ...`.
4. Add the new BiDi command to `CommandProcessor.#processCommand` in `src/bidiMapper/CommandProcessor.ts`. For now, just have it throw an UnknownErrorException.

```typescript
case '{NEW_COMMAND_NAME}':
  throw new UnknownErrorException(
    `Method ${command.method} is not implemented.`,
  );
```

5. Upload a CL and have it reviewed and landed via Gerrit.

## Adding new command

Want to add a shiny new command to WebDriver BiDi for Chromium? Here's the playbook:

### Prerequisites

#### Specification

The WebDriver BiDi [module](https://w3c.github.io/webdriver-bidi/#protocol-modules), [command](https://w3c.github.io/webdriver-bidi/#commands), or [event](https://w3c.github.io/webdriver-bidi/#events) must be specified either in the [WebDriver BiDi specification](https://w3c.github.io/webdriver-bidi) or as an extension in a separate specification (e.g., the [Permissions specification](https://www.w3.org/TR/permissions/#automation-webdriver-bidi)). The specification should include the command's type definitions in valid [CDDL](https://datatracker.ietf.org/doc/html/rfc8610) format.

#### WPT wdspec tests

You'll need tests to prove your command works as expected. These tests should be written using [WPT wdspec](https://web-platform-tests.org/writing-tests/wdspec.html) and submitted to Chromium under `third_party/blink/web_tests/external/wpt/webdriver/tests/bidi/` along with the spec itself.

#### CDP implementation

Make sure Chromium already has the CDP methods your command will rely on.

### Update CDDL types

Follow the steps in [Update CDDL types](#update-cddl-types) to update the protocol types before implementing the command.

### Implement the new command

`CommandProcessor.#processCommand` in `src/bidiMapper/CommandProcessor.ts` handles parsing parameters and running your command.

#### (only if the new command has non-empty parameters) parse command parameters

If your command has parameters, update the `BidiCommandParameterParser` interface in `src/bidiMapper/BidiParser.ts` and implement the parsing logic in `src/bidiMapper/BidiNoOpParser.ts`, `src/bidiTab/BidiParser.ts`, and `src/protocol-parser/protocol-parser.ts`.

#### Implement the new command

Write the core logic for your command in the appropriate domain processor.

#### Call the module processor's method

Call your new module processor method from `CommandProcessor.#processCommand`, passing in the parsed parameters.

#### Add e2e tests

Write end-to-end tests for your command, including the happy path and any edge cases that might trip things up. Focus on testing the code in the mapper.

Build the E2E test target and run your test:

```sh
autoninja -C ../../out/Default third_party/chromium-bidi:webdriver_bidi_e2e_tests
../../out/Default/bin/run_webdriver_bidi_e2e_tests -- -k <TestName>
```

#### Update WPT expectations

If WPT expectations or baselines need to be updated, use Chromium's standard tooling (e.g. `third_party/blink/tools/blink_tool.py rebaseline-cl` or update test expectations in `third_party/blink/web_tests/`).

#### Submit for review

Upload your change list via `git cl upload` and submit it for review.
