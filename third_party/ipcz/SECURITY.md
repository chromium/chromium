# Overview

ipcz is the underlying IPC routing and data transfer library used by Mojo. It
provides the mechanisms for transferring parcels (data and platform handles)
between nodes, which typically reside in separate processes. ipcz relies on its
embedder (via `IpczDriver`) to provide platform-specific facilities such as
channel transports, shared memory allocation, and platform handle brokering.

Communication between directly connected pairs of nodes is coordinated over a
`NodeLink`, backed by dedicated shared memory regions (`NodeLinkMemory`). ipcz
uses a lock-free block allocation scheme (`BlockAllocator`) within
`NodeLinkMemory` for internal link state (e.g. `RouterLinkState`) as well as
parcel data payloads.

# What constitutes a security vulnerability?

Shared memory regions backing a `NodeLinkMemory` are mapped read-write by both
connected nodes. ipcz's threat model explicitly assumes that any peer node may
be malicious.

Because a node already possesses direct, unrestricted read-write access to the
entire shared memory region, actions that manipulate state *within*
`NodeLinkMemory` do not constitute security vulnerabilities on their own. For
example, a malicious node can already:
- Directly overwrite arbitrary bytes in any block.
- Corrupt allocator freelists or shared refcounts to cause overlapping or
  prematurely-reused block allocations.

These actions do not grant an attacker any capabilities within the shared
region that they did not already have. Consequently:
- **Invalid reports:** A proof-of-concept that relies on ASan APIs to manually
  poison/unpoison `NodeLinkMemory` to simulate a "Use-After-Free" or
  double-free within shared memory is invalid. ASan single-process heap
  invariants do not apply to shared memory buffers that are concurrently
  writable by design.
- **Valid reports:** A report must demonstrate a concrete security violation in
  the malicious node's peer process. Examples include:
  - **Memory corruption outside shared memory:** Missing bounds checks or
    integer overflows when resolving fragment descriptors or copying data into
    process-private heap/stack memory.
  - **Double-fetch / TOCTOU:** Reading untrusted shared memory values multiple
    times, where a concurrent write by the peer invalidates earlier checks.
  - **Privileged DoS / Hangs:** Causing a malicious node's peer (e.g. the
    browser process) to crash, panic, or enter an infinite loop.
  - **Cross-link data exposure:** Breaking node link isolation to inspect or
    corrupt communications belonging to an unrelated node pair.
