# Automate running browserbench.org benchmarks

## MotionMark

The script in `motionmark.py` helps automatically run MotionMark1.2 benchmark in
a browser and extract a score out of it.

The following command line flags are supported:

- `-b`': The browser to run the benchmark in. The valid options currently are
         'chrome' and 'safari'.

- `-e`: Path to the executable for the driver binary.

- `-s`: The name of the test suite to run. The default is 'MotionMark'. The name
        has to be an exact match (e.g. 'HTML suite', etc.).

- `-a`: Additional command-line arguments to use when launching the browser.
        Currently supported only for Chrome (i.e. with `-b chrome`).

- `-g`: An optional githash associated with this run. This githash is reproduced
        verbatim with the result, and is not used for any other purpose.

- `-o`: Path to the output json file.

Example usage:

```
  python3 tools/browserbench-webdriver/motionmark.py  \
      -b chrome                                       \
      -e out/Default/chromedriver                     \
      -a 'enable-features=CanvasOopRasterization'     \
      -o motionmark.json
```
