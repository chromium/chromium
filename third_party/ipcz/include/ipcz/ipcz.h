// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_INCLUDE_IPCZ_IPCZ_H_
#define IPCZ_INCLUDE_IPCZ_IPCZ_H_

// ipcz is a cross-platform C library for interprocess communication (IPC) which
// supports efficient routing and data transfer over a large number of
// dynamically relocatable messaging endpoints.
//
// ipcz operates in terms of a small handful of abstractions encapsulated in
// this header: nodes, portals, parcels, drivers, boxes, and traps.
//
// NOTE: This header is intended to compile under C++11 or newer, and C99 or
// newer. The ABI defined here can be considered stable.
//
// Glossary
// --------
// *Nodes* are used by ipcz to model isolated units of an application. A typical
// application will create one ipcz node within each OS process it controls.
//
// *Portals* are messaging endpoints which belong to a specific node. Portals
// are created in entangled pairs: whatever goes into one portal comes out the
// other (its "peer"). Pairs may be created local to a single node, or they may
// be created to span two nodes. Portals may also be transferred freely through
// other portals.
//
// *Parcels* are the unit of communication between portals. Parcels can contain
// arbitrary application data as well as ipcz handles. Handles within a parcel
// are used to transfer objects (namely other portals, or driver-defined
// objects) from one portal to another, potentially on a different node.
//
// *Traps* provide a flexible mechanism to observe interesting portal state
// changes such as new parcels arriving or a portal's peer being closed.
//
// *Drivers* are provided by applications to implement platform-specific IPC
// details. They may also define new types of objects to be transmitted in
// parcels via boxes.
//
// *Boxes* are opaque objects used to wrap driver- or application-defined
// objects for seamless transmission across portals. Applications use the Box()
// and Unbox() APIs to go between concrete objects and transferrable box
// handles, and ipcz delegates to the driver or application to serialize boxed
// objects as needed for transmission.
//
// Overview
// --------
// To use ipcz effectively, an application must create multiple nodes to be
// interconnected. One node must be designated as the "broker" by the
// application (see CreateNode() flags). The broker is used by ipcz to
// coordinate certain types of internal transactions which demand a heightened
// level of trust and capability, so a broker node should always live in a more
// trustworthy process. For example in Chrome, the browser process is
// designated as the broker.
//
// In order for a node to communicate with other nodes in the system, the
// application must explicitly connect it to ONE other node using the
// ConnectNode() API. Once this is done, ipcz can automatically connect the node
// to additional other nodes as needed for efficient portal operation.
//
// In the example below, assume node A is designated as the broker. Nodes A and
// B have been connected directly by ConnectNode() calls in the application.
// Nodes A and C have been similarly connected:
//
//                    ┌───────┐
//     ConnectNode()  │       │  ConnectNode()
//        ┌──────────>O   A   O<───────────┐
//        │           │       │            │
//        │           └───────┘            │
//        │                                │
//        v ConnectNode()                  v ConnectNode()
//    ┌───O───┐                        ┌───O───┐
//    │       │                        │       │
//    │   B   │                        │   C   │
//    │       │                        │       │
//    └───────┘                        └───────┘
//
// ConnectNode() establishes initial portal pairs to link the two nodes
// together, illustrated above as "O"s. Once ConnectNode() returns, the
// application may immediately begin transmitting parcels through these portals.
//
// Now suppose node B creates a new local pair of portals (using OpenPortals())
// and sends one of those new portals in a parcel through its
// already-established portal to node A. The sent portal is effectively
// transferred to node A, and because its entangled peer still lives on node B
// there are now TWO portal pairs between nodes A and B:
//
//                    ┌───────┐
//                    │       │
//        ┌──────────>O   A   O<───────────┐
//        │ ┌────────>O       │            │
//        │ │         └───────┘            │
//        │ │                              │
//        v v                              v
//    ┌───O─O─┐                        ┌───O───┐
//    │       │                        │       │
//    │   B   │                        │   C   │
//    │       │                        │       │
//    └───────┘                        └───────┘
//
// Finally, suppose now the application takes this new portal on node A and
// sends it further along, through node A's already-established portal to node
// C. Because the transferred portal's peer still lives on node B, there is now
// a portal pair spanning nodes B and C:
//
//                    ┌───────┐
//                    │       │
//        ┌──────────>O   A   O<───────────┐
//        │           │       │            │
//        │           └───────┘            │
//        │                                │
//        v                                v
//    ┌───O───┐                        ┌───O───┐
//    │       │                        │       │
//    │   B   O────────────────────────O   C   │
//    │       │                        │       │
//    └───────┘                        └───────┘
//
// These two nodes were never explicitly connected by the application, but ipcz
// ensures that the portals will operate as expected. Behind the scenes, ipcz
// achieves this by establishing a direct, secure, and efficient communication
// channel between nodes B and C.

#include <stddef.h>
#include <stdint.h>

#define IPCZ_NO_FLAGS ((uint32_t)0)

// Helper to clarify flag definitions.
#define IPCZ_FLAG_BIT(bit) ((uint32_t)(1u << bit))

// Opaque handle to an ipcz object.
typedef uintptr_t IpczHandle;

// An IpczHandle value which is always invalid. Note that arbitrary non-zero
// values are not necessarily valid either, but zero is never valid.
#define IPCZ_INVALID_HANDLE ((IpczHandle)0)

// Generic result code for all ipcz operations. See IPCZ_RESULT_* values below.
typedef int IpczResult;

// Specific meaning of each value depends on context, but IPCZ_RESULT_OK always
// indicates success. These values are derived from common status code
// definitions across Google software.
#define IPCZ_RESULT_OK ((IpczResult)0)
#define IPCZ_RESULT_CANCELLED ((IpczResult)1)
#define IPCZ_RESULT_UNKNOWN ((IpczResult)2)
#define IPCZ_RESULT_INVALID_ARGUMENT ((IpczResult)3)
#define IPCZ_RESULT_DEADLINE_EXCEEDED ((IpczResult)4)
#define IPCZ_RESULT_NOT_FOUND ((IpczResult)5)
#define IPCZ_RESULT_ALREADY_EXISTS ((IpczResult)6)
#define IPCZ_RESULT_PERMISSION_DENIED ((IpczResult)7)
#define IPCZ_RESULT_RESOURCE_EXHAUSTED ((IpczResult)8)
#define IPCZ_RESULT_FAILED_PRECONDITION ((IpczResult)9)
#define IPCZ_RESULT_ABORTED ((IpczResult)10)
#define IPCZ_RESULT_OUT_OF_RANGE ((IpczResult)11)
#define IPCZ_RESULT_UNIMPLEMENTED ((IpczResult)12)
#define IPCZ_RESULT_INTERNAL ((IpczResult)13)
#define IPCZ_RESULT_UNAVAILABLE ((IpczResult)14)
#define IPCZ_RESULT_DATA_LOSS ((IpczResult)15)

// Helper to specify explicit struct alignment across C and C++ compilers.
#if defined(__cplusplus)
#define IPCZ_ALIGN(alignment) alignas(alignment)
#elif defined(__GNUC__)
#define IPCZ_ALIGN(alignment) __attribute__((aligned(alignment)))
#elif defined(_MSC_VER)
#define IPCZ_ALIGN(alignment) __declspec(align(alignment))
#else
#error "IPCZ_ALIGN() is not defined for your compiler."
#endif

// Helper to generate the smallest constant value which is aligned with
// `alignment` and at least as large as `value`.
#define IPCZ_ALIGNED(value, alignment) \
  ((((value) + ((alignment)-1)) / (alignment)) * (alignment))

// Helper used to explicitly specify calling convention or other
// compiler-specific annotations for each API function.
#if defined(IPCZ_API_OVERRIDE)
#define IPCZ_API IPCZ_API_OVERRIDE
#elif defined(_WIN32)
#define IPCZ_API __cdecl
#else
#define IPCZ_API
#endif

// An opaque handle value created by an IpczDriver implementation. ipcz uses
// such handles to provide relevant context when calling back into the driver.
typedef uintptr_t IpczDriverHandle;

#define IPCZ_INVALID_DRIVER_HANDLE ((IpczDriverHandle)0)

// Flags given to the ipcz activity handler by a driver transport to notify ipcz
// about incoming data or state changes.
typedef uint32_t IpczTransportActivityFlags;

// If set, the driver encountered an unrecoverable error using the transport and
// ipcz should discard it. Note that the driver is free to issue such
// notifications many times as long as it remans active, but ipcz will generally
// request deactivation ASAP once an error is signaled.
#define IPCZ_TRANSPORT_ACTIVITY_ERROR IPCZ_FLAG_BIT(0)

// When ipcz wants to deactivate a transport, it invokes the driver's
// DeactivateTransport() function. Once the driver has finished any clean up and
// can ensure that the transport's activity handler will no longer be invoked,
// it must then invoke the activity handler one final time with this flag set.
// This finalizes deactivation and allows ipcz to free any associated resources.
#define IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED IPCZ_FLAG_BIT(1)

