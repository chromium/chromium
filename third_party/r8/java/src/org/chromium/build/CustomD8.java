// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import com.android.tools.r8.CompilationFailedException;
import com.android.tools.r8.D8;
import com.android.tools.r8.D8Command;
import com.android.tools.r8.DesugarGraphConsumer;
import com.android.tools.r8.origin.Origin;

import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

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

    // Entry point for D8 compilation with support for --desugar-dependencies option
    // as well.
    public static void main(String[] args) throws CompilationFailedException, IOException {
        String desugarDependenciesOptions = "--desugar-dependencies";
        String desugarDependenciesPath = null;
        String[] d8Args = null;

        int desugarDepIdx = Arrays.asList(args).indexOf(desugarDependenciesOptions);
        if (desugarDepIdx != -1) {
            int numRemainingArgs = args.length - (desugarDepIdx + 2);
            if (numRemainingArgs < 0) {
                throw new CompilationFailedException(
                        "Missing argument to '" + desugarDependenciesOptions + "'");
            }
            desugarDependenciesPath = args[desugarDepIdx + 1];
            d8Args = new String[args.length - 2];
            // Copy over all other args before and after the desugar dependencies arg.
            System.arraycopy(args, 0, d8Args, 0, desugarDepIdx);
            System.arraycopy(args, desugarDepIdx + 2, d8Args, desugarDepIdx, numRemainingArgs);
        } else {
            d8Args = args;
        }

        // Use D8 command line parser to handle the normal D8 command line.
        D8Command.Builder builder = D8Command.parse(d8Args, new CommandLineOrigin());
        // If additional options was passed amend the D8 command builder.
        if (desugarDependenciesPath != null) {
            final Path desugarDependencies = Paths.get(desugarDependenciesPath);
            PrintWriter desugarDependenciesPrintWriter =
                    new PrintWriter(Files.newOutputStream(desugarDependencies));
            if (builder.getDesugarGraphConsumer() != null) {
                throw new CompilationFailedException("Too many desugar graph consumers.");
            }
            builder.setDesugarGraphConsumer(new DesugarGraphConsumer() {
                @Override
                public void accept(Origin dependent, Origin dependency) {
                    // The target's class files have root as their parent.
                    if (dependency.parent().equals(Origin.root())) {
                        return;
                    }
                    desugarDependenciesPrintWriter.println(dependency.parent());
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
