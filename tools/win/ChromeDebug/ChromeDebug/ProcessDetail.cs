// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using Microsoft.Win32.SafeHandles;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using ChromeDebug.LowLevel;
using System.Runtime.InteropServices;
using System.Drawing;

namespace ChromeDebug {
  internal class ProcessDetail : IDisposable {
    public ProcessDetail(int pid) {
      // Initialize everything to null in case something fails.
      this.processId = pid;
      this.processHandleFlags = LowLevelTypes.ProcessAccessFlags.NONE;
      this.cachedProcessBasicInfo = null;
      this.machineTypeIsLoaded = false;
      this.machineType = LowLevelTypes.MachineType.UNKNOWN;
      this.cachedPeb = null;
      this.cachedProcessParams = null;
      this.cachedCommandLine = null;
      this.processHandle = IntPtr.Zero;

      OpenAndCacheProcessHandle();
    }

    // Returns the machine type (x86, x64, etc) of this process.  Uses lazy evaluation and caches
    // the result.
    public LowLevelTypes.MachineType MachineType {
      get {
        if (machineTypeIsLoaded)
          return machineType;
        if (!CanQueryProcessInformation)
          return LowLevelTypes.MachineType.UNKNOWN;

        CacheMachineType();
        return machineType;
      }
    }

    public string NativeProcessImagePath {
      get {
        if (nativeProcessImagePath == null) {
          nativeProcessImagePath = QueryProcessImageName(
              LowLevelTypes.ProcessQueryImageNameMode.NATIVE_SYSTEM_FORMAT);
        }
        return nativeProcessImagePath;
      }
    }

    public string Win32ProcessImagePath {
      get {
        if (win32ProcessImagePath == null) {
          win32ProcessImagePath = QueryProcessImageName(
              LowLevelTypes.ProcessQueryImageNameMode.WIN32_FORMAT);
        }
        return win32ProcessImagePath;
      }
    }

    public Icon SmallIcon {
      get {
        LowLevel.LowLevelTypes.SHFILEINFO info = new LowLevelTypes.SHFILEINFO(true);
        LowLevel.LowLevelTypes.SHGFI flags = LowLevel.LowLevelTypes.SHGFI.Icon
                                             | LowLevelTypes.SHGFI.SmallIcon
                                             | LowLevelTypes.SHGFI.OpenIcon
                                             | LowLevelTypes.SHGFI.UseFileAttributes;
        int cbFileInfo = Marshal.SizeOf(info);
        LowLevel.NativeMethods.SHGetFileInfo(Win32ProcessImagePath,
                                             256,
                                             ref info,
                                             (uint)cbFileInfo,
                                             (uint)flags);
        return Icon.FromHandle(info.hIcon);
      }
    }

    // Returns the command line that this process was launched with.  Uses lazy evaluation and
    // caches the result.  Reads the command line from the PEB of the running process.
    public string CommandLine {
      get {
        if (!CanReadPeb)
          throw new InvalidOperationException();
        CacheProcessInformation();
        CachePeb();
        CacheProcessParams();
        CacheCommandLine();
        return cachedCommandLine;
      }
    }

    // Determines if we have permission to read the process's PEB.
    public bool CanReadPeb {
      get {
        LowLevelTypes.ProcessAccessFlags required_flags =
            LowLevelTypes.ProcessAccessFlags.VM_READ
          | LowLevelTypes.ProcessAccessFlags.QUERY_INFORMATION;

        // In order to read the PEB, we must have *both* of these flags.
        if ((processHandleFlags & required_flags) != required_flags)
          return false;

        // If we're on a 64-bit OS, in a 32-bit process, and the target process is not 32-bit,
        // we can't read its PEB.
        if (Environment.Is64BitOperatingSystem && !Environment.Is64BitProcess
            && (MachineType != LowLevelTypes.MachineType.X86))
          return false;

        return true;
      }
    }

    // If we can't read the process's PEB, we may still be able to get other kinds of information
    // from the process.  This flag determines if we can get lesser information.
    private bool CanQueryProcessInformation {
      get {
        LowLevelTypes.ProcessAccessFlags required_flags =
            LowLevelTypes.ProcessAccessFlags.QUERY_LIMITED_INFORMATION
          | LowLevelTypes.ProcessAccessFlags.QUERY_INFORMATION;
        
        // In order to query the process, we need *either* of these flags.
        return (processHandleFlags & required_flags) != LowLevelTypes.ProcessAccessFlags.NONE;
      }
    }

    private string QueryProcessImageName(LowLevelTypes.ProcessQueryImageNameMode mode) {
      StringBuilder moduleBuffer = new StringBuilder(1024);
      int size = moduleBuffer.Capacity;
      NativeMethods.QueryFullProcessImageName(
          processHandle,
          mode,
          moduleBuffer,
          ref size);
      if (mode == LowLevelTypes.ProcessQueryImageNameMode.NATIVE_SYSTEM_FORMAT)
        moduleBuffer.Insert(0, "\\\\?\\GLOBALROOT");
      return moduleBuffer.ToString();
    }

    // Loads the top-level structure of the process's information block and caches it.
    private void CacheProcessInformation() {
      System.Diagnostics.Debug.Assert(CanReadPeb);

      // Fetch the process info and set the fields.
      LowLevelTypes.PROCESS_BASIC_INFORMATION temp = new LowLevelTypes.PROCESS_BASIC_INFORMATION();
      int size;
      LowLevelTypes.NTSTATUS status = NativeMethods.NtQueryInformationProcess(
          processHandle, 
          LowLevelTypes.PROCESSINFOCLASS.PROCESS_BASIC_INFORMATION, 
          ref temp, 
          Utility.UnmanagedStructSize<LowLevelTypes.PROCESS_BASIC_INFORMATION>(), 
          out size);

      if (status != LowLevelTypes.NTSTATUS.SUCCESS) {
        throw new Win32Exception();
      }

      cachedProcessBasicInfo = temp;
    }

