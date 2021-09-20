# PartitionAlloc tools

This directory contains tools useful to inspect, debug and optimize
PartitionAlloc. In particular, `pa_tcache_inspect` is used to inspect a running
Chrome instance, and report statistics on its thread caches.

## `pa_tcache_inspect`

This tool displays data about any running Chrome process. The main constraint is
that both the tool and the running instance have to be built at revisions where
the allocator's layout is identical. For best results, it should be the same
revision whenever possible.

It works by first identifying the address of the thread cache registry, then use
it to find out all other data structures. They are then read from the remote
process, and displayed live.

Two ways of finding the object address are supported:

1. Using DWARF information
2. By scanning the process memory

In both cases, the tool must be able to read the remote process memory, which on
some Debian configuration requires running:

```
sudo sh -c 'echo 0 > /proc/sys/kernel/yama/ptrace_scope
```

### DWARF information

This requires a *local* version of elfutils installed.

On Debian, the package `libdw-dev` must be installed, and `has_local_elfutils`
set to `true` in args.gn.

This also requires `symbol_level = 2` in args.gn. Then the tool can be run with
`./pa_tcache_inspect PID`.

### Memory scanning

Just provide "0" as the optional third argument, and the tool will fall back to
memory scanning.
