# Nested Message Loop Mechanism

## Overview
In Chromium, the UI thread's message pump processes work in a round-robin
fashion from two primary sources: the Chrome Task Queue and the native OS
Message Queue (e.g., the Windows Message Queue). Bugs may occur when the thread
blocks and spins a nested message loop, causing re-entrancy into UI code that
destroys objects currently in use by the blocked outer stack frames.

## Mechanisms of Re-entrancy
When writing UI code, be aware of the following mechanisms that introduce
implicit nested message loops:

1.  **OS `SendMessage` (Windows):** If a `SendMessage` call crosses thread
    boundaries, the sending thread blocks until the target thread responds.
    While blocked, the OS will automatically dispatch incoming **non-queued**
    messages to the waiting thread.
    *   *Note:* Chrome tasks rely on *queued* messages, so they will not
        execute during this block. The risk comes entirely from native OS
        callbacks (e.g., window activation, focus changes, destruction)
        firing re-entrantly and calling back into Views code.
    *   Messages already on the queue will not be dispatched during these
        calls. Only sending a message directly to the thread will dispatch immediately.
    *   Any risk to these calls must originate from a direct `SendMessage` call
        to the HWND during a cross-thread call.
2.  **COM Synchronous Calls (Windows):** When a thread part of a single-threaded
    apartment (STA) blocks on a synchronous cross-apartment COM call, COM spins
    an internal message pump. This pump only processes messages from its
    internal RPC HWND and within a specific message range. However, if COM
    dispatches an RPC callback that re-enters Chrome code, state can still be
    mutated unexpectedly.
