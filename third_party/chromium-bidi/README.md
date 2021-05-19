# WebDriver BiDi for Chromium Prototype

This is an experimental prototype of the [WebDriver BiDi](https://w3c.github.io/webdriver-bidi/) protocol for Chromium, implemented as a JavaScript layer translating between BiDi and CDP run in Chrome Tab.

## Setup

This is a Node.js project, so install dependencies as usual:

    npm install

Then set the `CHROME_PATH` environment variable to a Chrome, Edge or Chromium binary to launch. For example, on macOS:

    export CHROME_PATH="/Applications/Google Chrome Canary.app/Contents/MacOS/Google Chrome Canary"
    export CHROME_PATH="/Applications/Microsoft Edge Canary.app/Contents/MacOS/Microsoft Edge Canary"
    export CHROME_PATH="example/path/to/Chromium.app/Contents/MacOS/Chromium"

If it's a newly downloaded binary, first run it manually once to ensure the operating system trusts it.

## Starting the Server

    npm run bidi-server

This will run the server on port 8080. Use the `PORT` environment variable to
run it on another port:

    PORT=8081 npm run bidi-server

Use the `DEBUG` environment variable to see debug info:

    DEBUG=* npm run bidi-server

## Running the Tests

**Note**: Most of the tests currently fail, but this is how to run them.

The tests are written using Python, in order to learn how to eventually do this
in web-platform-tests. Python 3.6+ and some dependencies are required:

    python3 -m pip install --user -r tests/requirements.txt

Running:

    python3 -m pytest --rootdir=tests

This will run the tests against an already running server on port
8080. Use the `PORT` environment variable to connect to another port:

    PORT=8081 python3 -m pytest --rootdir=tests


## Contributing
The BiDi commands are processed in the `src/bidiMapper/bidiCommandsProcessor.js`. To add a new command, add it to `processCommand`, write and call processor for it.
