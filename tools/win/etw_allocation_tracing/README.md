# ETW allocation tracing

Chromium satisfies almost every allocation out of PartitionAlloc's own super
pages, so the Windows heap ETW provider sees virtually nothing. Tools that
attribute native memory to call stacks on Windows - WPA's heap tables, HUD's
Heap tab - are therefore blind to Chromium's heap.

`base::allocator::EtwAllocationObserver` fixes that by emitting a TraceLogging
event for every allocation and free reported by the allocator dispatcher. The
dispatcher covers both the allocator shim (`malloc`/`new`, backed by
PartitionAlloc-Everywhere) and direct PartitionAlloc use, with no double
counting.

`chromium_allocation_tracing.wprp` is a ready-made Windows Performance Recorder
profile for capturing those events with call stacks.

## Provider

| | |
| --- | --- |
| Name | `Chromium.AllocationTracing` |
| GUID | `C9D77D7A-5129-51F6-3C45-3E0FD0418284` |

The GUID is the standard TraceLogging hash of the name, so `*Chromium.AllocationTracing`
and the GUID are interchangeable in `wpr` and `xperf`.

Keywords:

| Bit | Meaning |
| --- | --- |
| `0x1` | Allocations |
| `0x2` | Frees |
| `0x10` | Browser process |
| `0x20` | Renderer processes |
| `0x40` | GPU process |
| `0x80` | Utility processes |
| `0x100` | Any other process type |

A session must request at least one of `0x1` and `0x2`. Enabling with a
`MatchAnyKeyword` of 0 leaves the provider silent.

The process type bits select which processes write events rather than which
events get written. A session that sets none of them traces every process, so
`0x3` captures allocations and frees everywhere. A session that sets any of them
traces only processes of those types: `0x23` traces renderers only, and `0x53`
the browser and GPU processes only. The events themselves only ever carry `0x1`
or `0x2`.

