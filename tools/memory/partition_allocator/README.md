# PartitionAlloc tools

This directory contains tools useful to inspect, debug and optimize
PartitionAlloc. In particular, `pa_tcache_inspect` is used to inspect a running
Chrome instance, and report statistics on its thread caches.

## `pa_tcache_inspect`

This tool either requires to know the address of the `ThreadCacheRegistry`
instance in a given process (from attaching with `gdb`, for instance), or to
have a *local* version of elfutils installed.

On Debian, the package `libdw-dev` must be installed, and `has_local_elfutils`
set to `true` in args.gn. To allow the tool to read another process' address
space, you may have to run

```
sudo sh -c 'echo 0 > /proc/sys/kernel/yama/ptrace_scope
```

and also set `symbol_level = 2` in args.gn. Then the tool can be run with
`./pa_tcache_inspect PID`.

