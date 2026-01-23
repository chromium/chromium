# UNSAFE_TODO() remover

`UNSAFE_TODO()` is a macro that disables the `-Wunsafe-buffer-usage` compiler
warning. This tool identifies and removes the ones that become safe to remove.

This happens frequently, because code evolves and usage of std::array and
base::span increases over time.

## Prerequisites

To get maximum coverage, this tools cross-compiles on every platforms except
iOS. Please update your `.gclient` with:
```
target_os = ["win", "android", "linux", "chromeos", "mac", "fuchsia"]
```

Moreover, you need at least **5TB of free disk space** to store all the build
artifacts.

## **Usage**

```
# 1. Run analysis:
# Start the verification process. Progress is saved to `out.json` and can be
# resumed at any time
./unsafe_todo_remover.py

# 2. Apply fixes:
# Once the analysis finds safe-to-remove wrappers, apply the changes to the
# source code
./unsafe_todo_remover.py --apply
```
