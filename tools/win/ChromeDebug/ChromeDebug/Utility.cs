// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

using ChromeDebug.LowLevel;

namespace ChromeDebug {
  internal static class Utility {
    public static string[] SplitArgs(string unsplitArgumentLine) {
      if (unsplitArgumentLine == null)
        return new string[0];

      int numberOfArgs;
      IntPtr ptrToSplitArgs;
      string[] splitArgs;

      ptrToSplitArgs = NativeMethods.CommandLineToArgvW(unsplitArgumentLine, out numberOfArgs);

      // CommandLineToArgvW returns NULL upon failure.
      if (ptrToSplitArgs == IntPtr.Zero)
        throw new ArgumentException("Unable to split argument.", new Win32Exception());

      // Make sure the memory ptrToSplitArgs to is freed, even upon failure.
      try {
        splitArgs = new string[numberOfArgs];

        // ptrToSplitArgs is an array of pointers to null terminated Unicode strings.
        // Copy each of these strings into our split argument array.
        for (int i = 0; i < numberOfArgs; i++)
          splitArgs[i] = Marshal.PtrToStringUni(
              Marshal.ReadIntPtr(ptrToSplitArgs, i * IntPtr.Size));

        return splitArgs;
      }
      finally {
        // Free memory obtained by CommandLineToArgW.
        NativeMethods.LocalFree(ptrToSplitArgs);
      }
    }

    public static T ReadUnmanagedStructFromProcess<T>(IntPtr processHandle,
                                                      IntPtr addressInProcess) {
      int bytesRead;
      int bytesToRead = Marshal.SizeOf(typeof(T));
      IntPtr buffer = Marshal.AllocHGlobal(bytesToRead);
      if (!NativeMethods.ReadProcessMemory(processHandle, addressInProcess, buffer, bytesToRead,
              out bytesRead))
        throw new Win32Exception();
      T result = (T)Marshal.PtrToStructure(buffer, typeof(T));
      Marshal.FreeHGlobal(buffer);
      return result;
    }

    public static string ReadStringUniFromProcess(IntPtr processHandle,
                                                  IntPtr addressInProcess,
                                                  int NumChars) {
      int bytesRead;
      IntPtr outBuffer = Marshal.AllocHGlobal(NumChars * 2);

      bool bresult = NativeMethods.ReadProcessMemory(processHandle,
                                                     addressInProcess,
                                                     outBuffer,
                                                     NumChars * 2,
                                                     out bytesRead);
      if (!bresult)
        throw new Win32Exception();

      string result = Marshal.PtrToStringUni(outBuffer, bytesRead / 2);
      Marshal.FreeHGlobal(outBuffer);
      return result;
    }

    public static int UnmanagedStructSize<T>() {
      return Marshal.SizeOf(typeof(T));
    }
  }
}
