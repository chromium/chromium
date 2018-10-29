# Chromium web_runner
This directory contains the web_runner implementation. Web_runner enables
Fuchsia applications to embed Chrome frames for rendering web content.


### Building and deploying web_runner
When you build web_runner, Chromium will automatically generate scripts for
you that will automatically provision a device with Fuchsia and then install
`web_runner` and its dependencies.

To build and run web_runner, follow these steps:

0. Ensure that you have a device ready to boot into Fuchsia.

   If you wish to have WebRunner manage the OS deployment process, then you
   should have the device booting into
   [Zedboot](https://fuchsia.googlesource.com/zircon/+/master/docs/targets/bootloader_setup.md).

1. Build web_runner.

   ```
   $ autoninja -C out/Debug webrunner
   ```

2. Install web_runner.

    * **For devices running Zedboot**

          ```
          $ out/Debug/bin/install_webrunner -d
          ```

    * **For devices already running Fuchsia**

          You will need to add command line flags specifying the device's IP
          address and the path to the `ssh_config` used by the device
          (located at `FUCHSIA_OUT_DIR/ssh-keys/ssh_config`):

          ```
          $ out/Debug/bin/install_webrunner -d --ssh-config PATH_TO_SSH_CONFIG
          --host DEVICE_IP
          ```

3. Run "tiles" on the device.

   ```
   $ run tiles&
   ```

4. Press the OS key on your device to switch back to terminal mode.
   (Also known as the "Windows key" or "Super key" on many keyboards).

5. Launch a webpage.

   ```
   $ tiles_ctl add https://www.chromium.org/
   ```

6. Press the OS key to switch back to graphical view. The browser window should
   be displayed and ready to use.

7. You can deploy and run new versions of Chromium without needing to reboot.

   First kill any running processes:

      ```
      $ killall chromium; killall web_runner
      ```

   Then repeat steps 1 through 6 from the installation instructions, excluding
   step #3 (running Tiles).


### Closing a webpage

1. Press the Windows key to return to the terminal.

2. Instruct tiles_ctl to remove the webpage's window tile. The tile's number is
   reported by step 6, or it can be found by running `tiles_ctl list` and
   noting the ID of the "url" entry.

   ```shell
   $ tiles_ctl remove TILE_NUMBER
   ```

### Debugging

Rudimentary debugging is now possible with zxdb which is included in the SDK.
It is still early and fairly manual to set up. After following the steps above:

1. On device, run `sysinfo` to see your device's IP address.

1. On device, run `debug_agent --port=2345`.

1. On the host, run

```
third_party/fuchsia_sdk/sdk/tools/zxdb -s out/Debug/exe.unstripped -s out/Debug/lib.unstripped
```

1. In zxdb, `connect <ip-from-sysinfo-above> 2345`.

1. On the host, run `ps` and find the pid of the process you want to debug, e.g.
   `web_runner`.

1. In zxdb, `attach <pid>`. You should be able to attach to multiple processes.

1. In zxdb, `b ComponentControllerImpl::CreateForRequest` to set a breakpoint.

1. On device, do something to make your breakpoint be hit. In this case
   `tiles_ctl add https://www.google.com/` should cause a new request.

At this point, you should hit the breakpoint in zxdb.

```
[zxdb] l
   25     fuchsia::sys::Package package,
   26     fuchsia::sys::StartupInfo startup_info,
   27     fidl::InterfaceRequest<fuchsia::sys::ComponentController>
   28         controller_request) {
   29   std::unique_ptr<ComponentControllerImpl> result{
 ▶ 30       new ComponentControllerImpl(runner)};
   31   if (!result->BindToRequest(std::move(package), std::move(startup_info),
   32                              std::move(controller_request))) {
   33     return nullptr;
   34   }
   35   return result;
   36 }
   37
   38 ComponentControllerImpl::ComponentControllerImpl(WebContentRunner* runner)
   39     : runner_(runner), controller_binding_(this) {
   40   DCHECK(runner);
[zxdb] f
▶ 0 webrunner::ComponentControllerImpl::CreateForRequest() • component_controller_impl.cc:30
  1 webrunner::WebContentRunner::StartComponent() • web_content_runner.cc:34
  2 fuchsia::sys::Runner_Stub::Dispatch_() • fidl.cc:1255
  3 fidl::internal::StubController::OnMessage() • stub_controller.cc:38
  4 fidl::internal::MessageReader::ReadAndDispatchMessage() • message_reader.cc:213
  5 fidl::internal::MessageReader::OnHandleReady() • message_reader.cc:179
  6 fidl::internal::MessageReader::CallHandler() • message_reader.cc:166
  7 base::AsyncDispatcher::DispatchOrWaitUntil() • async_dispatcher.cc:183
  8 base::MessagePumpFuchsia::HandleEvents() • message_pump_fuchsia.cc:236
  9 base::MessagePumpFuchsia::Run() • message_pump_fuchsia.cc:282
  10 base::MessageLoop::Run() + 0x22b (no line info)
  11 base::RunLoop::Run() • run_loop.cc:102
  12 main() • main.cc:74
  13 0x472010320b8f
  14 0x0
[zxdb]
```

https://fuchsia.googlesource.com/garnet/+/master/docs/debugger.md#diagnosing-symbol-problems
maybe be a useful reference if you do not see symbols. That page also has
general help on using the debugger.
