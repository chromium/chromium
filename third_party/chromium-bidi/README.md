> [!NOTE]
> If you are viewing this on GitHub, the source was moved to Chromium. Please contribute there.

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

### Build Setup

The project uses Chromium build toolchains (`gn` and `ninja`) for compiling TypeScript and bundling.

1. Fetch the toolchains using `gclient` (requires `depot_tools` installed and in your PATH):
   ```sh
   gclient sync
   ```
2. Generate the Ninja build configuration:
   ```sh
   npm run gn:gen
   ```
3. Build the project:
   ```sh
   npm run build
   ```

### `cargo`

<!-- TODO(jrandolf): Remove after binaries get published -->

We use [cddlconv](https://github.com/google/cddlconv) to generate our WebDriverBiDi types before building.

1.  Install [Rust](https://rustup.rs/).
2.  Run `cargo install --git https://github.com/google/cddlconv.git cddlconv`

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

To auto-format files:

- **Python**: `git cl format --python` (or `ruff_chromium format <files>`)
- **JavaScript / TypeScript / JSON / Markdown**:
  ```sh
  ./tools/node.py node_modules/prettier/bin/prettier.cjs --write <files>
  ```
- **ESLint auto-fix**:
  ```sh
  ./tools/node.py node_modules/eslint/bin/eslint.js --fix <files>
  ```
- **keep-sorted**:
  ```sh
  keep-sorted --mode=fix <files>
  ```

### Starting WebDriver BiDi Server

This will run the server on port `8080`:

```sh
npm run server
```

Use the `PORT=` environment variable or `--port=` argument to run it on another port:

```sh
PORT=8081 npm run server
npm run server -- --port=8081
```

Use the `DEBUG` environment variable to see debug info:

```sh
DEBUG=* npm run server
```

Use the `DEBUG_DEPTH` (default: `10`) environment variable to see debug deeply nested objects:

```sh
DEBUG_DEPTH=100 DEBUG=* npm run server
```

Use the `CHANNEL=...` environment variable with one of the following values to run
the specific Chrome channel: `stable`, `beta`, `canary`, `dev`, `local`. Default is
`local`. The `local` channel means the pinned in `.browser` Chrome version will be
downloaded if it is not yet in cache. Otherwise, the requested Chrome version should
be installed.

```sh
CHANNEL=dev npm run server
```

Use the CLI argument `--verbose` to have CDP events printed to the console. Note: you have to enable debugging output `bidi:mapper:debug:*` as well.

```sh
DEBUG=bidi:mapper:debug:* npm run server -- --verbose
```

or

```sh
DEBUG=* npm run server -- --verbose
```

### Starting on Linux and Mac

TODO: verify it works on Windows.

You can also run the server by using `npm run server`. It will write
output to the file `log.txt`:

```sh
npm run server -- --port=8081 --headless=false
```

### Running with in other project

Sometimes it good to verify that a change will not affect thing downstream for other packages.
There is a useful `puppeteer` label you can add to any PR to run Puppeteer test with your changes.
It will bundle `chromium-bidi` and install it in Puppeteer project then run that package test.

## Running

### Unit tests

Running:

```sh
npm run unit
```

### E2E tests

The e2e tests serve the following purposes:

1. Brief checks of the scenarios (the detailed check is done in WPT)
2. Test Chromium-specific behavior nuances
3. Add a simple setup for engaging the specific command

The E2E tests are written using Python, in order to more-or-less align with the web-platform-tests.

#### Installation

Python 3.10+ and some dependencies are required:

```sh
python -m pip install --user pipenv
pipenv install
```

#### Running

The E2E tests require BiDi server running on the same host. By default, tests
try to connect to the port `8080`. The server can be run from the project root:

```sh
npm run e2e  # alias to to e2e:headless
npm run e2e:headful
npm run e2e:headless
```

This commands will run `./tools/run-e2e.mjs`, which will log the PyTest output to console,
Additionally the output is also recorded under `./logs/<DATE>.e2e.log`, this will contain
both the PyTest logs and in the event of `FAILED` test all the Chromium-BiDi logs.

If you need to see the logs for all test run the command with `VERBOSE=true`.

Simply pass `npm run e2e -- tests/<PathOrFile>` and the e2e will run only the selected one.
You run a specific test by running `npm run e2e -- -k <TestName>`.

Use `CHROMEDRIVER` environment to run tests in `chromedriver` instead of NodeJS runner:

```shell
CHROMEDRIVER=true npm run e2e
```

Use the `PORT` environment variable to connect to another port:

```sh
PORT=8081 npm run e2e
```

Use the `HEADLESS` to run the tests in headless (new or old) or headful modes.
Values: `new`, `old`, `false`, default: `new`.

```sh
HEADLESS=new npm run e2e
```

#### Updating snapshots

```sh
npm run e2e -- --snapshot-update true
```

See https://github.com/tophat/syrupy for more information.

### Local http server

E2E tests use local http
server [`pytest-httpserver`](https://pytest-httpserver.readthedocs.io/), which is run
automatically with the tests. However,
sometimes it is useful to run the http server outside the test
case, for example for manual debugging. This can be done by running:

```sh
pipenv run local_http_server
```

...or directly:

```sh
python tests/tools/local_http_server.py
```

### Examples

Refer to [examples/README.md](examples/README.md).

## WPT (Web Platform Tests)

WPT tests for WebDriver BiDi are located in Chromium under `third_party/blink/web_tests/external/wpt/webdriver/tests/bidi/`.

To run WPT tests in Chromium:

```sh
third_party/blink/tools/run_wpt_tests.py -t <target> webdriver/tests/bidi/
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

### Publish new `npm` release

#### Manual release

1. Dry-run

   ```sh
   npm publish --dry-run
   ```

2. Bump the `chromium-bidi` version number in `package.json` and upload a Chromium CL for review:

   ```sh
   npm version patch -m 'chore: Release v%s' --no-git-tag-version
   ```

   Instead of `patch`, use `minor` or `major` [as needed](https://semver.org/).

3. After the CL lands, tag the commit in git matching the bumped version (`v<version>`).
   CI will then automatically publish the new release to npm based on the tag.

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

1. Checkout a new branch in Chromium `src/`.
2. If your command lives in a separate spec, add a link to that spec in the `tools/update-bidi-types.sh` script.
3. Run the `tools/update-bidi-types.sh` script.
4. Run `npm run format`. If a new WebDriver BiDi command was added, this should fail with `error  Switch is not exhaustive. Cases not matched ...`.
5. Add the new BiDi command to `CommandProcessor.#processCommand` in `src/bidiMapper/CommandProcessor.ts`. For now, just have it throw an UnknownErrorException.

```typescript
case '{NEW_COMMAND_NAME}':
  throw new UnknownErrorException(
    `Method ${command.method} is not implemented.`,
  );
```

6. Upload a CL and have it reviewed and landed via Gerrit.

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

#### Update WPT expectations

If WPT expectations or baselines need to be updated, use Chromium's standard tooling (e.g. `third_party/blink/tools/blink_tool.py rebaseline-cl` or update test expectations in `third_party/blink/web_tests/`).

#### Submit for review

Upload your change list via `git cl upload` and submit it for review.
