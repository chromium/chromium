// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import com.android.tools.r8.CompilationFailedException;
import com.android.tools.r8.R8;
import com.android.tools.r8.R8Command;
import com.android.tools.r8.origin.Origin;
import com.android.tools.r8.utils.FlagFile;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class CustomR8 {
    private static class CommandLineOrigin extends Origin {
        private CommandLineOrigin() {
            super(root());
        }

        @Override
        public String part() {
            return "Command line";
        }
    }

    // Entry point for R8 compilation with support for some experimental command line flags.
    public static void main(String[] args) throws CompilationFailedException, IOException {
        // Need to expand argfile arg in case our custom command line args are in the file.
        String[] expandedArgs = FlagFile.expandFlagFiles(args, null);
        List<String> argList = new ArrayList<>(Arrays.asList(expandedArgs));
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
