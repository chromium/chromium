# WebDriver BiDi for Chromium [![chromium-bidi on npm](https://img.shields.io/npm/v/chromium-bidi)](https://www.npmjs.com/package/chromium-bidi)

## CI status

![E2E Tests](https://github.com/GoogleChromeLabs/chromium-bidi/actions/workflows/e2e.yml/badge.svg)
![Unit Tests](https://github.com/GoogleChromeLabs/chromium-bidi/actions/workflows/unit.yml/badge.svg)
![WPT Tests](https://github.com/GoogleChromeLabs/chromium-bidi/actions/workflows/wpt.yml/badge.svg)

![Pre-commit](https://github.com/GoogleChromeLabs/chromium-bidi/actions/workflows/pre-commit.yml/badge.svg)

This is an implementation of the
[WebDriver BiDi](https://w3c.github.io/webdriver-bidi/) protocol with some
extensions (**BiDi+**)
for Chromium, implemented as a JavaScript layer translating between BiDi and CDP,
running inside a Chrome tab.

Current status can be checked
at [WPT WebDriver BiDi status](https://wpt.fyi/results/webdriver/tests/bidi).

## BiDi+

**"BiDi+"** is an extension of the WebDriver BiDi protocol. In addition to [WebDriver BiDi](https://w3c.github.io/webdriver-bidi/) it has:

### Command `cdp.sendCommand`

```cddl
CdpSendCommandCommand = {
  method: "cdp.sendCommand",
  params: ScriptEvaluateParameters,
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

### Command `cdp.getSession`

```cddl
CdpGetSessionCommand = {
   method: "cdp.sendCommand",
   params: ScriptEvaluateParameters,
}

CdpGetSessionParameters = {
   context: BrowsingContext,
}

CdpGetSessionResult = {
   session: text,
}
```

The command returns the default CDP session for the selected browsing context.

### Events `cdp`

```cddl
CdpEventReceivedEvent = {
   method: "cdp.<CDP Event Name>",
   params: CdpEventReceivedParameters,
}

CdpEventReceivedParameters = {
   event: text,
   params: any,
   session: text,
}
```

The event contains a CDP event.

### Field `channel`

Each command can be extended with a `channel`:

```cddl
Command = {
   id: js-uint,
   channel?: text,
   CommandData,
   Extensible,
}
```

If provided and non-empty string, the very same `channel` is added to the response:

```cddl
CommandResponse = {
   id: js-uint,
   channel?: text,
   result: ResultData,
   Extensible,
}

ErrorResponse = {
  id: js-uint / null,
  channel?: text,
  error: ErrorCode,
  message: text,
  ?stacktrace: text,
  Extensible
}
```

When client uses
commands [`session.subscribe`](https://w3c.github.io/webdriver-bidi/#command-session-subscribe)
and [`session.unsubscribe`](https://w3c.github.io/webdriver-bidi/#command-session-unsubscribe)
with `channel`, the subscriptions are handled per channel, and the corresponding
`channel` filed is added to the event message:

```cddl
Event = {
  channel?: text,
  EventData,
  Extensible,
}
```

## Dev Setup

### `npm`

This is a Node.js project, so install dependencies as usual:

```sh
npm install
```

### pre-commit.com integration

Refer to the documentation at [.pre-commit-config.yaml](.pre-commit-config.yaml).

### Starting the Server

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

Use the CLI argument `--headless=false` to run browser in headful mode:

```sh
npm run server -- --headless=false
```

Use the `CHANNEL=...` environment variable or `--channel=...` argument with one of
the following values to run the specific Chrome channel: `stable`,
`beta`, `canary`, `dev`.

The requested Chrome version should be installed.

```sh
CHANNEL=dev npm run server
npm run server -- --channel=dev
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

You can also run the server by using script `./runBiDiServer.sh`. It will write
output to the file `log.txt`:

```sh
./runBiDiServer.sh --port=8081 --headless=false
```

## Running

### Unit tests

Running:

```sh
npm run unit
```

### E2E tests

The E2E tests are written using Python, in order to learn how to eventually do
this in web-platform-tests.

### Installation

Python 3.6+ and some dependencies are required:

```sh
python3 -m pip install --user -r tests/requirements.txt
```

### Running

The E2E tests require BiDi server running on the same host. By default, tests
try to connect to the port `8080`. The server can be run from the project root:

```sh
npm run e2e  # alias to to e2e-headless
npm run e2e-headful
npm run e2e-headless
```

Use the `PORT` environment variable to connect to another port:

```sh
PORT=8081 npm run e2e
```

### Examples

Refer to [examples/README.md](examples/README.md).

## WPT (Web Platform Tests)

WPT is added as
a [git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules). To get run
WPT tests:

### Check out and setup WPT

#### 1. Check out WPT

```sh
git submodule update --init
```

#### 2. Go to the WPT folder

```sh
cd wpt
```

#### 3. Set up virtualenv

Follow the [_System
Setup_](https://web-platform-tests.org/running-tests/from-local-system.html#system-setup)
instructions.

#### 4. Setup `hosts` file

Follow
the [`hosts` File Setup](https://web-platform-tests.org/running-tests/from-local-system.html#hosts-file-setup)
instructions.

##### 4.a On Linux, macOS or other UNIX-like system

```sh
./wpt make-hosts-file | sudo tee -a /etc/hosts
```

##### 4.b On **Windows**

This must be run in a PowerShell session with Administrator privileges:

```sh
python wpt make-hosts-file | Out-File $env:SystemRoot\System32\drivers\etc\hosts -Encoding ascii -Append
```

If you are behind a proxy, you also need to make sure the domains above are excluded
from your proxy lookups.

#### 5. Set `BROWSER_BIN`

Set the `BROWSER_BIN` environment variable to a Chrome, Edge or Chromium binary to launch.
For example, on macOS:

```sh
# Chrome
export BROWSER_BIN="/Applications/Google Chrome Canary.app/Contents/MacOS/Google Chrome Canary"
export BROWSER_BIN="/Applications/Google Chrome Dev.app/Contents/MacOS/Google Chrome Dev"
export BROWSER_BIN="/Applications/Google Chrome Beta.app/Contents/MacOS/Google Chrome Beta"
export BROWSER_BIN="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
export BROWSER_BIN="/Applications/Chromium.app/Contents/MacOS/Chromium"

# Edge
export BROWSER_BIN="/Applications/Microsoft Edge Canary.app/Contents/MacOS/Microsoft Edge Canary"
export BROWSER_BIN="/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"
```

### Run WPT tests

#### 1. Make sure you have Chrome Dev installed

https://www.google.com/chrome/dev/

#### 2. Build Chromedriver BiDi

Oneshot:

```sh
npm run build
```

Continuously:

```sh
npm run build --watch
```

#### 3. Run

```sh
npm run wpt -- webdriver/tests/bidi/
```

### Update WPT expectations if needed

```sh
UPDATE_EXPECTATIONS=true npm run wpt -- webdriver/tests/bidi/
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

### Contributing

The BiDi commands are processed in the `src/bidiMapper/commandProcessor.ts`. To add a
new command, add it to `_processCommand`, write and call processor for it.

### Publish new `npm` release

#### Automatic release

We use [release-please](https://github.com/googleapis/release-please) to automate releases. When a release should be done, check for the release PR in our [pull requests](https://github.com/GoogleChromeLabs/chromium-bidi/pulls) and merge it.

#### Manual release

1. Dry-run

   ```sh
   npm publish --dry-run
   ```

1. Open a PR bumping the chromium-bidi version number in `package.json` for review:

   ```sh
   npm version patch -m 'chore: Release v%s' --no-git-tag-version
   ```

   Instead of `patch`, use `minor` or `major` [as needed](https://semver.org/).

1. After the PR is reviewed, [create a GitHub release](https://github.com/GoogleChromeLabs/chromium-bidi/releases/new) specifying the tag name matching the bumped version.
   Our CI then automatically publishes the new release to npm based on the tag name.
