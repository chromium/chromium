# Automate running browserbench.org benchmarks

The following command line flags are supported for all benchmarks:

- `-b`': The browser to run the benchmark in. The valid options currently are
         'chrome', 'safari', and 'stp'.

- `-e`: Path to the executable for the driver binary.

- `-a`: Additional command-line arguments to use when launching the browser.
        Currently supported only for Chrome (i.e. with `-b chrome`).

- `-g`: An optional githash associated with this run. This githash is reproduced
        verbatim with the result, and is not used for any other purpose.

- `-o`: Path to the output json file.

- `--chrome-path`: Path to the chrome binary. If not present, default binary
                   is used.

In order to use these scripts, you must have the following present:

- Install [chromedriver][https://chromedriver.chromium.org/downloads].
- Install [selenium][https://pypi.org/project/selenium/].

Safari requires enabling remote automation:

1. Enable the developer menu in the Advanced tab of Preferences.
2. Enable 'Remote Automation' via the 'Developer' menu.
3. Run `safaridriver --enable`.

## MotionMark

The script in `motionmark.py` helps automatically run MotionMark1.2 benchmark in
a browser and extract a score out of it.

This script supports the following additional command line flags:

- `-s`: The name of the test suite to run. The default is 'MotionMark'. The name
        has to be an exact match (e.g. 'HTML suite', etc.).


Example usage:

```
  python3 tools/browserbench-webdriver/motionmark.py  \
      -b chrome                                       \
      -e out/Default/chromedriver                     \
      -a 'enable-features=CanvasOopRasterization'     \
      -o motionmark.json
```


## Speedometer

This script in `speedometer.py` runs Speedometer2.0 benchmark in a browser, and
extracts the score out of it.

Example usage:

```
  python3 tools/browserbench-webdriver/speedometer.py  \
      -b chrome                                        \
      -e out/Default/chromedriver                      \
      -o speedometer.json
```

## JetStream

This script in `jetstream.py` runs JetStream benchmark in a browser, and
extracts the score out of it.

Example usage:

```
  python3 tools/browserbench-webdriver/jetstream.py    \
      -b chrome                                        \
      -e out/Default/chromedriver                      \
      -o jetstream.json
```
