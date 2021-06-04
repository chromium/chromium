# WebDriver BiDi for Chromium Prototype

This is an experimental prototype of the [WebDriver BiDi](https://w3c.github.io/webdriver-bidi/) protocol for Chromium, implemented as a JavaScript layer translating between BiDi and CDP run in Chrome Tab.

## Setup

This is a Node.js project, so install dependencies as usual:

    npm install

Then set the `BROWSER_PATH` environment variable to a Chrome, Edge or Chromium binary to launch. For example, on macOS:

    export BROWSER_PATH="/Applications/Google Chrome Canary.app/Contents/MacOS/Google Chrome Canary"
    export BROWSER_PATH="/Applications/Microsoft Edge Canary.app/Contents/MacOS/Microsoft Edge Canary"
    export BROWSER_PATH="example/path/to/Chromium.app/Contents/MacOS/Chromium"

If it's a newly downloaded binary, first run it manually once to ensure the operating system trusts it.

## Starting the Server

    npm run bidi-server

This will run the server on port 8080. Use the `PORT` environment variable to
run it on another port:

    PORT=8081 npm run bidi-server

Use the `DEBUG` environment variable to see debug info:

    DEBUG=* npm run bidi-server

Use the `HEADLESS=false` environment variable to run browser in headful mode:

    HEADLESS=false npm run bidi-server

## Running the Tests

**Note**: Most of the tests currently fail, but this is how to run them.

The tests are written using Python, in order to learn how to eventually do this
in web-platform-tests. Python 3.6+ and some dependencies are required:

    python3 -m pip install --user -r tests/requirements.txt

Running:

    python3 -m pytest --rootdir=tests

This will run the tests against an already running server on port 8080. Use the `PORT` environment variable to connect to another port:

    PORT=8081 python3 -m pytest --rootdir=tests

## How does it work?

The architecture is described in the [WebDriver BiDi in Chrome Context implementation plan](https://docs.google.com/document/d/1VfQ9tv0wPSnb5TI-MOobjoQ5CXLnJJx9F_PxOMQc8kY).

There are 2 main modules:

1. backend WS server in `src`. It runs webSocket server, and for each ws connection runs an instance of browser with BiDi Mapper.
2. front-end BiDi Mapper in `src/bidiMapper`. Gets BiDi commands from the backend, and map them to CDP commands.

## Contributing

The BiDi commands are processed in the `src/bidiMapper/commandProcessor.ts`. To add a new command, add it to `_processCommand`, write and call processor for it.

## Debugging

If you use VS Code, you can create folder `.vscode`, and put 2 files in it:
1. `.vscode/launch.json`
```
{
  // Use IntelliSense to learn about possible attributes.
  // Hover to view descriptions of existing attributes.
  // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
  "version": "0.2.0",
  "configurations": [
    {
      "type": "pwa-node",
      "request": "launch",
      "name": "Run Server",
      "skipFiles": ["<node_internals>/**"],
      "cwd": "${workspaceFolder}",
      "console": "externalTerminal",
      "program": "${workspaceFolder}/src/.build/server.js",
      "outFiles": ["${workspaceFolder}/src/.build/**/*.js"],
      "env": {
        "DEBUG": "*",
        "PORT": "8080",
        "BROWSER_PATH": "example/path/to/Chromium"
      }
    }
  ]
}
```

2. `.vscode/settings.json`
```
{
  "python.testing.pytestArgs": ["tests"],
  "python.testing.unittestEnabled": false,
  "python.testing.nosetestsEnabled": false,
  "python.testing.pytestEnabled": true
}
```