# ipcz

## Overview
ipcz is a fully cross-platform library for interprocess communication (IPC)
intended to address two generic problems: *routing* and *data transfer*.

### Routing
With ipcz, applications create pairs of entangled **portals** to facilitate
bidirectional communication. These are grouped into collections called
**nodes**, which typically correspond 1:1 to OS processes.

Nodes may be explicitly connected by an application to establish portal pairs
which span the node boundary.

```
                   ┌───────┐
       Connect     │       │     Connect
       ┌──────────>O   A   O<───────────┐
       │           │       │            │
       │           └───────┘            │
       │                                │
       v                                v
   ┌───O───┐                        ┌───O───┐
   │       │                        │       │
   │   B   │                        │   C   │
   │       │                        │       │
   └───────┘                        └───────┘
```

Here nodes A and B are explicitly connected by the application, as are nodes A
and C. B can put stuff into its portal, and that stuff will come out of the
linked portal on A.

But portals may also be sent over other portals. For example, B may create a
new pair of portals...

```
                   ┌───────┐
       Connect     │       │     Connect
       ┌──────────>O   A   O<───────────┐
       │           │       │            │
       │           └───────┘            │
       │                                │
       v                                v
   ┌───O───┐                        ┌───O───┐
   │       │                        │       │
   │   B   │                        │   C   │
   │ O───O │                        │       │
   └───────┘                        └───────┘
```

...and send one over B's existing portal to A:

```
                   ┌───────┐
       Connect     │       │     Connect
       ┌──────────>O   A   O<───────────┐
       │ ┌────────>O       │            │
       │ │         └───────┘            │
       │ │                              │
       v v                              v
   ┌───O─O─┐                        ┌───O───┐
   │       │                        │       │
   │   B   │                        │   C   │
   │       │                        │       │
   └───────┘                        └───────┘
```

Node A may then forward this new portal along its own existing portal to node C:

```
                   ┌───────┐
       Connect     │       │     Connect
       ┌──────────>O   A   O<───────────┐
       │           │       │            │
       │           └───────┘            │
       │                                │
       v                                v
   ┌───O───┐                        ┌───O───┐
   │       │                        │       │
   │   B   O────────────────────────O   C   │
   │       │                        │       │
   └───────┘                        └───────┘
```


As a result, the application ends up with a portal on node B linked directly to
a portal on node C, despite no explicit effort by the application to connect
these two nodes directly.

ipcz enables this seamless creation and transferrence of routes with minimal
end-to-end latency and amortized overhead.

### Data Transfer
ipcz supports an arbitrarily large number of interconnected portals across the
system, with potentially hundreds of thousands of individual portal pairs
spanning any two nodes. Apart from managing how these portal pairs route their
communications (i.e. which nodes they must traverse from end-to-end), ipcz is
also concerned with how each communication is physically conveyed from one node
to another.

In a traditional IPC system, each transmission typically corresponds to a system
I/O call of some kind (e.g. POSIX `writev()`, or `WriteFile()` on Windows).
These calls may require extra copies of transmitted data, incur additional
context switches, and potentially elicit other forms of overhead (e.g. redundant
idle CPU wakes) under various conditions.

ipcz tries to avoid such I/O operations in favor of pure userspace memory
transactions, falling back onto system I/O only for signaling and less frequent
edge cases. To facilitate this behavior, every pair of interconnected nodes has
a private shared memory pool managed by ipcz.

## Usage

Applications (specifically Mojo Core) must provide each node with an implementation
of the `IpczDriver` interface to perform a variety of straightforward, platform- and
environment-specific tasks such as establishing a basic I/O transport,
generating random numbers, and allocating shared memory regions.
See [reference drivers](src/reference_drivers) for examples.

This directory in the Chromium tree is the source of truth for ipcz. It is integrated
directly into Mojo Core (`//mojo/core`) and is not intended to be used as a standalone
or embeddable library outside of Chromium.

Consequently, ipcz is implemented using Chromium's `//base` library.

In the past ipcz allowed other applications to link against it as a library with
stable C interfaces, which was called 'standalone' mode. In order to support
standalone mode, `//base` was not allowed as a dependency. The standalone mode
is no longer supported by ipcz. In particular, currently using `//base` is
allowed, but a few of its polyfills are still referenced in code (like
`safe_math.h`) until they are factored out.

## Design
Some extensive coverage of ipcz design details can be found
[here](https://docs.google.com/document/d/1i49DF2af4JDspE1fTXuPrUvChQcqDChdHH6nx4xiyoY/edit?resourcekey=0-t_viq9NAbGb5kr_ni9scTA#heading=h.rlzi4jxw96rk).