#if defined(__cplusplus)
extern "C" {
#endif

// Notifies ipcz of activity on a transport. `transport` must be a handle to a
// transport which is currently activated. The `transport` handle is acquired
// exclusively by the driver transport via an ipcz call to the driver's
// ActivateTransport(), which also provides the handle to the driver.
//
// The driver must use this function to feed incoming data and driver handles
// from the transport to ipcz, or to inform ipcz of any error conditions
// resulting in unexpected and irrecoverable dysfunction of the transport.
//
// If the driver encounters an unrecoverable error while performing I/O on the
// transport, it should invoke this with the IPCZ_TRANSPORT_ACTIVITY_ERROR flag
// to instigate deactivation of the transport by ipcz via a subsequent
// DeactivateTransport() call.
//
// `options` is currently unused and must be null.
//
// NOTE: It is the driver's responsibility to ensure that calls to this function
// for the same value of `transport` are mututally exclusive. Overlapping calls
// are unsafe and will result in undefined behavior.
typedef IpczResult(IPCZ_API* IpczTransportActivityHandler)(
    IpczHandle transport,                    // in
    const void* data,                        // in
    size_t num_bytes,                        // in
    const IpczDriverHandle* driver_handles,  // in
    size_t num_driver_handles,               // in
    IpczTransportActivityFlags flags,        // in
    const void* options);                    // in

// Structure to be filled in by a driver's GetSharedMemoryInfo().
struct IPCZ_ALIGN(8) IpczSharedMemoryInfo {
  // The exact size of this structure in bytes. Set by ipcz before passing the
  // structure to the driver.
  size_t size;

  // The size of the shared memory region in bytes.
  size_t region_num_bytes;
};

// IpczDriver
// ==========
//
// IpczDriver is a function table to be populated by the application and
// provided to ipcz when creating a new node. The driver implements concrete
// I/O operations to facilitate communication between nodes, giving embedding
// systems full control over choice of OS-specific transport mechanisms and I/O
// scheduling decisions.
//
// The driver API is meant to be used by both the application embedding ipcz,
// particularly for creating transports to make initial contact between nodes,
// as well as by ipcz itself to delegate creation and management of new
// transports which ipcz brokers between nodes.
struct IPCZ_ALIGN(8) IpczDriver {
  // The exact size of this structure in bytes. Must be set accurately by the
  // application before passing this structure to any ipcz API functions.
  size_t size;

  // Close()
  // =======
  //
  // Called by ipcz to request that the driver release the object identified by
  // `handle`.
  IpczResult(IPCZ_API* Close)(IpczDriverHandle handle,  // in
                              uint32_t flags,           // in
                              const void* options);     // in

  // Serialize()
  // ===========
  //
  // Serializes a driver object identified by `handle` into a collection of
  // bytes and readily transmissible driver objects, for eventual transmission
  // over `transport`. At a minimum this must support serialization of transport
  // and memory objects allocated by ipcz through the driver. Any other driver
  // objects intended for applications to box and transmit through portals must
  // also be serializable here.
  //
  // If the specified object is invalid or unserializable, the driver must
  // ignore all other arguments (including `transport`) and return
  // IPCZ_RESULT_INVALID_ARGUMENT.
  //
  // If the object can be serialized but success may depend on the value of
  // `transport`, and `transport` is IPCZ_INVALID_DRIVER_HANDLE, the driver must
  // return IPCZ_RESULT_ABORTED. ipcz may invoke Serialize() in this way to
  // query whether a specific object can be serialized at all, even when it
  // doesn't have a specific transport in mind.
  //
  // If the object can be serialized or transmitted as-is and `transport` is
  // valid, but the serialized outputs would not be transmissible over
  // `transport` specifically, the driver must ignore all other arguments and
  // return IPCZ_RESULT_PERMISSION_DENIED. This implies that neither end of
  // `transport` is sufficiently privileged or otherwise able to transfer the
  // object directly, and in this case ipcz may instead attempt to relay the
  // object through a more capable broker node.
  //
  // For all other outcomes, the object identified by `handle` is considered to
  // be serializable and ultimately transmissible.
  //
  // `num_bytes` and `num_handles` on input point to the capacities of the
  // respective output buffers provided by ipcz. If either capacity pointer is
  // null, a capacity of zero is implied; and if either input capacity is zero,
  // the corresponding input buffer may be null.
  //
  // Except in the failure modes described above, the driver must update any
  // non-null capacity input to reflect the exact capacity required to serialize
  // the object. For example if `num_bytes` is non-null and the object
  // serializes to 8 bytes of data, `*num_bytes` must be set to 8 upon return.
  //
  // If the required data or handle capacity is larger than the respective input
  // capacity, the driver must return IPCZ_RESULT_RESUORCE_EXHAUSTED without
  // modifying the contents of either `data` or `handles` buffers.
  //
  // Finally, if the input capacities were both sufficient, the driver must fill
  // `data` and `handles` with a serialized representation of the object and
  // return IPCZ_RESULT_OK. In this case ipcz relinquishes `handle` and will no
  // longer refer to it unless the driver outputs it back in `handles`, implying
  // that it was already transmissible as-is.
  IpczResult(IPCZ_API* Serialize)(IpczDriverHandle handle,     // in
                                  IpczDriverHandle transport,  // in
                                  uint32_t flags,              // in
                                  const void* options,         // in
                                  void* data,                  // out
                                  size_t* num_bytes,           // in/out
                                  IpczDriverHandle* handles,   // out
                                  size_t* num_handles);        // in/out

  // Deserialize()
  // =============
  //
  // Deserializes a driver object from a collection of bytes and transmissible
  // driver objects which which was originally produced by Serialize() and
  // received on the calling node via `transport`.
  //
  // Any return value other than IPCZ_RESULT_OK indicates an error and implies
  // that `handle` is unmodified. Otherwise `handle` must contain a valid driver
  // handle to the deserialized object.
  IpczResult(IPCZ_API* Deserialize)(
      const void* data,                        // in
      size_t num_bytes,                        // in
      const IpczDriverHandle* driver_handles,  // in
      size_t num_driver_handles,               // in
      IpczDriverHandle transport,              // in
      uint32_t flags,                          // in
      const void* options,                     // in
      IpczDriverHandle* handle);               // out

  // CreateTransports()
  // ==================
  //
  // Creates a new pair of entangled bidirectional transports, returning them in
  // `new_transport0` and `new_transport1`.
  //
  // The input `transport0` and `transport1` are provided for context which may
  // be important to the driver: the output transport in `new_transport0` will
  // be sent over `transport0`, while the output transport in `new_transport1`
  // will be sent over `transport1`. That is, the new transport is being created
  // to establish a direct link between the remote node on `transport0` and the
  // remote node on `transport1`.
  //
  // Any return value other than IPCZ_RESULT_OK indicates an error and implies
  // that `new_transport0` and `new_transport1` are unmodified.
  IpczResult(IPCZ_API* CreateTransports)(
      IpczDriverHandle transport0,        // in
      IpczDriverHandle transport1,        // in
      uint32_t flags,                     // in
      const void* options,                // in
      IpczDriverHandle* new_transport0,   // out
      IpczDriverHandle* new_transport1);  // out

  // ActivateTransport()
  // ===================
  //
  // Called by ipcz to activate a transport. `driver_transport` is the
  // driver-side handle assigned to the transport by the driver, either as given
  // to ipcz via ConnectNode(), or as returned by the driver from an ipcz call
  // out to CreateTransports().
  //
  // `transport` is a handle the driver can use when calling `activity_handler`
  // to update ipcz regarding any incoming data or state changes from the
  // transport.
  //
  // Before this returns, the driver should establish any I/O monitoring or
  // scheduling state necessary to support operation of the endpoint, and once
  // it returns ipcz may immediately begin making Transmit() calls on
  // `driver_transport`.
  //
  // Any return value other than IPCZ_RESULT_OK indicates an error, and the
  // endpoint will be dropped by ipcz. Otherwise the endpoint may be used
  // immediately to accept or submit data, and it should continue to operate
  // until ipcz calls Close() on `driver_transport`.
  //
  // Note that `activity_handler` invocations MUST be mutually exclusive,
  // because transmissions from ipcz are expected to arrive and be processed
  // strictly in-order.
  //
  // The driver may elicit forced destruction of itself by calling
  // `activity_handler` with the flag IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED.
  IpczResult(IPCZ_API* ActivateTransport)(
      IpczDriverHandle driver_transport,              // in
      IpczHandle transport,                           // in
      IpczTransportActivityHandler activity_handler,  // in
      uint32_t flags,                                 // in
      const void* options);                           // in

  // DeactivateTransport()
  // =====================
  //
  // Called by ipcz to deactivate a transport. The driver does not need to
  // complete deactivation synchronously, but it must begin to deactivate the
  // transport and must invoke the transport's activity handler one final time
  // with IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED once finished. Beyond that point,
  // the activity handler must no longer be invoked for that transport.
  //
  // Note that even after deactivatoin, ipcz may continue to call into the
  // transport until it's closed with an explicit call to the driver's Close()
  // by ipcz.
  IpczResult(IPCZ_API* DeactivateTransport)(
      IpczDriverHandle driver_transport,  // in
      uint32_t flags,                     // in
      const void* options);               // in

  // Transmit()
  // ==========
  //
  // Called by ipcz to delegate transmission of data and driver handles over the
  // identified transport endpoint. If the driver cannot fulfill the request,
  // it must return a result other than IPCZ_RESULT_OK, and this will cause the
  // transport's connection to be severed.
  //
  // Note that any driver handles in `driver_handles` were obtained by ipcz from
  // the driver itself, by some prior call to the driver's own Serialize()
  // function. These handles are therefore expected to be directly transmissible
  // by the driver alongside any data in `data`.
  //
  // The net result of this transmission should be an activity handler
  // invocation on the corresponding remote transport by the driver on its node.
  // It is the driver's responsibility to get any data and handles to the other
  // transport, and to ensure that all transmissions from transport end up
  // invoking the activity handler on the peer transport in the same order they
  // were transmitted.
  //
  // If ipcz only wants to wake the peer node rather than transmit data or
  // handles, `num_bytes` and `num_driver_handles` may both be zero.
  IpczResult(IPCZ_API* Transmit)(IpczDriverHandle driver_transport,       // in
                                 const void* data,                        // in
                                 size_t num_bytes,                        // in
                                 const IpczDriverHandle* driver_handles,  // in
                                 size_t num_driver_handles,               // in
                                 uint32_t flags,                          // in
                                 const void* options);                    // in

  // ReportBadTransportActivity()
  // ============================
  //
  // The ipcz Reject() API can be used by an application to reject a specific
  // parcel received from a portal. If the parcel in question came from a
  // remote node, ipcz invokes ReportBadTransportActivity() to notify the driver
  // about the `transport` which delivered the rejected parcel.
  //
  // `context` is an opaque value passed by the application to the Reject() call
  // which elicited this invocation.
  IpczResult(IPCZ_API* ReportBadTransportActivity)(IpczDriverHandle transport,
                                                   uintptr_t context,
                                                   uint32_t flags,
                                                   const void* options);

  // AllocateSharedMemory()
  // ======================
  //
  // Allocates a shared memory region and returns a driver handle in
  // `driver_memory` which can be used to reference it in other calls to the
  // driver.
  IpczResult(IPCZ_API* AllocateSharedMemory)(
      size_t num_bytes,                  // in
      uint32_t flags,                    // in
      const void* options,               // in
      IpczDriverHandle* driver_memory);  // out

  // GetSharedMemoryInfo()
  // =====================
  //
  // Returns information about the shared memory region identified by
  // `driver_memory`.
  IpczResult(IPCZ_API* GetSharedMemoryInfo)(
      IpczDriverHandle driver_memory,      // in
      uint32_t flags,                      // in
      const void* options,                 // in
      struct IpczSharedMemoryInfo* info);  // out

  // DuplicateSharedMemory()
  // =======================
  //
  // Duplicates a shared memory region handle into a new distinct handle
  // referencing the same underlying region.
  IpczResult(IPCZ_API* DuplicateSharedMemory)(
      IpczDriverHandle driver_memory,        // in
      uint32_t flags,                        // in
      const void* options,                   // in
      IpczDriverHandle* new_driver_memory);  // out

  // MapSharedMemory()
  // =================
  //
  // Maps a shared memory region identified by `driver_memory` and returns its
  // mapped address in `address` on success and a driver handle in
  // `driver_mapping` which can be passed to the driver's Close() to unmap the
  // region later.
  //
  // Note that the lifetime of `driver_mapping` should be independent from that
  // of `driver_memory`. That is, if `driver_memory` is closed immediately after
  // this call succeeds, the returned mapping must still remain valid until the
  // mapping itself is closed.
  IpczResult(IPCZ_API* MapSharedMemory)(
      IpczDriverHandle driver_memory,     // in
      uint32_t flags,                     // in
      const void* options,                // in
      void** address,                     // out
      IpczDriverHandle* driver_mapping);  // out

  // GenerateRandomBytes()
  // =====================
  //
  // Generates `num_bytes` bytes of random data to fill `buffer`.
  IpczResult(IPCZ_API* GenerateRandomBytes)(size_t num_bytes,     // in
                                            uint32_t flags,       // in
                                            const void* options,  // in
                                            void* buffer);        // out
};

#if defined(__cplusplus)
}  // extern "C"
#endif

