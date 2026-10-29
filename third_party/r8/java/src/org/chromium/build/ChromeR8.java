// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import com.android.tools.r8.CompilationFailedException;
import com.android.tools.r8.Disassemble;
import com.android.tools.r8.R8;
import com.android.tools.r8.R8Command;
import com.android.tools.r8.origin.Origin;
import com.android.tools.r8.tracereferences.TraceReferences;
import com.android.tools.r8.utils.FlagFile;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Wrapper around R8, ChromeD8 (--d8), TraceReferences (--tracereferences), FlushingReTrace
 * (--retrace), and Disassemble (--disassemble).
 *
 * <p>Note: Compiled standalone by //third_party/r8/3pp/3pp.py with only r8.jar on the classpath, so
 * do not depend on //build/android:build_java (e.g. @NullMarked).
 */
public class ChromeR8 {
    private static class CommandLineOrigin extends Origin {
        private CommandLineOrigin() {
            super(root());
        }

        @Override
        public String part() {
            return "Command line";
        }
    }

    // Entry point for R8 compilation with support for some experimental command line flags, as
    // well as subcommands that share the R8 jar (--d8, --tracereferences, --retrace,
    // --disassemble).
    public static void main(String[] args)
            throws CompilationFailedException, ExecutionException, IOException {
        // Need to expand argfile arg in case our custom command line args are in the file.
        String[] expandedArgs = FlagFile.expandFlagFiles(args, null);
        List<String> argList = new ArrayList<>(Arrays.asList(expandedArgs));
        // Allow -Dfoo=bar after the main class (e.g. when invoked via graalvm.py's tracing agent).
        Iterator<String> it = argList.iterator();
        while (it.hasNext()) {
            String arg = it.next();
            if (arg.startsWith("-D")) {
                it.remove();
                int eq = arg.indexOf('=', 2);
                if (eq != -1) {
                    System.setProperty(arg.substring(2, eq), arg.substring(eq + 1));
                } else {
                    System.setProperty(arg.substring(2), "");
                }
            }
        }
        if (argList.remove("--d8")) {
            ChromeD8.main(argList.toArray(new String[0]));
            return;
        }
        if (argList.remove("--tracereferences")) {
            TraceReferences.main(argList.toArray(new String[0]));
            return;
        }
        if (argList.remove("--retrace")) {
            FlushingReTrace.main(argList.toArray(new String[0]));
            return;
        }
        if (argList.remove("--disassemble")) {
            Disassemble.main(argList.toArray(new String[0]));
            return;
        }
        boolean startupLayoutOptimization = argList.remove("--enable-startup-layout-optimization");

        // Use R8 command line parser to handle the normal R8 command line.
        R8Command.Builder builder =
                R8Command.parse(argList.toArray(new String[0]), new CommandLineOrigin());
        if (startupLayoutOptimization) {
            builder.setEnableStartupLayoutOptimization(true);
        }
        R8.run(builder.build());
    }
}
