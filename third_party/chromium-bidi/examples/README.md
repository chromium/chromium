# Examples

The examples are stored in the `/examples` folder and are intended to show who
the BiDi protocol can be used. Examples are based on
[Puppeteer's examples](https://github.com/puppeteer/puppeteer/tree/main/examples)
.

## Installation

The examples are written using Python, to align with e2e test. Python 3.6+ and
some dependencies are required:

```sh
python3 -m pip install --user -r tests/requirements.txt
```

## Running

The examples require BiDi server running on the same host on the port `8080`.
The server can be run from the project root:

```sh
npm run server
```

After running server, examples can be simply run:

```sh
python3 examples/cross-browser.py
```