    // Follows a pointer from the PROCESS_BASIC_INFORMATION structure in the target process's
    // address space to read the PEB.
    private void CachePeb() {
      System.Diagnostics.Debug.Assert(CanReadPeb);

      if (cachedPeb == null) {
        cachedPeb = Utility.ReadUnmanagedStructFromProcess<LowLevelTypes.PEB>(
            processHandle,
            cachedProcessBasicInfo.Value.PebBaseAddress);
      }
    }

    // Follows a pointer from the PEB structure in the target process's address space to read the
    // RTL_USER_PROCESS_PARAMETERS structure.
    private void CacheProcessParams() {
      System.Diagnostics.Debug.Assert(CanReadPeb);

      if (cachedProcessParams == null) {
        cachedProcessParams =
            Utility.ReadUnmanagedStructFromProcess<LowLevelTypes.RTL_USER_PROCESS_PARAMETERS>(
                processHandle, cachedPeb.Value.ProcessParameters);
      }
    }

    private void CacheCommandLine() {
      System.Diagnostics.Debug.Assert(CanReadPeb);

      if (cachedCommandLine == null) {
        cachedCommandLine = Utility.ReadStringUniFromProcess(
            processHandle,
            cachedProcessParams.Value.CommandLine.Buffer,
            cachedProcessParams.Value.CommandLine.Length / 2);
      }
    }

    private void CacheMachineType() {
      System.Diagnostics.Debug.Assert(CanQueryProcessInformation);

      // If our extension is running in a 32-bit process (which it is), then attempts to access
      // files in C:\windows\system (and a few other files) will redirect to C:\Windows\SysWOW64
      // and we will mistakenly think that the image file is a 32-bit image.  The way around this
      // is to use a native system format path, of the form:
      //    \\?\GLOBALROOT\Device\HarddiskVolume0\Windows\System\foo.dat
      // NativeProcessImagePath gives us the full process image path in the desired format.
      string path = NativeProcessImagePath;

      // Open the PE File as a binary file, and parse just enough information to determine the
      // machine type.
      //http://www.microsoft.com/whdc/system/platform/firmware/PECOFF.mspx
      using (SafeFileHandle safeHandle = NativeMethods.CreateFile(
                 path, 
                 LowLevelTypes.FileAccessFlags.GENERIC_READ, 
                 LowLevelTypes.FileShareFlags.SHARE_READ, 
                 IntPtr.Zero, 
                 LowLevelTypes.FileCreationDisposition.OPEN_EXISTING, 
                 LowLevelTypes.FileFlagsAndAttributes.NORMAL, 
                 IntPtr.Zero)) {
        FileStream fs = new FileStream(safeHandle, FileAccess.Read);
        using (BinaryReader br = new BinaryReader(fs)) {
          fs.Seek(0x3c, SeekOrigin.Begin);
          Int32 peOffset = br.ReadInt32();
          fs.Seek(peOffset, SeekOrigin.Begin);
          UInt32 peHead = br.ReadUInt32();
          if (peHead != 0x00004550) // "PE\0\0", little-endian
            throw new Exception("Can't find PE header");
          machineType = (LowLevelTypes.MachineType)br.ReadUInt16();
          machineTypeIsLoaded = true;
        }
      }
    }

    private void OpenAndCacheProcessHandle() {
      // Try to open a handle to the process with the highest level of privilege, but if we can't
      // do that then fallback to requesting access with a lower privilege level.
      processHandleFlags = LowLevelTypes.ProcessAccessFlags.QUERY_INFORMATION
                         | LowLevelTypes.ProcessAccessFlags.VM_READ;
      processHandle = NativeMethods.OpenProcess(processHandleFlags, false, processId);
      if (processHandle == IntPtr.Zero) {
        processHandleFlags = LowLevelTypes.ProcessAccessFlags.QUERY_LIMITED_INFORMATION;
        processHandle = NativeMethods.OpenProcess(processHandleFlags, false, processId);
        if (processHandle == IntPtr.Zero) {
          processHandleFlags = LowLevelTypes.ProcessAccessFlags.NONE;
          throw new Win32Exception();
        }
      }
    }

    // An open handle to the process, along with the set of access flags that the handle was
    // open with.
    private int processId;
    private IntPtr processHandle;
    private LowLevelTypes.ProcessAccessFlags processHandleFlags;
    private string nativeProcessImagePath;
    private string win32ProcessImagePath;

    // The machine type is read by parsing the PE image file of the running process, so we cache
    // its value since the operation expensive.
    private bool machineTypeIsLoaded;
    private LowLevelTypes.MachineType machineType;

    // The following fields exist ultimately so that we can access the command line.  However,
    // each field must be read separately through a pointer into another process's address
    // space so the access is expensive, hence we cache the values.
    private Nullable<LowLevelTypes.PROCESS_BASIC_INFORMATION> cachedProcessBasicInfo;
    private Nullable<LowLevelTypes.PEB> cachedPeb;
    private Nullable<LowLevelTypes.RTL_USER_PROCESS_PARAMETERS> cachedProcessParams;
    private string cachedCommandLine;

    ~ProcessDetail() {
      Dispose();
    }

    public void Dispose() {
      if (processHandle != IntPtr.Zero)
        NativeMethods.CloseHandle(processHandle);
      processHandle = IntPtr.Zero;
    }
  }
}
