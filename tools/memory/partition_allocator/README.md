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

The tool must be able to read the remote process memory, which on some Debian
configurations requires running:

```
sudo sh -c 'echo 0 > /proc/sys/kernel/yama/ptrace_scope
```
