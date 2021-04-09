// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import com.android.tools.r8.CompilationFailedException;
import com.android.tools.r8.D8;
import com.android.tools.r8.D8Command;
import com.android.tools.r8.DesugarGraphConsumer;
import com.android.tools.r8.origin.Origin;
import com.android.tools.r8.utils.FlagFile;

import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class CustomD8 {
    private static class CommandLineOrigin extends Origin {
        private CommandLineOrigin() {
            super(root());
        }

        @Override
        public String part() {
            return "Command line";
        }
    }

    private static String parseAndRemoveArg(List<String> args, String name)
            throws CompilationFailedException {
        int idx = args.indexOf(name);
        if (idx == -1) {
            return null;
        }
        if (idx == args.size() - 1) {
            throw new CompilationFailedException("Missing argument to '" + name + "'");
        }
        String value = args.get(idx + 1);
        args.subList(idx, idx + 2).clear();
        return value;
    }

    // Entry point for D8 compilation with support for --desugar-dependencies option
    // as well.
    public static void main(String[] args) throws CompilationFailedException, IOException {
        // Need to expand argfile arg in case our custom command line args are in the file.
        String[] expandedArgs = FlagFile.expandFlagFiles(args, null);
        List<String> argList = new ArrayList<>(Arrays.asList(expandedArgs));
        String desugarDependenciesPath = parseAndRemoveArg(argList, "--desugar-dependencies");
        String fileTmpPrefix = parseAndRemoveArg(argList, "--file-tmp-prefix");

        // Use D8 command line parser to handle the normal D8 command line.
        D8Command.Builder builder =
                D8Command.parse(argList.toArray(new String[0]), new CommandLineOrigin());

        if (desugarDependenciesPath != null) {
            final Path desugarDependencies = Paths.get(desugarDependenciesPath);
            PrintWriter desugarDependenciesPrintWriter =
                    new PrintWriter(Files.newOutputStream(desugarDependencies));
            if (builder.getDesugarGraphConsumer() != null) {
                throw new CompilationFailedException("Too many desugar graph consumers.");
            }
            builder.setDesugarGraphConsumer(new DesugarGraphConsumer() {
                private String formatOrigin(Origin origin) {
                    String path = origin.toString();
                    // Class files are extracted to a temporary directory for incremental dexing.
                    // Remove the prefix of the path corresponding to the temporary directory so
                    // that these paths are consistent between builds.
                    if (fileTmpPrefix != null && path.startsWith(fileTmpPrefix)) {
                        return path.substring(fileTmpPrefix.length());
                    }
                    return path;
                }

                @Override
                public void accept(Origin dependent, Origin dependency) {
                    String dependentPath = formatOrigin(dependent);
                    String dependencyPath = formatOrigin(dependency);
                    synchronized (desugarDependenciesPrintWriter) {
                        desugarDependenciesPrintWriter.println(
                                dependentPath + " -> " + dependencyPath);
                    }
                }

                @Override
                public void finished() {
                    desugarDependenciesPrintWriter.close();
                }
            });
        }

        // Run D8.
        D8.run(builder.build());
    }
}
