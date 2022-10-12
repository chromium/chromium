# Examples

The examples are stored in the `/examples` folder and are intended to show how the
BiDi protocol can be used. Examples are based on
[Puppeteer's examples](https://github.com/puppeteer/puppeteer/tree/main/examples)
.

## Installation

The examples are written using Python, to align with e2e test. Python 3.6+ and some
dependencies are required:

```sh
python -m pip install --user -r requirements.txt
```

## Running

The examples require WebDriver BiDi server running on the same host on the port
`8080`.

The server can be run from the project root:

```sh
npm run server
```

ChromeDriver and EdgeDriver can be run by:

```sh
$DRIVER_BINARY_PATH --port=8080
```

Firefox can be run by:

```sh
$FIREFOX_BINARY_PATH --remote-debugging-port=8080
```

CDP-BiDi Mapper can be run from the project root:

```sh
npm run server
```

After running BiDi server, examples can be simply run:

```sh
python cross-browser.py
```