// Flags which may be passed via the `memory_flags` field of
// IpczCreateNodeOptions to configure features of ipcz internal memory
// allocation and usage.
typedef uint32_t IpczMemoryFlags;

// If this flag is set, the node will not attempt to expand the shared memory
// pools it uses to allocate parcel data between itself and other nodes.
//
// This means more application messages may fall back onto driver I/O for
// transmission, but also that ipcz' memory footprint will remain largely
// constant. Note that memory may still be expanded to facilitate new portal
// links as needed.
#define IPCZ_MEMORY_FIXED_PARCEL_CAPACITY ((IpczMemoryFlags)(1 << 0))

// Options given to CreateNode() to configure the new node's behavior.
struct IPCZ_ALIGN(8) IpczCreateNodeOptions {
  // The exact size of this structure in bytes. Must be set accurately before
  // passing the structure to CreateNode().
  size_t size;

  // See IpczMemoryFlags above.
  IpczMemoryFlags memory_flags;
};

// See CreateNode() and the IPCZ_CREATE_NODE_* flag descriptions below.
typedef uint32_t IpczCreateNodeFlags;

// Indicates that the created node will serve as the broker in its cluster.
//
// Brokers are expected to live in relatively trusted processes -- not
// necessarily elevated in privilege but also generally not restricted by
// sandbox constraints and not prone to processing risky, untrusted data -- as
// they're responsible for helping other nodes establish direct lines of
// communication as well as in some cases relaying data and driver handles
// between lesser-privileged nodes.
//
// Broker nodes do not expose any additional ipcz APIs or require much other
// special care on the part of the application**, but every cluster of connected
// nodes must have a node designated as the broker. Typically this is the first
// node created by an application's main process or a system-wide service
// coordinator, and all other nodes are created in processes spawned by that one
// or in processes which otherwise trust it.
//
// ** See notes on Close() regarding destruction of broker nodes.
#define IPCZ_CREATE_NODE_AS_BROKER IPCZ_FLAG_BIT(0)

// See ConnectNode() and the IPCZ_CONNECT_NODE_* flag descriptions below.
typedef uint32_t IpczConnectNodeFlags;

// Indicates that the remote node for this connection is expected to be a broker
// node, and it will be treated as such. Do not use this flag when connecting to
// any untrusted process.
#define IPCZ_CONNECT_NODE_TO_BROKER IPCZ_FLAG_BIT(0)

// Indicates that the remote node for this connection is expected not to be a
// broker, but to already have a link to a broker; and that the calling node
// wishes to inherit the remote node's broker as well. This flag must only be
// used when connecting to a node the caller trusts. The calling node must not
// already have an established broker from a previous ConnectNode() call. The
// remote node must specify IPCZ_CONNECT_NODE_SHARE_BROKER as well.
#define IPCZ_CONNECT_NODE_INHERIT_BROKER IPCZ_FLAG_BIT(1)

// Indicates that the calling node already has a broker, and that this broker
// will be inherited by the remote node. The remote node must also specify
// IPCZ_CONNECT_NODE_INHERIT_BROKER in its corresponding ConnectNode() call.
#define IPCZ_CONNECT_NODE_SHARE_BROKER IPCZ_FLAG_BIT(2)

// ipcz may periodically allocate shared memory regions to facilitate
// communication between two nodes. In many runtime environments, even within
// a security sandbox, the driver can do do this safely and directly by
// interfacing with the OS. In some environments however, direct allocation is
// not possible. In such cases a node must delegate this responsibility to some
// other trusted node in the system, typically the broker node.
//
// Specifying this flag ensures that all shared memory allocation elicited by
// the connecting node will be delegated to the connectee.
#define IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE IPCZ_FLAG_BIT(3)

// Optional limits provided by IpczPutOptions for Put() or IpczBeginPutOptions
// for BeginPut().
struct IPCZ_ALIGN(8) IpczPutLimits {
  // The exact size of this structure in bytes. Must be set accurately before
  // passing the structure to any API functions.
  size_t size;

  // Specifies the maximum number of unread parcels to allow in a portal's
  // queue. If a Put() or BeginPut() call specifying this limit would cause the
  // receiver's number of number of queued unread parcels to exceed this value,
  // the call will fail with IPCZ_RESULT_RESOURCE_EXHAUSTED.
  size_t max_queued_parcels;

  // Specifies the maximum number of data bytes to allow in a portal's queue.
  // If a Put() or BeginPut() call specifying this limit would cause the number
  // of data bytes across all queued unread parcels to exceed this value, the
  // call will fail with IPCZ_RESULT_RESOURCE_EXHAUSTED.
  size_t max_queued_bytes;
};

// Options given to Put() to modify its default behavior.
struct IPCZ_ALIGN(8) IpczPutOptions {
  // The exact size of this structure in bytes. Must be set accurately before
  // passing the structure to Put().
  size_t size;

