# Tools for ChromeOS development on linux.

This directory contains tools that are useful when developing ChromeOS UI on
linux.

## run_cros.sh

`run_cros.sh` is a shell script to ash-chrome on linux, with a set of
useful predefind options (e.g. enables login, debug shortcuts and ui-devtools)
with optional customization (e.g. display settings).

```
  $ tools/chromeos/run_cros.sh
```

will run the chrome in `out/Release` directory with default settings. You may
add `src/tools/chromeos` directory to your PATH and can simply run it
without the path.

```
  $ run_cros.sh
```

If the directory for your local build is different from the default, you can
override it with `--ash-chrome-build-dir`.


```
  $ run_cros.sh --ash-chrome-build-dir=<dir_to_your_chrome_binary>
```

Please use `--help` to show more options.

```
  $ run_cros.sh --help
```

### Environment variables
You can set the following environment variable instead of using command line
flag.

`ASH_CHROME_BUILD_DIR` : specifies the directory for --ash-chrome-build-dir
