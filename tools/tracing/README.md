# Perfetto Trace Collection Profiling for Chrome Developers
This tool provides a way for chrome developers to collect perfetto traces
and symbolize them.

# Startup Tracing
The script profile_chrome_startup runs a chrome trace on android, locates the
trace file, and automatically copies it to a local folder.

Example Linux Platform Usage:
```
tools/tracing/profile_chrome_startup --platform=android
```

# Future Work
We are planning to add symbolization support and trace collection for other
platforms.

See the google internal design doc for more details pertaining to this tool:
https://docs.google.com/document/d/1BJPbcl5SPjOvuRuP1JSFAUPK3ZWNIS7j1h94rPHRzVE
