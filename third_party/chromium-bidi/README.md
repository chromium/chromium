# WebDriver BiDi for Chromium

This is an implementation of the
[WebDriver BiDi](https://w3c.github.io/webdriver-bidi/) protocol for Chromium,
implemented as a JavaScript layer translating between BiDi and CDP, running
inside a Chrome tab.

Current status can be checked here:
[Chromium BiDi progress](https://docs.google.com/spreadsheets/d/1acM-kHlubpwnW1mFboS9hePawq3u1kf21oQzD16q-Ao/edit?usp=sharing&resourcekey=0-PuLHQYLmDJUOXH_mFO-QiA)
.

## Setup

This is a Node.js project, so install dependencies as usual:

```sh
npm install
```

## Starting the Server

```sh
npm run server
```

This will run the server on port 8080. Use the `PORT` environment variable
or `--port=...` argument to run it on another port:

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

### Starting on Linux and Mac

TODO: verify if it works on Windows.

You can also run the server by using script `./runBiDiServer.sh`. It will write
output to the file `log.txt`:

```sh
./runBiDiServer.sh --port=8081 --headless=false
```

## Running the Tests and Examples

### Unit tests

Running:

```sh
npm run unit
```

### e2e tests

The e2e tests are written using Python, in order to learn how to eventually do
this in web-platform-tests.

#### Installation

Python 3.6+ and some dependencies are required:

```sh
python3 -m pip install --user -r tests/requirements.txt
```

#### Running

The e2e tests require BiDi server running on the same host. By default, tests
try to connect to the port `8080`. The server can be run from the project root:

```sh
npm run e2e
```

Use the `PORT` environment variable to connect to another port:

```sh
PORT=8081 npm run e2e
```

### Examples

The examples are stored in the `/examples` folder and are intended to show who
the BiDi protocol can be used. Examples are based on
[Puppeteer's examples](https://github.com/puppeteer/puppeteer/tree/main/examples)
.

#### Instalation

The examples are written using Python, to align with e2e test. Python 3.6+ and
some dependencies are required:

```sh
python3 -m pip install --user -r tests/requirements.txt
```

#### Running

The examples require BiDi server running on the same host on the port `8080`.
The server can be run from the project root:

```sh
npm run server
```

After running server, examples can be simply run:

```sh
python3 examples/cross-browser.py
```

## How does it work?

The architecture is described in the
[WebDriver BiDi in Chrome Context implementation plan](https://docs.google.com/document/d/1VfQ9tv0wPSnb5TI-MOobjoQ5CXLnJJx9F_PxOMQc8kY)
.

There are 2 main modules:

1. backend WS server in `src`. It runs webSocket server, and for each ws
   connection runs an instance of browser with BiDi Mapper.
2. front-end BiDi Mapper in `src/bidiMapper`. Gets BiDi commands from the
   backend, and map them to CDP commands.

## Contributing

The BiDi commands are processed in the `src/bidiMapper/commandProcessor.ts`. To
add a new command, add it to `_processCommand`, write and call processor for it.
