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
sudo sh -c 'echo 0 > /proc/sys/kernel/yama/ptrace_scope'
```

## `pa_dump_heap` and `plot_superpages.py`

`pa_dump_heap` dumps the state of PartitionAlloc's heap (superpages, slot spans,
buckets, and the pool's `ReservationOffsetTable`) from a running Chrome process
into a JSON file:

```bash
autoninja -C out/Default pa_dump_heap
out/Default/pa_dump_heap --pid=<PID> --json=output.json
```

`plot_superpages.py` parses this JSON dump and generates visualizations:

```bash
python3 tools/memory/partition_allocator/plot_superpages.py --json=output.json --output=output
```

### GigaCage Occupancy Visualization (`<output>_gigacage.png`)

`plot_superpages.py` generates `<output>_gigacage.png`, displaying all
superpages in the thread-cached root's pool (e.g. the 16 GiB BRP pool via
`ReservationOffsetTable`) as a 2D grid (128 columns per row, 2 MiB per cell,
256 MiB per row), colored by state: Free, Normal Buckets, or Direct Mapped.