Processes deselected this way stop writing events but keep the observer
installed, so they still pay its small idle cost (see [Cost](#cost)). The
`enabled-processes` parameter described under [Capturing](#capturing) keeps
processes from installing it at all.

The selection is made inside each process against the union of the keywords of
all the sessions that enable the provider, so it scopes which processes *write*
events, not which session *receives* them. Concurrent sessions therefore cannot
select different processes: a session asking for `0x23` narrows a concurrent
`0x3` session to renderers as well. In the other direction, a session receives
events from every process the union selects, not only the ones it asked for, so
a renderer-scoped session running alongside a browser-scoped one will see
browser events too. ETW cannot filter that out, because `MatchAnyKeyword` is an
or-match and the events carry only `0x1` or `0x2`. Scope the session with a
process id filter if you need a hard guarantee.

Events:

| Name | Source | Fields |
| --- | --- | --- |
| `Alloc` | allocator shim (`malloc`/`new`) | `Addr`, `Size` |
| `Free` | allocator shim | `Addr` |
| `PAAlloc` | direct PartitionAlloc | `Addr`, `Size`, `Type` |
| `PAFree` | direct PartitionAlloc | `Addr` |

Frees carry no size. Consumers have to remember it from the matching
allocation, exactly as they already do for the Windows heap provider. The
dispatcher decomposes `realloc` into a free followed by an allocation, so there
is no distinct realloc event.

`Type` is the name of the PartitionAlloc partition the allocation came from,
and is empty for untyped allocations. Only `PAAlloc` carries it; the shim's
`Alloc` has no such field, so decode the payload according to the event name.

## Capturing

The observer is behind a feature flag and is only installed at process startup,
so the browser has to be launched with it:

```
chrome.exe --enable-features=AllocationEtwTracing
```

The flag propagates to child processes automatically. Once the browser is
running, tracing can be turned on and off at will without relaunching, which is
the main practical advantage over Windows heap tracing (that one needs the
target process to be pre-registered before it starts).

By default every process installs the observer, and so pays its idle cost even
while no session is listening. To install it in some process types only, add
the `enabled-processes` parameter:

```
chrome.exe --enable-features=AllocationEtwTracing:enabled-processes/renderer-only
```

| Value | Processes |
| --- | --- |
| `all-processes` | All (the default) |
| `browser-only` | Browser |
| `renderer-only` | Renderers |
| `gpu-only` | GPU |
| `browser-and-renderer` | Browser and renderers |
| `browser-and-gpu` | Browser and GPU |
| `non-renderer` | All but renderers |

The values follow PartitionAlloc's parameter of the same name. Unlike the
process type keywords, which can change the selection without relaunching, this
is fixed for the lifetime of the browser, but the processes it leaves out run at
full speed.

Recording needs an elevated prompt:

```
wpr -start chromium_allocation_tracing.wprp!ChromiumAllocationTracing -filemode
...exercise the browser...
wpr -stop allocations.etl
```

Profiles in the file:

| Profile | Stacks |
| --- | --- |
| `ChromiumAllocationTracing` | On allocation events only |
| `ChromiumAllocationTracingNoStacks` | None |

Both trace every process. To trace some process types only, add their bits to
the profile's `<Keyword Value="0x3"/>`, e.g. `0x23` for renderers only.

The equivalent with `xperf`, if you prefer it:

```
xperf -start alloc -on C9D77D7A-5129-51F6-3C45-3E0FD0418284:0x3:5'stack ^
      -f alloc.etl -buffersize 1024 -minbuffers 512 -maxbuffers 512
xperf -start -on PROC_THREAD+LOADER -f kernel.etl
...
xperf -stop alloc -stop -d allocations.etl
```

`PROC_THREAD+LOADER` is not optional. Without module load events the stacks
cannot be symbolicated. Here too, `:0x23:` instead of `:0x3:` would trace
renderers only.

## Why stacks are filtered by event name

Call stacks are not captured in-process. The session enables the provider with
`EVENT_ENABLE_PROPERTY_STACK_TRACE` and the kernel walks the stack at
event-write time. This works in sandboxed child processes: writing ETW events
needs no handles, files, registry or ALPC access. Only starting a session does,
and that happens in the profiler.

Stack walking dominates the cost of a capture, and a free's stack is not the
interesting one - the interesting stack is the one that allocated the memory.
So the profile asks for stacks on `Alloc` and `PAAlloc` only.

The usual way to do that, `EVENT_FILTER_TYPE_STACKWALK`, selects events by id
and is ignored for TraceLogging providers, which have no static event ids. The
mechanism that does work is `EVENT_FILTER_TYPE_STACKWALK_NAME` (Windows 10 1709
and later), which selects by event name. That is why the event names above are
terse: they are filter handles, not just labels. In the profile this is a
`<StackEventNameFilters FilterIn="true">` element.

`wpr -profiledetails` will confirm the filter resolved:

```
c9d77d7a-5129-51f6-3c45-3e0fd0418284: 0x3: 0x05 : Stack
        Stacks filtered in by name, keyword <unset> level 0xff:
                Alloc
                PAAlloc
```

Note that a *private*, in-process ETW session cannot use any of this. Such
sessions accept scope filters such as `EVENT_FILTER_TYPE_PID` but reject
name-based and stack filters with `ERROR_INVALID_PARAMETER`, and they have no
kernel logger to walk stacks with in the first place.

## Cost

Measured on a release build with a 512K allocation/free pair microbenchmark,
per allocation *and* its matching free, without stack walking:

| | `malloc`/`free` | direct `PartitionRoot` |
| --- | --- | --- |
| Feature off | baseline | baseline |
| Observer installed, no session | +4% | +4% |
| Observer installed, session running | 5.6x | 4.1x |

That works out to roughly 400-500 ns and 164 bytes on disk per event. The
numbers are absolute overheads on a build with DCHECKs on, so the relative
multipliers are smaller than they look on a build with a faster baseline.

The permanent ~4% is the price of being able to turn tracing on mid-flight, and
it only applies when the feature is enabled; with the feature off the observer
is never installed and the dispatcher does not even hook the allocators. The
same goes for processes that `enabled-processes` leaves out. When no session is
listening, or in processes that the sessions' process type keywords deselect,
the cost per allocation is one relaxed atomic load and a well-predicted branch.

Stack walking is not included above and is considerably more expensive than the
event write itself. Expect high event rates: budget large buffers (the profile
uses 512 x 1024 KB) and check `wpr`'s reported lost event count.

## Analysis

Out of the box, WPA decodes the events into the **Generic Events** table, which
has a real `Stack` column, so allocation stacks can be viewed and aggregated by
count. The payload fields land in untyped positional `Field 1..N` columns,
which cannot be summed, so byte totals per stack are not available without a
plugin.

For refset-style byte-weighted tables, a WPA plugin built on the
[Microsoft Performance Toolkit SDK](https://github.com/microsoft/microsoft-performance-toolkit-sdk)
can pair the allocations with their frees and render real expandable stack
trees via `ITableBuilderWithRowCount.AddHierarchicalColumn`. The built-in Heap
table is not extensible - it is wired to a native cooker with fixed provider
GUIDs - so a custom table is the only route.

When rendering stacks, trim the three `ntdll.dll` frames that the event write
itself contributes (`EtwEventWrite`, `EtwpEventWriteFull`, `ZwTraceEvent`).

## Source

* `base/allocator/etw_allocation_observer_win.{h,cc}` - the provider and observer
* `base/allocator/etw_allocation_observer_win_unittest.cc` - tests, including an
  end-to-end capture through a private ETW session and a check on the
  `EVENT_FILTER_EVENT_NAME` payload layout
* `components/memory_system/memory_system.cc` - where the observer is installed
* `components/memory_system/memory_system_features.cc` - the `AllocationEtwTracing` feature
  and its `enabled-processes` parameter
