# Android Touch Replay Tool

This tool allows to record and replay touch events on Android devices. The
primary goal is for the replays to be consistent with each other as much as
possible to allow for repeating touch gestures many times with statistical
analysis to follow. Maintaining consistency with the original sequence of
touches is only a secondary goal.

Recording on one device and replaying on another is likely not going to work
because of differences in the structure of touch events. The way the kernel
drivers emit the events depends heavily on device type and OS version.

Update: After creating the Touch Replay tool Android started supporting
[a port of evemu](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/cmds/uinput/README.md)
with more functionality like human readable format for dumps. We should
eventually rewrite these instructions to describe the new tool.

## How to use

Please use an optimized release build to minimize potential delays between
emitted events. Here is how to build it:

```
# autoninja -C out/AndroidRelease touch_replay
```

This produces an executable only runnable on the device. Push it to the device:

```
# adb push out/AndroidRelease/touch_replay /data/local/tmp
```

Before running the tool make sure it will be invoked with root privileges:

```
adb root
```

Run the tool. All interaction with the screen will be recorded until you press
Ctrl+C.

```
# adb shell '/data/local/tmp/touch_replay record /data/local/tmp/touch_events.dump'
```

To replay change `record` with `replay` in the command above.

The last piece of functionality of this tool is the help message:
```
# adb shell '/data/local/tmp/touch_replay --help'
Usage: /data/local/tmp/touch_replay record|replay FILE

Record input events to FILE or replay them from FILE.
```

### Usage with Perfetto

It is important to be able to replay touch scenarios while the device is being
traced. The `cpu_profile` tool is compatible with running the replay
simultaneously. See about its usage in the [Perfetto
documentation](https://perfetto.dev/docs/quickstart/callstack-sampling#prerequisites).

Using Perfetto from the browser to record a trace on the device is less
straightforward. While attached to the device, Perfetto may make the device
invisible to the commandline `adb` tool:

```
adb: no device found
```

One workaround is to start replay and attach with Perfetto later:

```
adb shell 'nohup sh -c "sleep 15 && /data/local/tmp/touch_replay replay
/data/local/tmp/touch_events.dump" >/dev/null 2>&1 &'
```

This will wait for 15 seconds before replaying. The adb command would return
immediately. After that the tool runs on the device without depending on the
existing adb session. This works by running the program as a daemon with the
help of the `nohup` utility. Since both the sleeping and replaying needs to be
daemonized, they are wrapped into a shell command together.

Immediately after invoking the command above it should be possible to connect to
the device with Perfetto (within the specified time interval, 15 seconds in this
case) and start profiling.
