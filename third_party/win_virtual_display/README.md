# Windows Virtual Display Driver

This directory contains a code and a visual studio solution for a driver which
instantiates and controls virtual displays on a host machine.

See: [Virtual Displays for Automated Tests](https://docs.google.com/document/d/1rtxO2FEg0Zl_-oXHzIBsJo6py7wkySUpYruteNMlPys/edit?resourcekey=0-yLkX6DGPwNFn1ARMpM-zLQ#heading=h.in0m2co51p2p)
and [GSoC 2023 project Proposal](https://docs.google.com/document/d/1SY2FfGaZaKR6Twe3VXknFGWaooEaDcFdNRIy1piqFNk/edit?resourcekey=0-aEouTXQV_inimkq87_lU4g)

The client-side controller for this driver lives in:
[//third_party/win_virtual_display/controller](https://crsrc.org/c/third_party/win_virtual_display/controller).


## Prerequisites

### Driver Development Kit
1. Download the windows
[Enterprise Windows Driver Kit](https://learn.microsoft.com/en-us/legal/windows/hardware/enterprise-wdk-license-2022)
(Windows 11, version 22H2 required).
2. Mount the ISO.
3. (Optionally) Copy the contents of the mounted ISO to a directory on
C:/ (e.g. C:/wdk)
4. Open `LaunchBuildEnv.cmd` in the root of the EWDK directory.

## Building

Note that the following instructions are for use with the EWDK and the command
line. Building from Visual Studio is not covered here.

### Building the Driver
The build steps must be completed in an environment from `LaunchBuildEnv.cmd`
in the [Driver Development Kit](#driver-development-kit) section above.

```
msbuild third_party\win_virtual_display\driver\ChromiumVirtualDisplayDriver.vcxproj /t:build /property:Platform=x64
```

### Installing the Driver

After [Building the Driver](#building-the-driver), the driver package will be
located in `x64\Debug\ChromiumVirtualDisplayDriver`.

#### Install the driver:
In an **Elevated** command prompt:
```
cd x64\Debug\ChromiumVirtualDisplayDriver
pnputil /add-driver *.inf /install /subdirs
```

Note: During development, the driver may fail to install due to an untrusted
development certificate. To allow the install, add the certificate as a trusted
authority:
1. Open the certificate "ChromiumVirtualDisplayDriver.cer" in `x64\Debug`.
2. Click Install Certificate.
3. Select "Local Machine".
4. "Place all certificates in the following store", click "Browse...".
5. Select "Trusted Root Certification Authorities", click OK.
6. Click Next.
7. Click Finish.

## Running the Sample Executable
The sample executable code is located in
//third_party/win_virtual_display/controller. It can be built and ran
with the following commands:
```
autoninja -C out/Default display_driver_controller_executable
./out/Default/display_driver_controller_executable
```

Note that running the executable (second command above) requires an an elevated
command prompt.

Running the executable will instantiate a software device (driver) which will
run the driver code to create the virtual displays. After running the
executable, you should expect to see several more displays appear in the
Windows display settings panel. Note that this does not work over Remote Desktop
and you must use another solution like Chrome Remote Desktop.

## Tracing / Debugging
Tracing is achieved with the
[In-flight Trace Recorder](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/using-wpp-recorder).
Tracing can be added to Driver.cpp using `TraceLog`. Example:
```cpp
TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER , "WdfDriverCreate failed, %!STATUS!", ntStatus);
```

Note that the file which includes `TraceEvents` must have the appropriate
flags set in the `.vcxproj`:
```xml
<ClCompile Include="Driver.cpp">
<WppEnabled>true</WppEnabled>
<WppRecorderEnabled>true</WppRecorderEnabled>
<WppPreprocessorDefinitions>WPP_MACRO_USE_KM_VERSION_FOR_UM=1</WppPreprocessorDefinitions>
</ClCompile>
```

In order to view the logs, use the TraceView tool included in the windows SDK.
For example it might be located here:
```
C:\wdk\Program Files\Windows Kits\10\Tools\10.0.22621.0\x64\traceview.exe
```

Once TraceView is running:

1. File -> Create New Log Session.
2. Select "PDB (Debug Information) File" and click "...".
3. (Must build the solution first). Select `ChromiumVirtualDisplayDriver.pdb`
file (located in `x64/Debug/ChromiumVirtualDisplayDriver.pdb`).
4. Click OK.
5. click Next.
6. Click Finish.

Now the drivers log events will appear in the window.
