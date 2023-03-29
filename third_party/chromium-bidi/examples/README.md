# Examples

The examples are stored in this folder and are intended to demonstrate how the
BiDi protocol can be used. Examples are based on
[Puppeteer's examples](https://github.com/puppeteer/puppeteer/tree/main/examples).

## Installation

The examples are written using Python, in order to align with E2E tests.
Python 3.6+ and some dependencies are required:

```sh
python -m pip install --user -r requirements.txt
```

## Running

The examples require WebDriver BiDi server running on the same host on port `8080`.

The server (CDP-BiDi Mapper) can be run from the project root:

```sh
npm run server
```

ChromeDriver and EdgeDriver can be run by:

```sh
$DRIVER_BINARY_PATH [--port=8080]
```

Firefox can be run by:

```sh
$FIREFOX_BINARY_PATH [--remote-debugging-port=8080]
```

After running the BiDi server, examples can be simply executed, for example:

```sh
<!-- keep-sorted start -->
python3 console_log_example.py
python3 print_example.py
python3 screenshot_example.py
python3 script_example.py
<!-- keep-sorted end -->
```

### Classic-BiDi examples

CDP-BiDi Mapper does not support WebDriver classic by itself, so you need to use ChromeDriver, EdgeDriver or GeckoDriver to run the following example:

```sh
python3 classic_to_bidi_example.py
```
