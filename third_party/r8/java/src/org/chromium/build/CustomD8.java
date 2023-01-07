// Copyright 2021 The Chromium Authors
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
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

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

    private static class Deps implements DesugarGraphConsumer {
        private Map<String, Set<String>> mDeps = new ConcurrentHashMap<>();
        private String mFileTmpPrefix;
        private static final String DEP_PREFIX = "  <-  ";

        public Deps(String fileTmpPrefix) {
            mFileTmpPrefix = fileTmpPrefix;
        }

        private String formatOrigin(Origin origin) {
            String path = origin.toString();
            // Class files are extracted to a temporary directory for incremental dexing.
            // Remove the prefix of the path corresponding to the temporary directory so
            // that these paths are consistent between builds.
            if (mFileTmpPrefix != null && path.startsWith(mFileTmpPrefix)) {
                return path.substring(mFileTmpPrefix.length());
            }
            return path;
        }

        @Override
        public void acceptProgramNode(Origin node) {
            String potentialDependent = formatOrigin(node);
            // Removing all nodes that D8 already knows about so that only those that are still
            // relevant (added via calls to accept) are kept. Deletes stale nodes.
            mDeps.remove(potentialDependent);
        }

        @Override
        public void accept(Origin dependentOrigin, Origin dependencyOrigin) {
            String dependent = formatOrigin(dependentOrigin);
            String dependency = formatOrigin(dependencyOrigin);
            add(dependent, dependency);
        }

        private void add(String dependent, String dependency) {
            mDeps.computeIfAbsent(dependent, k -> ConcurrentHashMap.newKeySet()).add(dependency);
        }

        @Override
        public void finished() {}

        private void loadFromFile(Path path) throws IOException {
            String dependent = null;
            for (String line : Files.readAllLines(path)) {
                if (line.startsWith(DEP_PREFIX)) {
                    add(dependent, line.substring(DEP_PREFIX.length()));
                } else {
                    dependent = line;
                }
            }
        }

        @Override
        public String toString() {
            StringBuilder builder = new StringBuilder();
            for (String dependent : sorted(mDeps.keySet())) {
                builder.append(dependent).append("\n");
                for (String dependency : sorted(mDeps.get(dependent))) {
                    builder.append(DEP_PREFIX).append(dependency).append("\n");
                }
            }
            return builder.toString();
        }

        private static List<String> sorted(Set<String> set) {
            List<String> list = new ArrayList<>(set);
            Collections.sort(list);
            return list;
        }
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
            if (builder.getDesugarGraphConsumer() != null) {
                throw new CompilationFailedException("Too many desugar graph consumers.");
            }
            Deps deps = new Deps(fileTmpPrefix);
            if (Files.exists(desugarDependencies)) {
                deps.loadFromFile(desugarDependencies);
            }
            builder.setDesugarGraphConsumer(deps);
            // Run D8 to create/update the graph before writing deps to the file.
            D8.run(builder.build());
            try (PrintWriter desugarDependenciesPrintWriter =
                            new PrintWriter(Files.newOutputStream(desugarDependencies))) {
                desugarDependenciesPrintWriter.println(deps.toString());
            }
        } else {
            D8.run(builder.build());
        }
    }
}
