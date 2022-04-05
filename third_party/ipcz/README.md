# ipcz

## Overview
ipcz is a fully cross-platform C library for interprocess communication (IPC)
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

## Setup
To set up a new local repository, first install
[depot\_tools](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up)
and make sure it's in your `PATH`.

Then from within the repository root:

```
cp .gclient-default .gclient
gclient sync
```

When updating a local copy of the repository, it's a good idea to rerun
`gclient sync` to ensure that all external dependencies are up-to-date.

## Build
ipcz uses GN for builds. This is provided by the `depot_tools` installation.

To create a new build configuration, first create a directory for it. For
example on Linux or macOS:

```
mkdir -p out/Debug
```

Then run `gn args` to create and edit the build configuration:

```
gn args out/Debug
```

For a typical debug build the contents may be as simple as:

```
is_debug = true
```

Now targets can be built:

```
ninja -C out/Debug ipcz_tests

# Hope they all pass!
./ipcz_tests
```

## Usage
ipcz may be statically linked into a project, or it may be consumed as a shared
library. A shared library can be built with the `ipcz_shared` target.

The library is meant to be consumed exclusively through the C ABI defined in
[`include/ipcz/ipcz.h`](include/ipcz/ipcz.h). Applications populate an `IpczAPI`
structure by calling `IpczGetAPI()`, the library's only exported symbol. From
there they can create and connect nodes and establish portals for higher-level
communication.

Applications must provide each node with an implementation of the `IpczDriver`
function table to perform a variety of straightforward, platform- and
environment-specific tasks such as establishing a basic I/O transport,
generating random numbers, and allocating shared memory regions.
See [reference drivers](src/reference_drivers) for examples.

## In Chromium
This directory in the Chromium tree *is* the source of truth for ipcz. It is not
a mirror of an external repository, so there is no separate maintenance of local
modifications or other versioning considerations.

The decision to place ipcz sources in `//third_party/ipcz` was made in light of
some unique characteristics:

- No dependencies on //base or other Chromium directories are allowed, with the
  exception of a very small number of carefully chosen APIs allowed when
  integrating with Chromium builds.

- The library is structured and maintained to be useful as a standalone
  dependency, without needing any other contents of the Chromium tree or its
  large set of dependencies.

- Certain style and dependency violations are made in service of the above two
  points; for example, ipcz depends on parts of Abseil disallowed in the rest of
  upstream Chromium, and ipcz internally uses relative include paths rather than
  paths rooted in Chromium's top-level directory.

## Design
Some extensive coverage of ipcz design details can be found
[here](https://docs.google.com/document/d/1i49DF2af4JDspE1fTXuPrUvChQcqDChdHH6nx4xiyoY/edit?resourcekey=0-t_viq9NAbGb5kr_ni9scTA#heading=h.rlzi4jxw96rk).