  // Optional limits to apply when determining if the Put() should be completed.
  const struct IpczPutLimits* limits;
};

// See BeginPut() and the IPCZ_BEGIN_PUT_* flags described below.
typedef uint32_t IpczBeginPutFlags;

// Indicates that the caller is willing to produce less data than originally
// requested by their `*num_bytes` argument to BeginPut(). If the implementation
// would prefer a smaller chunk of data or if the requested size would exceed
// limits specified in the call's corresponding IpczPutLimits, passing this flag
// may allow the call to succeed while returning a smaller acceptable value in
// `*num_bytes`, rather than simply failing the call with
// IPCZ_RESULT_RESOURCE_EXHAUSTED.
#define IPCZ_BEGIN_PUT_ALLOW_PARTIAL IPCZ_FLAG_BIT(0)

// Options given to BeginPut() to modify its default behavior.
struct IPCZ_ALIGN(8) IpczBeginPutOptions {
  // The exact size of this structure in bytes. Must be set accurately before
  // passing the structure to BeginPut().
  size_t size;

  // Optional limits to apply when determining if the BeginPut() should be
  // completed.
  const struct IpczPutLimits* limits;
};

// See EndPut() and the IPCZ_END_PUT_* flags described below.
typedef uint32_t IpczEndPutFlags;

// If this flag is given to EndPut(), any in-progress two-phase put operation is
// aborted without committing a parcel to the portal.
#define IPCZ_END_PUT_ABORT IPCZ_FLAG_BIT(0)

// See Get() and the IPCZ_GET_* flag descriptions below.
typedef uint32_t IpczGetFlags;

// When given to Get(), this flag indicates that the caller is willing to accept
// a partial retrieval of the next available parcel. This means that in
// situations where Get() would normally return IPCZ_RESULT_RESOURCE_EXHAUSTED,
// it will instead return IPCZ_RESULT_OK with as much data and handles as the
// caller indicated they could accept. This flag may not be specified if
// IPCZ_GET_PARCEL_ONLY is specified.
#define IPCZ_GET_PARTIAL IPCZ_FLAG_BIT(0)

// When given to Get() and a parcel is available to consume from the referenced
// portal, no data or handles are consumed from the available parcel. Instead
// only a handle to the parcel is returned, and the parcel is removed from the
// portal to allow subsequent parcels to be retrieved. See documentation on
// Get(). This flag may not be specified if IPCZ_GET_PARTIAL is specified.
#define IPCZ_GET_PARCEL_ONLY IPCZ_FLAG_BIT(1)

// See EndGet() and the IPCZ_END_GET_* flag descriptions below.
typedef uint32_t IpczEndGetFlags;

// If this flag is given to EndGet(), any in-progress two-phase get operation is
// aborted without consuming any data from the portal.
#define IPCZ_END_GET_ABORT IPCZ_FLAG_BIT(0)

// Enumerates the type of contents within a box.
typedef uint32_t IpczBoxType;

// A box which contains an opaque driver object.
#define IPCZ_BOX_TYPE_DRIVER_OBJECT ((IpczBoxType)0)

// A box which contains an opaque application-defined object with a custom
// serializer.
#define IPCZ_BOX_TYPE_APPLICATION_OBJECT ((IpczBoxType)1)

// A box which contains a collection of bytes and ipcz handles.
#define IPCZ_BOX_TYPE_SUBPARCEL ((IpczBoxType)2)

// A function passed to Box() when boxing application objects. This function
// implements serialization for the object identified by `object` in a manner
// similar to IpczDriver's Serialize() function. If the object is not
// serializable this must return IPCZ_RESULT_FAILED_PRECONDITION and ignore
// other arguments.
//
// This may be called with insufficient capacity (`num_bytes` and `num_handles`)
// in which case it should update those values with capacity requirements and
// return IPCZ_RESULT_RESOURCE_EXHAUSTED.
//
// Otherwise the function must fill in `bytes` and `handles` with values that
// effectively represent the opaque object identified by `object`, and return
// IPCZ_RESULT_OK to indicate success.
typedef IpczResult (*IpczApplicationObjectSerializer)(uintptr_t object,
                                                      uint32_t flags,
                                                      const void* options,
                                                      void* data,
                                                      size_t* num_bytes,
                                                      IpczHandle* handles,
                                                      size_t* num_handles);

// A function passed to Box() when boxing application objects. This function
// must clean up any resources associated with the opaque object identified by
// `object`.
typedef void (*IpczApplicationObjectDestructor)(uintptr_t object,
                                                uint32_t flags,
                                                const void* options);

// Describes the contents of a box. Boxes may contain driver objects, arbitrary
// application-defined objects, or collections of bytes and ipcz handles
// (portals or other boxes).
struct IPCZ_ALIGN(8) IpczBoxContents {
  // The exact size of this structure in bytes. Must be set accurately before
  // passing the structure to Box() or Unbox().
  size_t size;

  // The type of contents contained in the box.
  IpczBoxType type;

  union {
    // A handle to a driver object, when `type` is IPCZ_BOX_TYPE_DRIVER_OBJECT.
    IpczDriverHandle driver_object;

    // An opaque object identifier which has meaning to `serializer` and
    // `destructor` when `type` is IPCZ_BOX_TYPE_APPLICATION_OBJECT.
    uintptr_t application_object;

    // A handle to an ipcz parcel object referencing the box contents. This may
    // be used with Get()/BeginGet()/EndGet() APIs to extract its serialized
    // data and handles. Used for boxes of type IPCZ_BOX_TYPE_SUBPARCEL.
    IpczHandle subparcel;
  } object;

  // Used only for IPCZ_BOX_TYPE_APPLICATION_OBJECT. ipcz may use these
  // functions to serialize and/or destroy the opaque object identified by
  // `object.application_object` above.
  IpczApplicationObjectSerializer serializer;
  IpczApplicationObjectDestructor destructor;
};

// See Unbox() and the IPCZ_UNBOX_* flags described below.
typedef uint32_t IpczUnboxFlags;

// If set, the box is not consumed and the driver handle returned is not removed
// from the box.
#define IPCZ_UNBOX_PEEK IPCZ_FLAG_BIT(0)

// Flags given by the `flags` field in IpczPortalStatus.
typedef uint32_t IpczPortalStatusFlags;

// Indicates that the opposite portal is closed. Subsequent put operations on
// this portal will always fail with IPCZ_RESULT_NOT_FOUND. If there are not
// currently any unretrieved parcels in the portal either, subsequent get
// operations will also fail with the same error.
#define IPCZ_PORTAL_STATUS_PEER_CLOSED IPCZ_FLAG_BIT(0)

// Indicates that the opposite portal is closed AND no more parcels can be
// expected to arrive from it. If this bit is set on a portal's status, the
// portal is essentially useless. Such portals no longer support Put() or
// Get() operations, and those operations will subsequently always return
// IPCZ_RESULT_NOT_FOUND.
#define IPCZ_PORTAL_STATUS_DEAD IPCZ_FLAG_BIT(1)

// Information returned by QueryPortalStatus() or provided to
// IpczTrapEventHandlers when a trap's conditions are met on a portal.
struct IPCZ_ALIGN(8) IpczPortalStatus {
  // The exact size of this structure in bytes. Must be set accurately before
  // passing the structure to any functions.
  size_t size;

  // Flags. See the IPCZ_PORTAL_STATUS_* flags described above for the possible
  // flags combined in this value.
  IpczPortalStatusFlags flags;

  // The number of unretrieved parcels queued on this portal.
  size_t num_local_parcels;

  // The number of unretrieved bytes (across all unretrieved parcels) queued on
  // this portal.
  size_t num_local_bytes;

  // The number of unretrieved parcels queued on the opposite portal.
  size_t num_remote_parcels;

  // The number of unretrieved bytes (across all unretrieved parcels) queued on
  // the opposite portal.
  size_t num_remote_bytes;
};

// Flags given to IpczTrapConditions to indicate which types of conditions a
// trap should observe.
//
// Note that each type of condition may be considered edge-triggered or
// level-triggered. An edge-triggered condition is one which is only
// observable momentarily in response to a state change, while a level-triggered
// condition is continuously observable as long as some constraint about a
// portal's state is met.
//
// Level-triggered conditions can cause a Trap() attempt to fail if they're
// already satisfied when attempting to install a trap to monitor them.
typedef uint32_t IpczTrapConditionFlags;

// Triggers a trap event when the trap's portal is itself closed. This condition
// is always observed even if not explicitly set in the IpczTrapConditions given
// to the Trap() call. If a portal is closed while a trap is installed on it,
// an event will fire for the trap with this condition flag set. This condition
// is effectively edge-triggered, because as soon as it becomes true, any
// observing trap as well as its observed subject cease to exist.
#define IPCZ_TRAP_REMOVED IPCZ_FLAG_BIT(0)

// Triggers a trap event whenever the opposite portal is closed. Typically
// applications are interested in the more specific IPCZ_TRAP_DEAD.
// Level-triggered.
#define IPCZ_TRAP_PEER_CLOSED IPCZ_FLAG_BIT(1)

