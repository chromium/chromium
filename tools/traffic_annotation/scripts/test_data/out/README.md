This is a fake "out" directory, that contains just enough files (in the right
subdirectories) to run auditor_test.py.

out/Debug contains Python bindings for traffic_annotation.proto (which includes
files generated during build). This lets auditor.py
`import traffic_annotation_pb2`. The version of
chrome_settings_full_runtime.proto used here only contains a handful of policy,
to keep file size manageable.

out/Android contains an args.gn file with `target_os="android"`, so we can test
`get_current_platform()` in auditor.py.
