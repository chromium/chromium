Basic usage
===========
Download dependencies (in the directory `palloc_viewer`):
```
git clone https://github.com/ocornut/imgui
git clone https://github.com/epezent/implot
```

Build with `ninja`.

Run with `./palloc-viewer-gui $pid_of_target_process`.

Dependencies
============
This is almost certainly incomplete...

Debian packages:
```
apt install libsdl2-dev libc6-dbg libdw-dev
```

You'll need a Chrome build that comes with debug symbols.
If you're building Chrome from source yourself, just make sure to build with symbols.
If you're using an official Chrome build from Google's official repos, you'll need to
separately grab the Google-internal debug symbols (only accessible to Googlers) and
put them somewhere where libdw can find them when looking up debug symbols using the
build ID. (I haven't tested that method myself.)

There are some structs that are more or less copied over from PartitionAlloc; if those
change in Chrome, you may have to copy those changes over.

WARNINGS
========
If you're using Chrome's task manager to look up the PID of a process, keep in mind
that while the task manager is open, there will be a ton of extra periodic IPC messages
flying around. In particular if you're looking at process wakeups, you should probably
close Chrome's task manager.
(We also trigger wakeups when we attach with ptrace to read FSBASE, but we only do that
once per thread, so that should be fine.)

Warnings are logged to stderr, they don't show up in the UI.

We are racily parsing heap data structures while they're concurrently being modified.
This can lead to consistency issues that can cause spurious warnings and temporarily
inaccurate data. (So a bit of error spew on stderr is normal.)

The kernel does not provide a clean way to determine whether a swapped-out page is
owned exclusively by us, so we just assume that it is.

If you run this over a remote desktop protocol with lossy encoding, it might get hard
to see some details in the UI. If you're a Googler, there are docs on how to fix that.

Screenshots
===========
See
[here](https://drive.google.com/drive/folders/1IqPKAmayTwqAHZkWuLa15OYNdPP-Awir?usp=sharing).