// Triggers a trap event whenever there are no more parcels available to
// retrieve from this portal AND the opposite portal is closed. This means the
// portal will never again have parcels to retrieve and is effectively useless.
// Level-triggered.
#define IPCZ_TRAP_DEAD IPCZ_FLAG_BIT(2)

// Triggers a trap event whenever the number of parcels queued for retrieval by
// this portal exceeds the threshold given by `min_local_parcels` in
// IpczTrapConditions. Level-triggered.
#define IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS IPCZ_FLAG_BIT(3)

// Triggers a trap event whenever the number of bytes queued for retrieval by
// this portal exceeds the threshold given by `min_local_bytes` in
// IpczTrapConditions. Level-triggered.
#define IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES IPCZ_FLAG_BIT(4)

// Triggers a trap event whenever the number of parcels queued for retrieval on
// the opposite portal drops below the threshold given by `max_remote_parcels`
// IpczTrapConditions. Level-triggered.
#define IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS IPCZ_FLAG_BIT(5)

// Triggers a trap event whenever the number of bytes queued for retrieval on
// the opposite portal drops below the threshold given by `max_remote_bytes` in
// IpczTrapConditions. Level-triggered.
#define IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES IPCZ_FLAG_BIT(6)

// Triggers a trap event whenever the number of locally available parcels
// increases by any amount. Edge-triggered.
#define IPCZ_TRAP_NEW_LOCAL_PARCEL IPCZ_FLAG_BIT(7)

// Triggers a trap event whenever the number of queued remote parcels decreases
// by any amount. Edge-triggered.
#define IPCZ_TRAP_CONSUMED_REMOTE_PARCEL IPCZ_FLAG_BIT(8)

// Indicates that the trap event is being fired from within the extent of an
// ipcz API call (i.e., as opposed to being fired from within the extent of an
// incoming driver transport notification.) For example if a trap is monitoring
// a portal for incoming parcels, and the application puts a parcel into the
// portal's peer on the same node, the trap event will be fired within the
// extent of the corresponding Put() call, and this flag will be set on the
// event.
//
// This flag is ignored when specifying conditions to watch for Trap(), and it
// may be set on any event dispatched to an IpczTrapEventHandler.
#define IPCZ_TRAP_WITHIN_API_CALL IPCZ_FLAG_BIT(9)

// A structure describing portal conditions necessary to trigger a trap and
// invoke its event handler.
struct IPCZ_ALIGN(8) IpczTrapConditions {
  // The exact size of this structure in bytes. Must be set accurately before
  // passing the structure to Trap().
  size_t size;

  // See the IPCZ_TRAP_* flags described above.
  IpczTrapConditionFlags flags;

  // See IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS. If that flag is not set in `flags`,
  // this field is ignord.
  size_t min_local_parcels;

  // See IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES. If that flag is not set in `flags`,
  // this field is ignored.
  size_t min_local_bytes;

  // See IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS. If that flag is not set in `flags`,
  // this field is ignored.
  size_t max_remote_parcels;

  // See IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES. If that flag is not set in `flags`,
  // this field is ignored.
  size_t max_remote_bytes;
};

// Structure passed to each IpczTrapEventHandler invocation with details about
// the event.
struct IPCZ_ALIGN(8) IpczTrapEvent {
  // The size of this structure in bytes. Populated by ipcz to indicate which
  // version is being provided to the handler.
  size_t size;

  // The context value that was given to Trap() when installing the trap that
  // fired this event.
  uintptr_t context;

  // Flags indicating which condition(s) triggered this event.
  IpczTrapConditionFlags condition_flags;

  // The current status of the portal which triggered this event. This address
  // is only valid through the extent of the event handler invocation.
  const struct IpczPortalStatus* status;
};

// An application-defined function to be invoked by a trap when its observed
// conditions are satisfied on the monitored portal.
typedef void(IPCZ_API* IpczTrapEventHandler)(const struct IpczTrapEvent* event);

#if defined(__cplusplus)
extern "C" {
#endif

// IpczAPI
// =======
//
// Table of API functions defined by ipcz. Instances of this structure may be
// populated by passing them to an implementation of IpczGetAPIFn.
//
// Note that all functions follow a consistent parameter ordering:
//
//   1. Object handle (node or portal) if applicable
//   2. Function-specific strict input values
//   3. Flags - possibly untyped and unused
//   4. Options struct - possibly untyped and unused
//   5. Function-specific in/out values
//   6. Function-specific strict output values
//
// The rationale behind this convention is generally to have order flow from
// input to output. Flags are inputs, and options provide an extension point for
// future versions of these APIs; as such they skirt the boundary between strict
// input values and in/out values.
//
// The order and signature (ABI) of functions defined here must never change,
// but new functions may be added to the end.
struct IPCZ_ALIGN(8) IpczAPI {
  // The exact size of this structure in bytes. Must be set accurately by the
  // application before passing the structure to an implementation of
  // IpczGetAPIFn.
  size_t size;

  // Close()
  // =======
  //
  // Releases the object identified by `handle`. If it's a portal, the portal is
  // closed. If it's a node, parcel, or parcel fragment, the object is
  // destroyed. If it's a boxed driver object, the object is released via the
  // driver API's Close(). If it's a boxed application object, the object is
  // destroyed using the object's boxed custom destructor.
  //
  // This function is NOT thread-safe. It is the application's responsibility to
  // ensure that no other threads are performing other operations on `handle`
  // concurrently with this call or any time thereafter.
  //
  // Note that while closure is itself a (non-blocking) synchronous operation,
  // closure of various objects may have asynchronous side effects. For example,
  // closing a portal might asynchronously trigger a trap event on the portal's
  // remote peer.
  //
  // `flags` is ignored and must be 0.
  //
  // `options` is ignored and must be null.
  //
  // NOTE: If `handle` is a broker node in its cluster of connected nodes,
  // certain operations across the cluster -- such as driver object transmission
  // through portals or portal transference in general -- may begin to fail
  // spontaneously once destruction is complete.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if `handle` referred to a valid object and was
  //        successfully closed by this operation.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `handle` is invalid.
  IpczResult(IPCZ_API* Close)(IpczHandle handle,     // in
                              uint32_t flags,        // in
                              const void* options);  // in

  // CreateNode()
  // ============
  //
  // Initializes a new ipcz node. Applications typically need only one node in
  // each communicating process, but it's OK to create more. Practical use cases
  // for multiple nodes per process may include various testing scenarios, and
  // any situation where simulating a multiprocess environment is useful.
  //
  // All other ipcz calls are scoped to a specific node, or to a more specific
  // object which is itself scoped to a specific node.
  //
  // `driver` is the driver to use when coordinating internode communication.
  // Nodes which will be interconnected must use the same or compatible driver
  // implementations.
  //
  // `driver_node` is a driver-side handle to assign to the node throughout its
  // lifetime. This handle provides the driver with additional context when ipcz
  // makes driver API calls pertaining to a specific node. May be
  // IPCZ_INVALID_DRIVER_HANDLE if not used by the driver.
  //
  // If `flags` contains IPCZ_CREATE_NODE_AS_BROKER then the node will act as
  // the broker in its cluster of connected nodes. See details on that flag
  // description above.
  //
  // `options` is ignored and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if a new node was created. In this case, `*node` is
  //        populated with a valid node handle upon return.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `node` is null, or `driver` is null or
  //        invalid.
  //
  //    IPCZ_RESULT_UNIMPLEMENTED if some condition of the runtime environment
  //        or architecture-specific details of the ipcz build would prevent it
  //        from operating correctly. For example, the is returned if ipcz was
  //        built against a std::atomic implementation which does not provide
  //        lock-free 32-bit and 64-bit atomics.
  IpczResult(IPCZ_API* CreateNode)(
      const struct IpczDriver* driver,              // in
      IpczDriverHandle driver_node,                 // in
      IpczCreateNodeFlags flags,                    // in
      const struct IpczCreateNodeOptions* options,  // in
      IpczHandle* node);                            // out

  // ConnectNode()
  // =============
  //
  // Connects `node` to another node in the system using an application-provided
  // driver transport handle in `driver_transport` for communication. If this
  // call will succeed, ipcz will call back into the driver to activate this
  // transport via ActivateTransport() before returning.
  //
  // The application is responsible for delivering the other endpoint of the
  // transport to whatever other node will use it with its own corresponding
  // ConnectNode() call.
  //
  // The calling node opens a number of initial portals (given by
  // `num_initial_portals`) linked to corresponding initial portals on the
  // remote node as soon as a two-way connection is fully established.
  //
  // Establishment of this connection is typically asynchronous, but this
  // depends on the driver implementation. In any case, the initial portals are
  // created and returned synchronously and can be used immediately by the
  // application. If the remote node issues a corresponding ConnectNode() call
  // with a smaller `num_initial_portals`, the excess portals created by this
  // node will behave as if their peer has been closed. On the other hand if the
  // remote node gives a larger `num_initial_portals`, then its own excess
  // portals will behave as if their peer has been closed.
  //
  //
  // If IPCZ_CONNECT_NODE_TO_BROKER is given in `flags`, the remote node must
  // be a broker node, and the calling node will treat it as such. If the
  // calling node is also a broker, the brokers' respective networks will be
  // effectively merged as long as both brokers remain alive: nodes in one
  // network will be able to discover and communicate directly with nodes in the
  // other network. Note that when two networks are merged, each broker remains
  // as the only broker within its own network; but brokers share enough
  // information to allow for discovery and interconnection of nodes between
  // networks.
  //
  // Conversely if IPCZ_CONNECT_NODE_TO_BROKER is *not* given and neither the
  // local nor remote nodes is a broker, one of the two nodes MUST NOT have a
  // broker yet and must specify IPCZ_CONNECT_NODE_INHERIT_BROKER in its
  // ConnectNode() call. The other node MUST have a broker already and it must
  // specify IPCZ_CONNECT_NODE_SHARE_BROKER in its own corresponding
  // ConnectNode() call.
  //
  // If IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE is given in `flags`, the
  // calling node delegates all ipcz internal shared memory allocation
  // operations to the remote node. This flag should only be used when the
  // calling node is operating in a restricted environment where direct shared
  // memory allocation is not possible.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if all arguments were valid and connection was initiated.
  //        `num_initial_portals` portal handles are populated in
  //        `initial_portals`. These may be used immediately by the application.
  //
  //        Note that because the underlying connection may be established
  //        asynchronously (depending on the driver implementation), this
  //        operation may still fail after returning this value. If this
  //        happens, all of the returned initial portals will behave as if their
  //        peer has been closed.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `node` is invalid, `num_initial_portals`
  //        is zero, `initial_portals` is null, or `flags` specifies one or more
  //        flags which are invalid for `node` or invalid when combined.
  //
  //    IPCZ_RESULT_OUT_OF_RANGE if `num_initial_portals` is larger than the
  //        ipcz implementation allows. There is no hard limit specified, but
  //        any ipcz implementation must support at least 8 initial portals.
  IpczResult(IPCZ_API* ConnectNode)(IpczHandle node,                    // in
                                    IpczDriverHandle driver_transport,  // in
                                    size_t num_initial_portals,         // in
                                    IpczConnectNodeFlags flags,         // in
                                    const void* options,                // in
                                    IpczHandle* initial_portals);       // out

  // OpenPortals()
  // =============
  //
  // Opens two new portals which exist as each other's opposite.
  //
  // Data and handles can be put in a portal with put operations (see Put(),
  // BeginPut(), EndPut()). Anything placed into a portal can be retrieved in
  // the same order by get operations (Get(), BeginGet(), EndGet()) on the
  // opposite portal.
  //
  // To open portals which span two different nodes at creation time, see
  // ConnectNode().
  //
  // `flags` is ignored and must be 0.
  //
  // `options` is ignored and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if portal creation was successful. `*portal0` and
  //        `*portal1` are each populated with opaque portal handles which
  //        identify the new pair of portals. The new portals are each other's
  //        opposite and are entangled until one of them is closed.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `node` is invalid, or if either
  //        `portal0` or `portal1` is null.
  IpczResult(IPCZ_API* OpenPortals)(IpczHandle node,       // in
                                    uint32_t flags,        // in
                                    const void* options,   // in
                                    IpczHandle* portal0,   // out
                                    IpczHandle* portal1);  // out

  // MergePortals()
  // ==============
  //
  // Merges two portals into each other, effectively destroying both while
  // linking their respective peer portals with each other. A portal cannot
  // merge with its own peer, and a portal cannot be merged into another if one
  // or more parcels have already been put into or taken out of either of them.
  // There are however no restrictions on what can be done to the portal's peer
  // prior to merging the portal with another.
  //
  // If we have two portal pairs:
  //
  //    A ---- B     and    C ---- D
  //
  // some parcels are placed into A, and some parcels are placed into D, and
  // then we merge B with C, the net result will be a single portal pair:
  //
  //    A ---- D
  //
  // All past and future parcels placed into A will arrive at D, and vice versa.
  //
  // `flags` is ignored and must be 0.
  //
  // `options` is ignored and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the two portals were merged successfully. Neither
  //        handle is valid past this point. Parcels now travel between the
  //        merged portals' respective peers, including any parcels that were
  //        in flight or queued at the time of this merge.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `first` or `second` is invalid, if
  //        `first` and `second` are each others' peer, or `first` and `second`
  //        refer to the same portal.
  //
  //    IPCZ_RESULT_FAILED_PRECONDITION if either `first` or `second` has
  //        already had one or more parcels put into or gotten out of them.
  IpczResult(IPCZ_API* MergePortals)(IpczHandle first,      // in
                                     IpczHandle second,     // in
                                     uint32_t flags,        // in
                                     const void* options);  // out

  // QueryPortalStatus()
  // ===================
  //
  // Queries specific details regarding the status of a portal, such as the
  // number of unread parcels or data bytes available on the portal or its
  // opposite, or whether the opposite portal has already been closed.
  //
  // Note that because the portal's status is inherently dynamic and may be
  // modified at any time by any thread in any process with a handle to either
  // the portal or its opposite, the information returned in `status` may be
  // stale by the time a successful QueryPortalStatus() call returns.
  //
  // `flags` is ignored and must be 0.
  //
  // `options` is ignored and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the requested query was completed successfully.
  //        `status` is populated with details.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT `portal` is invalid. `status` is null or
  //        invalid.
  IpczResult(IPCZ_API* QueryPortalStatus)(
      IpczHandle portal,                 // in
      uint32_t flags,                    // in
      const void* options,               // in
      struct IpczPortalStatus* status);  // out

  // Put()
  // =====
  //
  // Puts any combination of data and handles into the portal identified by
  // `portal`. Everything put into a portal can be retrieved in the same order
  // by a corresponding get operation on the opposite portal. Depending on the
  // driver and the state of the relevant portals, the data and handles may
  // be delivered and retreivable immediately by the remote portal, or they may
  // be delivered asynchronously.
  //
  // `flags` is unused and must be IPCZ_NO_FLAGS.
  //
  // `options` may be null.
  //
  // If this call fails (returning anything other than IPCZ_RESULT_OK), any
  // provided handles remain property of the caller. If it succeeds, their
  // ownership is assumed by ipcz.
  //
  // Data to be submitted is read directly from the address given by the `data`
  // argument, and `num_bytes` specifies how many bytes of data to copy from
  // there.
  //
  // Callers may wish to request a view directly into portal memory for direct
  // writing. In such cases, a two-phase put operation can be used instead, by
  // calling BeginPut() and EndPut() as defined below.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the provided data and handles were successfull placed
  //        into the portal as a new parcel.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `portal` is invalid, `data` is null but
  //        `num_bytes` is non-zero, `handles` is null but `num_handles` is
  //        non-zero, `options` is non-null but invalid, one of the handles in
  //        `handles` is equal to `portal` or its (local) opposite if
  //        applicable, or if any handle in `handles` is invalid or not
  //        serializable.
  //
  //    IPCZ_RESULT_RESOURCE_EXHAUSTED if `options->limits` is non-null and at
  //        least one of the specified limits would be violated by the
  //        successful completion of this call.
  //
  //    IPCZ_RESULT_NOT_FOUND if it is known that the opposite portal has
  //        already been closed and anything put into this portal would be lost.
  IpczResult(IPCZ_API* Put)(IpczHandle portal,                      // in
                            const void* data,                       // in
                            size_t num_bytes,                       // in
                            const IpczHandle* handles,              // in
                            size_t num_handles,                     // in
                            uint32_t flags,                         // in
                            const struct IpczPutOptions* options);  // in

  // BeginPut()
  // ==========
  //
  // Begins a two-phase put operation on `portal`. While a two-phase put
  // operation is in progress on a portal, any other BeginPut() call on the same
  // portal will fail with IPCZ_RESULT_ALREADY_EXISTS.
  //
  // Unlike a plain Put() call, two-phase put operations allow the application
  // to write directly into portal memory, potentially reducing memory access
  // costs by eliminating redundant copying and caching.
  //
  // The input value of `*num_bytes` tells ipcz how much data the caller would
  // like to place into the portal.
  //
  // Limits provided to BeginPut() elicit similar behavior to Put(), with the
  // exception that `flags` may specify IPCZ_BEGIN_PUT_ALLOW_PARTIAL to allow
  // BeginPut() to succeed even the caller's suggested value of
  // `*num_bytes` would cause the portal to exceed the maximum queued byte limit
  // given by `options->limits`. In that case BeginPut() may update `*num_bytes`
  // to reflect the remaining capacity of the portal, allowing the caller to
  // commit at least some portion of their data with EndPut().
  //
  // Handles for two-phase puts are only provided when finalizing the operation
  // with EndPut().
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the two-phase put operation has been successfully
  //        initiated. This operation must be completed with EndPut() before any
  //        further Put() or BeginPut() calls are allowed on `portal`. `*data`
  //        is set to the address of a portal buffer into which the application
  //        may copy its data, and `*num_bytes` is updated to reflect the
  //        capacity of that buffer, which may be greater than (or less than, if
  //        and only if IPCZ_BEGIN_PUT_ALLOW_PARTIAL was set in `flags`) the
  //        capacity requested by the input value of `*num_bytes`.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `portal` is invalid, `*num_bytes` is
  //        non-zero but `data` is null, or options is non-null and invalid.
  //
  //    IPCZ_RESULT_RESOURCE_EXHAUSTED if completing the put with the number of
  //        bytes specified by `*num_bytes` would cause the portal to exceed the
  //        queued parcel limit or (if IPCZ_BEGIN_PUT_ALLOW_PARTIAL is not
  //        specified in `flags`) data byte limit specified by
  //        `options->limits`.
  //
  //    IPCZ_RESULT_ALREADY_EXISTS if there is already a two-phase put operation
  //        in progress on `portal`.
  //
  //    IPCZ_RESULT_NOT_FOUND if it is known that the opposite portal has
  //        already been closed and anything put into this portal would be lost.
  IpczResult(IPCZ_API* BeginPut)(
      IpczHandle portal,                          // in
      IpczBeginPutFlags flags,                    // in
      const struct IpczBeginPutOptions* options,  // in
      size_t* num_bytes,                          // out
      void** data);                               // out

  // EndPut()
  // ========
  //
  // Ends the two-phase put operation started by the most recent successful call
  // to BeginPut() on `portal`.
  //
  // `num_bytes_produced` specifies the number of bytes actually written into
  // the buffer that was returned from the original BeginPut() call.
  //
  // Usage of `handles` and `num_handles` is identical to Put().
  //
  // If this call fails (returning anything other than IPCZ_RESULT_OK), any
  // provided handles remain property of the caller. If it succeeds, their
  // ownership is assumed by ipcz.
  //
  // If IPCZ_END_PUT_ABORT is given in `flags` and there is a two-phase put
  // operation in progress on `portal`, all other arguments are ignored and the
  // pending two-phase put operation is cancelled without committing a new
  // parcel to the portal.
  //
  // If EndPut() fails for any reason other than
  // IPCZ_RESULT_FAILED_PRECONDITION, the two-phase put operation remains in
  // progress, and EndPut() must be called again to abort the operation or
  // attempt completion with different arguments.
  //
  // `options` is unused and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the two-phase operation was successfully completed or
  //        aborted. If not aborted all data and handles were committed to a new
  //        parcel enqueued for retrieval by the opposite portal.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `portal` is invalid, `num_handles` is
  //        non-zero but `handles` is null, `num_bytes_produced` is larger than
  //        the capacity of the buffer originally returned by BeginPut(), or any
  //        handle in `handles` is invalid or not serializable.
  //
  //    IPCZ_RESULT_FAILED_PRECONDITION if there was no two-phase put operation
  //        in progress on `portal`.
  //
  //    IPCZ_RESULT_NOT_FOUND if it is known that the opposite portal has
  //        already been closed and anything put into this portal would be lost.
  IpczResult(IPCZ_API* EndPut)(IpczHandle portal,          // in
                               size_t num_bytes_produced,  // in
                               const IpczHandle* handles,  // in
                               size_t num_handles,         // in
                               IpczEndPutFlags flags,      // in
                               const void* options);       // in

  // Get()
  // =====
  //
  // Retrieves some combination of data and handles from a source object.
  //
  // If IPCZ_GET_PARCEL_ONLY is specified in `flags` and `source` is a portal,
  // then `data`, `num_bytes` `handles`, and `num_handles` are all ignored and,
  // if a parcel is available to retrieve from the portal, a handle to it is
  // output in `parcel`. This handle can itself be used with Get() (or
  // BeginGet() and EndGet()) to retrieve the parcel's contents, or with
  // Reject() to reject its contents. Returned parcels are owned by the caller
  // and must eventually be closed with Close() to release any associated
  // resources.
  //
  // Otherwise, on input the values pointed to by `num_bytes` and `num_handles`
  // must specify the capacity of each corresponding buffer argument. A null
  // pointer implies zero capacity. It is an error to specify non-zero capacity
  // if the corresponding buffer (`data` or `handles`) is null.
  //
  // Normally the data consumed by this call is copied directly to the address
  // given by the `data` argument, and `*num_bytes` specifies how many bytes of
  // storage are available there.  If an application wishes to read directly
  // from parcel memory instead, a two-phase get operation can be used by
  // calling BeginGet() and EndGet() as defined below.
  //
  // Note that if the caller does not provide enough storage capacity for a
  // complete parcel and does not specify IPCZ_GET_PARTIAL in `flags`, this
  // returns IPCZ_RESULT_RESOURCE_EXHAUSTED and outputs the actual capacity
  // required for the message without copying any of its contents. See details
  // of that return value below.
  //
  // If IPCZ_GET_PARTIAL is specified, the call succeeds as long as `source` is
  // a parcel or a portal with a parcel available. In this case the caller
  // retrieves as much data and handles as their expressed capacity will allow,
  // and the in/out capacity arguments (`num_bytes` and `num_handles`) are still
  // updated as specified in the IPCZ_RESULT_OK details below.
  //
  // If this call succeeds and `parcel` is non-null, then `*parcel` is populated
  // with a new parcel handle which the application can use to report
  // application-level validation failures regarding the retreived parcel (see
  // Reject()).
  //
  // `options` is ignored and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if `source` is a portal and there is a parcel available
  //        in the portal's queue, or `source` is a parcel; and in either case
  //        the parcel's data and handles were able to be copied into the
  //        caller's provided buffers, or IPCZ_GET_PARCEL_ONLY was specified and
  //        `parcel` was non-null.
  //
  //        When IPCZ_GET_PARCEL_ONLY is not specified, values pointed to by
  //        `num_bytes` and `num_handles` (for each one that is non-null) are
  //        updated to reflect what was actually consumed. Note that the caller
  //        assumes ownership of all returned handles.
  //
  //        If `parcel` was non-null, it is populated with a handle to the
  //        retrieved parcel object. If any attached handles were consumed by
  //        the Get() call itself, they will no longer be attached to the
  //        returned parcel object.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `source` is invalid or not a portal or
  //        parcel, `data` is null but `*num_bytes` is non-zero, `handles` is
  //        null but `*num_handles` is non-zero; IPCZ_GET_PARCEL_ONLY is
  //        specified in `flags` but `parcel` is null; `parcel` is non-null but
  //        `source` is itself a parcel; or `parcel` is non-null but
  //        IPCZ_GET_PARTIAL is specified in `flags`.
  //
  //    IPCZ_RESULT_RESOURCE_EXHAUSTED if the consumed parcel would exceed the
  //        caller's specified capacity for either data bytes or handles, and
  //        IPCZ_GET_PARTIAL was not specified in `flags`. In this case any
  //        non-null size pointer is updated to convey the minimum capacity that
  //        would have been required for an otherwise identical Get() call to
  //        have succeeded. Callers observing this result may wish to allocate
  //        storage accordingly and retry with updated parameters.
  //
  //    IPCZ_RESULT_UNAVAILABLE if `source` is a portal whose parcel queue is
  //        currently empty. In this case callers should wait before attempting
  //        to get anything from the same portal again.
  //
  //    IPCZ_RESULT_NOT_FOUND if `source` is a portal which has no more parcels
  //        in its queue and whose peer portal is known to be closed. If this
  //        result is returned, no more parcels can ever be read from `source`.
  //
  //    IPCZ_RESULT_ALREADY_EXISTS if there is a two-phase get operation in
  //        progress on `source`.
  IpczResult(IPCZ_API* Get)(IpczHandle source,    // in
                            IpczGetFlags flags,   // in
                            const void* options,  // in
                            void* data,           // out
                            size_t* num_bytes,    // in/out
                            IpczHandle* handles,  // out
                            size_t* num_handles,  // in/out
                            IpczHandle* parcel);  // out

  // BeginGet()
  // ==========
  //
  // Begins a two-phase get operation on `source` to retrieve data and handles.
  // While a two-phase get operation is in progress on an object, all other get
  // operations on the same object will fail with IPCZ_RESULT_ALREADY_EXISTS.
  //
  // Unlike a plain Get() call, two-phase get operations allow the application
  // to read directly from parcel memory, potentially reducing memory access
  // costs by eliminating redundant copying and caching.
  //
  // If `data` or `num_bytes` is null and the parcel has at least one byte of
  // data, or if there are handles present but `num_handles` is null, this
  // returns IPCZ_RESULT_RESOURCE_EXHAUSTED.
  //
  // Otherwise a successful BeginGet() updates values pointed to by `data`,
  // `num_bytes`, and `num_handles` to convey the parcel's data storage and
  // capacity as well as the capacity required to read out any handles.
  //
  // NOTE: When performing two-phase get operations, callers should be mindful
  // of time-of-check/time-of-use (TOCTOU) vulnerabilities. Exposed parcel
  // memory may be shared with (and writable in) the process which transmitted
  // the parcel, and that process may not be trustworthy.
  //
  // `flags` is ignored and must be IPCZ_NO_FLAGS.
  //
  // `options` is ignored and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the two-phase get was successfully initiated. In this
  //        case both `*data` and `*num_bytes` are updated (if `data` and
  //        `num_bytes` were non-null) to describe the parcel memory from which
  //        the application is free to read parcel data. If `num_handles` is
  //        is non-null, the value pointed to is updated to reflect the number
  //        of handles available to retrieve.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `source` is invalid.
  //
  //    IPCZ_RESULT_RESOURCE_EXHAUSTED if the parcel has at least one data byte
  //        but `data` or `num_bytes` is null; or if the parcel has any handles
  //        but `num_handles` null.
  //
  //    IPCZ_RESULT_UNAVAILABLE if `source` is a portal whose parcel queue is
  //        currently empty. In this case callers should wait before attempting
  //        to get anything from the same portal again.
  //
  //    IPCZ_RESULT_NOT_FOUND if `source` is a portal with no more parcels in
  //        its queue and whose peer portal is known to be closed. In this case,
  //        no get operation can ever succeed again on this portal.
  //
  //    IPCZ_RESULT_ALREADY_EXISTS if there is already a two-phase get operation
  //        in progress on `source`.
  IpczResult(IPCZ_API* BeginGet)(IpczHandle source,     // in
                                 uint32_t flags,        // in
                                 const void* options,   // in
                                 const void** data,     // out
                                 size_t* num_bytes,     // out
                                 size_t* num_handles);  // out

  // EndGet()
  // ========
  //
  // Ends the two-phase get operation started by the most recent successful call
  // to BeginGet() on `source`.
  //
  // `num_bytes_consumed` specifies the number of bytes actually read from the
  // buffer that was returned from the original BeginGet() call. `num_handles`
  // specifies the capacity of `handles` and must be no larger than the
  // capacity indicated by the corresponding output from BeginGet().
  //
  // If IPCZ_END_GET_ABORT is given in `flags` and there is a two-phase get
  // operation in progress on `source`, all other arguments are ignored and the
  // pending operation is cancelled without consuming any data from the source.
  //
  // `options` is unused and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the two-phase operation was successfully completed or
  //        aborted. Note that if `source` is a portal and its frontmost parcel
  //        was not fully consumed by this call, it will remain in queue with
  //        the rest of its data intact for a subsequent get operation to
  //        retrieve from the portal. Exactly `num_handles` handles will be
  //        copied into `handles`.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `source` is invalid, or if `num_handles`
  //        is non-zero but `handles` is null.
  //
  //    IPCZ_RESULT_OUT_OF_RANGE if either `num_bytes_consumed` or `num_handles`
  //        is larger than the capacity returned by BeginGet().
  //
  //    IPCZ_RESULT_FAILED_PRECONDITION if there was no two-phase get operation
  //        in progress on `source`.
  IpczResult(IPCZ_API* EndGet)(IpczHandle source,          // in
                               size_t num_bytes_consumed,  // in
                               size_t num_handles,         // in
                               IpczEndGetFlags flags,      // in
                               const void* options,        // in
                               IpczHandle* handles);       // out

  // Trap()
  // ======
  //
  // Attempts to install a trap to catch interesting changes to a portal's
  // state. The condition(s) to observe are specified in `conditions`.
  // Regardless of what conditions the caller specifies, all successfully
  // installed traps also implicitly observe IPCZ_TRAP_REMOVED.
  //
  // If successful, ipcz guarantees that `handler` will be invoked -- with the
  // the `context` field of the invocation's IpczTrapEvent reflecting the value
  // of `context` given here -- once any of the specified conditions have been
  // met.
  //
  // Immediately before invoking its handler, the trap is removed from the
  // portal and must be reinstalled in order to observe further state changes.
  //
  // When a portal is closed, any traps still installed on it are notified by
  // invoking their handler with IPCZ_TRAP_REMOVED in the event's
  // `condition_flags`. This effectively guarantees that all installed traps
  // eventually see a handler invocation.
  //
  // Note that `handler` may be invoked from any thread that can modify the
  // state of the observed portal. This is limited to threads which make direct
  // ipcz calls on the portal, and any threads on which the portal's node may
  // receive notifications from a driver transport.
  //
  // If any of the specified conditions are already met, the trap is not
  // installed and this call returns IPCZ_RESULT_FAILED_PRECONDITION. See below
  // for details.
  //
  // `flags` is ignored and must be 0.
  //
  // `options` is ignored and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the trap was installed successfully. In this case
  //        `flags` and `status` arguments are ignored.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `portal` is invalid, `conditions` is
  //        null or invalid, `handler` is null, or `status` is non-null but its
  //        `size` field specifies an invalid value.
  //
  //    IPCZ_RESULT_FAILED_PRECONDITION if the conditions specified are already
  //        met on the portal. If `satisfied_condition_flags` is non-null, then
  //        its pointee value will be updated to reflect the flags in
  //        `conditions` which were already satisfied by the portal's state. If
  //        `status` is non-null, a copy of the portal's last known status will
  //        also be stored there.
  IpczResult(IPCZ_API* Trap)(
      IpczHandle portal,                                  // in
      const struct IpczTrapConditions* conditions,        // in
      IpczTrapEventHandler handler,                       // in
      uintptr_t context,                                  // in
      uint32_t flags,                                     // in
      const void* options,                                // in
      IpczTrapConditionFlags* satisfied_condition_flags,  // out
      struct IpczPortalStatus* status);                   // out

  // Reject()
  // ========
  //
  // Reports an application-level validation failure to ipcz, in reference to
  // a specific `parcel` returned by a previous call to Get().
  //
  // ipcz propagates this rejection to the driver via
  // ReportBadTransportActivity(), if and only if the associated parcel did in
  // fact come from a remote node.
  //
  // `context` is an opaque handle which, on success, is passed to the driver
  // when issuing a corresponding ReportBadTransportActivity() invocation.
  //
  // `flags` is ignored and must be 0.
  //
  // `options` is ignored and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the driver was successfully notified about this
  //        rejection via ReportBadTransportActivity().
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `parcel` is not a valid parcel handle
  //        previously returned by Get().
  //
  //    IPCZ_RESULT_FAILED_PRECONDITION if `parcel` is associated with a parcel
  //        that did not come from another node.
  IpczResult(IPCZ_API* Reject)(IpczHandle parcel,
                               uintptr_t context,
                               uint32_t flags,
                               const void* options);

  // Box()
  // =====
  //
  // Boxes an object managed by the driver or application and returns a new
  // IpczHandle to reference the box. If the driver or application is able to
  // serialize the boxed object, the box can be placed into a portal for
  // transmission to another node.
  //
  // Boxes can be sent through portals along with other IpczHandles, effectively
  // allowing drivers and applications to introduce new types of transferrable
  // objects via boxes.
  //
  // `flags` is ignored and must be 0.
  //
  // `options` is ignored and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the object was boxed and a new IpczHandle is returned
  //        in `handle`.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `contents` is null or malformed, or if
  //        `handle` is null.
  IpczResult(IPCZ_API* Box)(IpczHandle node,                         // in
                            const struct IpczBoxContents* contents,  // in
                            uint32_t flags,                          // in
                            const void* options,                     // in
                            IpczHandle* handle);                     // out

  // Unbox()
  // =======
  //
  // Unboxes the contents of a box previously produced by Box(). Note that if
  // a box was originally produced from an application object and subsequently
  // transmitted to another node, it will be unboxed as a parcel fragment
  // produced by the object's custom serializer.
  //
  // `flags` is ignored and must be 0.
  //
  // `options` is ignored and must be null.
  //
  // Returns:
  //
  //    IPCZ_RESULT_OK if the object was successfully unboxed. A description of
  //        the box contents is placed in `contents`.
  //
  //    IPCZ_RESULT_INVALID_ARGUMENT if `handle` is invalid or does not
  //        reference a box, or if `contents` is null or malformed.
  IpczResult(IPCZ_API* Unbox)(IpczHandle handle,                  // in
                              IpczUnboxFlags flags,               // in
                              const void* options,                // in
                              struct IpczBoxContents* contents);  // out
};

// A function which populates `api` with a table of ipcz API functions. The
// `size` field must be set by the caller to the size of the structure before
// issuing this call.
//
// In practice ipcz defines IpczGetAPI() as an implementation of this function
// type. How applications acquire a reference to that function depends on how
// the application builds and links against ipcz.
//
// Upon return, `api->size` indicates the size of the function table actually
// populated and therefore which version of the ipcz implementation is in use.
// Note that this size will never exceed the input value of `api->size`: if the
// caller is built against an older version than what is available, the
// available implementation will only populate the functions appropriate for
// that older version. Conversely if the caller is built against a newer version
// than what is available, `api->size` on output may be smaller than its value
// was on input.
//
// Returns:
//
//    IPCZ_RESULT_OK if `api` was successfully populated. In this case
//       `api->size` effectively indicates the API version provided, and the
//       appropriate function pointers within `api` are filled in.
//
//    IPCZ_RESULT_INVALID_ARGUMENT if `api` is null or the caller's provided
//       `api->size` is less than the size of the function table required to
//       host API version 0.
typedef IpczResult(IPCZ_API* IpczGetAPIFn)(struct IpczAPI* api);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // IPCZ_INCLUDE_IPCZ_IPCZ_H_
