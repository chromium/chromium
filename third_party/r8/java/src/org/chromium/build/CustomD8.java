// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import com.android.tools.r8.ByteDataView;
import com.android.tools.r8.CompilationFailedException;
import com.android.tools.r8.D8;
import com.android.tools.r8.D8Command;
import com.android.tools.r8.DesugarGraphConsumer;
import com.android.tools.r8.DexFilePerClassFileConsumer;
import com.android.tools.r8.DexIndexedConsumer;
import com.android.tools.r8.Diagnostic;
import com.android.tools.r8.DiagnosticsHandler;
import com.android.tools.r8.OutputMode;
import com.android.tools.r8.ProgramConsumer;
import com.android.tools.r8.ProgramResource;
import com.android.tools.r8.ProgramResourceProvider;
import com.android.tools.r8.ResourceException;
import com.android.tools.r8.origin.ArchiveEntryOrigin;
import com.android.tools.r8.origin.Origin;
import com.android.tools.r8.origin.PathOrigin;
import com.android.tools.r8.position.Position;
import com.android.tools.r8.utils.FlagFile;
import com.android.tools.r8.utils.OneShotByteResource;

import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Consumer;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

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
        private final Map<String, Set<String>> mDeps = new ConcurrentHashMap<>();
        private final String mFileTmpPrefix;
        private final Set<Origin> mProgramJarOrigins;
        private static final String DEP_PREFIX = "  <-  ";

        public Deps(String fileTmpPrefix, Set<Origin> programJarOrigins) {
            mFileTmpPrefix = fileTmpPrefix;
            mProgramJarOrigins = programJarOrigins;
        }

        private String formatOrigin(Origin origin) {
            if (origin instanceof ArchiveEntryOrigin
                    && mProgramJarOrigins.contains(origin.parent())) {
                return ((ArchiveEntryOrigin) origin).getEntryName();
            }
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

    private static final class JarInput {
        final Path mJarPath;
        final Origin mJarOrigin;
        final List<String> mClassEntries = new ArrayList<>();

        JarInput(Path jarPath) {
            mJarPath = jarPath;
            mJarOrigin = new PathOrigin(jarPath);
        }
    }

    private static final class FilteredJarResourceProvider implements ProgramResourceProvider {
        private final JarInput mJarInput;

        FilteredJarResourceProvider(JarInput jarInput) {
            mJarInput = jarInput;
        }

        private static boolean isClassFile(String name) {
            String lower = name.toLowerCase(Locale.ROOT);
            if (!lower.endsWith(".class")) {
                return false;
            }
            return !lower.equals("module-info.class") && !lower.endsWith("/module-info.class");
        }

        private static byte[] readEntryBytes(ZipFile zipFile, ZipEntry entry) throws IOException {
            long size = entry.getSize();
            try (InputStream in = zipFile.getInputStream(entry)) {
                if (size >= 0 && size <= Integer.MAX_VALUE) {
                    return in.readNBytes((int) size);
                }
                return in.readAllBytes();
            }
        }

        private ProgramResource createResource(ZipFile zipFile, ZipEntry entry, String entryName)
                throws IOException {
            byte[] bytes = readEntryBytes(zipFile, entry);
            String descriptor = "L" + entryName.substring(0, entryName.length() - 6) + ";";
            Origin entryOrigin = new ArchiveEntryOrigin(entryName, mJarInput.mJarOrigin);
            return OneShotByteResource.create(
                    entryOrigin, ProgramResource.Kind.CF, bytes, Collections.singleton(descriptor));
        }

        @Override
        @SuppressWarnings("deprecation")
        public Collection<ProgramResource> getProgramResources() throws ResourceException {
            List<ProgramResource> resources = new ArrayList<>();
            getProgramResources(resources::add);
            return resources;
        }

        @Override
        public void getProgramResources(Consumer<ProgramResource> consumer)
                throws ResourceException {
            try (ZipFile zipFile =
                    new ZipFile(mJarInput.mJarPath.toFile(), StandardCharsets.UTF_8)) {
                if (!mJarInput.mClassEntries.isEmpty()) {
                    for (String entryName : mJarInput.mClassEntries) {
                        ZipEntry entry = zipFile.getEntry(entryName);
                        if (entry == null) {
                            throw new ResourceException(
                                    mJarInput.mJarOrigin,
                                    "Missing entry '" + entryName + "' in " + mJarInput.mJarPath);
                        }
                        consumer.accept(createResource(zipFile, entry, entryName));
                    }
                } else {
                    Enumeration<? extends ZipEntry> entries = zipFile.entries();
                    while (entries.hasMoreElements()) {
                        ZipEntry entry = entries.nextElement();
                        String name = entry.getName();
                        if (!entry.isDirectory() && isClassFile(name)) {
                            consumer.accept(createResource(zipFile, entry, name));
                        }
                    }
                }
            } catch (IOException e) {
                throw new ResourceException(mJarInput.mJarOrigin, e);
            }
        }
    }

    /**
     * Writes a ZIP archive with uncompressed (STORED) entries and hermetic timestamps (2001-01-01
     * 00:00:00). Faster than {@link java.util.zip.ZipOutputStream} because it does not re-compute
     * CRC32s, and faster than zipflinger due to b/574603563.
     */
    private static final class StoredZipWriter implements AutoCloseable {
        // DOS date/time for 2001-01-01 00:00:00.
        private static final int HERMETIC_DOS_TIME = 0x0000;
        private static final int HERMETIC_DOS_DATE = 0x2a21;
        private static final long EXTERNAL_ATTR_0644 = 0644L << 16;
        private static final int COPY_BUFFER_SIZE = 64 * 1024;

        private static final class CdEntry {
            final byte[] mNameBytes;
            final int mFlags;
            final long mCrc;
            final long mSize;
            final long mLocalHeaderOffset;

            CdEntry(byte[] nameBytes, int flags, long crc, long size, long localHeaderOffset) {
                mNameBytes = nameBytes;
                mFlags = flags;
                mCrc = crc;
                mSize = size;
                mLocalHeaderOffset = localHeaderOffset;
            }
        }

        private final OutputStream mOut;
        private final byte[] mHeaderBuf = new byte[46];
        private final byte[] mCopyBuf = new byte[COPY_BUFFER_SIZE];
        private final CRC32 mCrc32 = new CRC32();
        private final List<CdEntry> mCdEntries = new ArrayList<>();
        private long mPosition;

        StoredZipWriter(Path outputPath) throws IOException {
            mOut = new BufferedOutputStream(Files.newOutputStream(outputPath), COPY_BUFFER_SIZE);
        }

        private static void putU16(byte[] buf, int off, int val) {
            buf[off] = (byte) val;
            buf[off + 1] = (byte) (val >>> 8);
        }

        private static void putU32(byte[] buf, int off, long val) {
            buf[off] = (byte) val;
            buf[off + 1] = (byte) (val >>> 8);
            buf[off + 2] = (byte) (val >>> 16);
            buf[off + 3] = (byte) (val >>> 24);
        }

        private static int utf8Flags(byte[] nameBytes) {
            for (byte b : nameBytes) {
                if (b < 0) {
                    return 0x0800; // Bit 11: UTF-8 filename.
                }
            }
            return 0;
        }

        private void writeLocalHeader(byte[] nameBytes, int flags, long crc, long size)
                throws IOException {
            mCdEntries.add(new CdEntry(nameBytes, flags, crc, size, mPosition));

            putU32(mHeaderBuf, 0, 0x04034b50L);
            putU16(mHeaderBuf, 4, 20);
            putU16(mHeaderBuf, 6, flags);
            putU16(mHeaderBuf, 8, 0); // STORED.
            putU16(mHeaderBuf, 10, HERMETIC_DOS_TIME);
            putU16(mHeaderBuf, 12, HERMETIC_DOS_DATE);
            putU32(mHeaderBuf, 14, crc);
            putU32(mHeaderBuf, 18, size);
            putU32(mHeaderBuf, 22, size);
            putU16(mHeaderBuf, 26, nameBytes.length);
            putU16(mHeaderBuf, 28, 0); // extraLen.
            mOut.write(mHeaderBuf, 0, 30);
            mOut.write(nameBytes);
            mPosition += 30L + nameBytes.length;
        }

        void writeEntry(String name, byte[] data) throws IOException {
            byte[] nameBytes = name.getBytes(StandardCharsets.UTF_8);
            mCrc32.reset();
            mCrc32.update(data);
            long crc = mCrc32.getValue();
            writeLocalHeader(nameBytes, utf8Flags(nameBytes), crc, data.length);
            mOut.write(data);
            mPosition += data.length;
        }

        void copyEntry(ZipFile srcZip, ZipEntry srcEntry) throws IOException {
            long crc = srcEntry.getCrc();
            long size = srcEntry.getSize();
            if (crc < 0 || size < 0) {
                try (InputStream in = srcZip.getInputStream(srcEntry)) {
                    writeEntry(srcEntry.getName(), in.readAllBytes());
                }
                return;
            }
            byte[] nameBytes = srcEntry.getName().getBytes(StandardCharsets.UTF_8);
            writeLocalHeader(nameBytes, utf8Flags(nameBytes), crc, size);
            try (InputStream in = srcZip.getInputStream(srcEntry)) {
                long remaining = size;
                while (remaining > 0) {
                    int toRead = (int) Math.min(mCopyBuf.length, remaining);
                    int n = in.read(mCopyBuf, 0, toRead);
                    if (n < 0) {
                        throw new IOException("Unexpected EOF copying " + srcEntry.getName());
                    }
                    mOut.write(mCopyBuf, 0, n);
                    remaining -= n;
                }
            }
            mPosition += size;
        }

        void finish() throws IOException {
            long cdOffset = mPosition;
            for (CdEntry e : mCdEntries) {
                putU32(mHeaderBuf, 0, 0x02014b50L);
                putU16(mHeaderBuf, 4, 0x0314); // Unix, version 2.0.
                putU16(mHeaderBuf, 6, 20);
                putU16(mHeaderBuf, 8, e.mFlags);
                putU16(mHeaderBuf, 10, 0); // STORED.
                putU16(mHeaderBuf, 12, HERMETIC_DOS_TIME);
                putU16(mHeaderBuf, 14, HERMETIC_DOS_DATE);
                putU32(mHeaderBuf, 16, e.mCrc);
                putU32(mHeaderBuf, 20, e.mSize);
                putU32(mHeaderBuf, 24, e.mSize);
                putU16(mHeaderBuf, 28, e.mNameBytes.length);
                putU16(mHeaderBuf, 30, 0); // extraLen.
                putU16(mHeaderBuf, 32, 0); // commentLen.
                putU16(mHeaderBuf, 34, 0); // diskNum.
                putU16(mHeaderBuf, 36, 0); // internalAttr.
                putU32(mHeaderBuf, 38, EXTERNAL_ATTR_0644);
                putU32(mHeaderBuf, 42, e.mLocalHeaderOffset);
                mOut.write(mHeaderBuf, 0, 46);
                mOut.write(e.mNameBytes);
                mPosition += 46L + e.mNameBytes.length;
            }
            long cdSize = mPosition - cdOffset;
            int count = mCdEntries.size();
            if (count > 0xffff || cdOffset > 0xffffffffL || cdSize > 0xffffffffL) {
                throw new IOException("ZIP64 required (entries=" + count + ")");
            }
            putU32(mHeaderBuf, 0, 0x06054b50L);
            putU16(mHeaderBuf, 4, 0);
            putU16(mHeaderBuf, 6, 0);
            putU16(mHeaderBuf, 8, count);
            putU16(mHeaderBuf, 10, count);
            putU32(mHeaderBuf, 12, cdSize);
            putU32(mHeaderBuf, 16, cdOffset);
            putU16(mHeaderBuf, 20, 0);
            mOut.write(mHeaderBuf, 0, 22);
        }

        @Override
        public void close() throws IOException {
            mOut.close();
        }
    }

    private abstract static class StoredArchiveConsumer implements ProgramConsumer {
        protected final Path mOutputPath;
        protected final Map<String, Path> mExtraFiles;

        StoredArchiveConsumer(Path outputPath, Map<String, Path> extraFiles) {
            mOutputPath = outputPath;
            mExtraFiles = extraFiles;
        }

        protected abstract void writeDexEntries(StoredZipWriter writer) throws IOException;

        void writeArchive() throws IOException {
            Map<String, byte[]> extraData = new TreeMap<>();
            for (Map.Entry<String, Path> e : mExtraFiles.entrySet()) {
                extraData.put(e.getKey(), Files.readAllBytes(e.getValue()));
            }
            try (StoredZipWriter writer = new StoredZipWriter(mOutputPath)) {
                writeDexEntries(writer);
                for (Map.Entry<String, byte[]> e : extraData.entrySet()) {
                    writer.writeEntry(e.getKey(), e.getValue());
                }
                writer.finish();
            }
        }

        @Override
        public void finished(DiagnosticsHandler handler) {
            try {
                writeArchive();
            } catch (IOException e) {
                try {
                    Files.deleteIfExists(mOutputPath);
                } catch (IOException ignored) {
                }
                final Origin origin = new PathOrigin(mOutputPath);
                handler.error(
                        new Diagnostic() {
                            @Override
                            public Origin getOrigin() {
                                return origin;
                            }

                            @Override
                            public Position getPosition() {
                                return Position.UNKNOWN;
                            }

                            @Override
                            public String getDiagnosticMessage() {
                                return "Failed to write " + mOutputPath + ": " + e;
                            }
                        });
            }
        }
    }

    private static final class PerClassArchiveConsumer extends StoredArchiveConsumer
            implements DexFilePerClassFileConsumer {
        private final Path mBaseJar;
        private final Set<String> mExcludedEntries;
        private final ConcurrentHashMap<String, byte[]> mNewEntries = new ConcurrentHashMap<>();

        PerClassArchiveConsumer(
                Path outputPath,
                Path baseJar,
                Set<String> excludedEntries,
                Map<String, Path> extraFiles) {
            super(outputPath, extraFiles);
            mBaseJar = baseJar;
            mExcludedEntries = excludedEntries;
        }

        @Override
        public void accept(
                String primaryClassDescriptor,
                ByteDataView data,
                Set<String> descriptors,
                DiagnosticsHandler handler) {
            String name =
                    primaryClassDescriptor.substring(1, primaryClassDescriptor.length() - 1)
                            + ".dex";
            mNewEntries.put(name, data.copyByteData());
        }

        @Override
        protected void writeDexEntries(StoredZipWriter writer) throws IOException {
            List<String> newNames = new ArrayList<>(mNewEntries.keySet());
            Collections.sort(newNames);
            if (mBaseJar == null) {
                for (String name : newNames) {
                    writer.writeEntry(name, mNewEntries.get(name));
                }
                return;
            }
            try (ZipFile baseZip = new ZipFile(mBaseJar.toFile(), StandardCharsets.UTF_8)) {
                List<ZipEntry> keptBase = new ArrayList<>();
                Enumeration<? extends ZipEntry> entries = baseZip.entries();
                while (entries.hasMoreElements()) {
                    ZipEntry entry = entries.nextElement();
                    String name = entry.getName();
                    if (!entry.isDirectory()
                            && name.endsWith(".dex")
                            && !mNewEntries.containsKey(name)
                            && !mExcludedEntries.contains(name)
                            && !mExtraFiles.containsKey(name)) {
                        keptBase.add(entry);
                    }
                }
                keptBase.sort(Comparator.comparing(ZipEntry::getName));
                int i = 0;
                int j = 0;
                while (i < newNames.size() && j < keptBase.size()) {
                    String newName = newNames.get(i);
                    ZipEntry baseEntry = keptBase.get(j);
                    if (newName.compareTo(baseEntry.getName()) <= 0) {
                        writer.writeEntry(newName, mNewEntries.get(newName));
                        i++;
                    } else {
                        writer.copyEntry(baseZip, baseEntry);
                        j++;
                    }
                }
                while (i < newNames.size()) {
                    String newName = newNames.get(i++);
                    writer.writeEntry(newName, mNewEntries.get(newName));
                }
                while (j < keptBase.size()) {
                    writer.copyEntry(baseZip, keptBase.get(j++));
                }
            }
        }
    }

    private static final class IndexedArchiveConsumer extends StoredArchiveConsumer
            implements DexIndexedConsumer {
        private final ConcurrentHashMap<Integer, byte[]> mNewEntries = new ConcurrentHashMap<>();

        IndexedArchiveConsumer(Path outputPath, Map<String, Path> extraFiles) {
            super(outputPath, extraFiles);
        }

        @Override
        public void accept(
                int fileIndex,
                ByteDataView data,
                Set<String> descriptors,
                DiagnosticsHandler handler) {
            mNewEntries.put(fileIndex, data.copyByteData());
        }

        @Override
        protected void writeDexEntries(StoredZipWriter writer) throws IOException {
            List<Integer> indices = new ArrayList<>(mNewEntries.keySet());
            Collections.sort(indices);
            for (int idx : indices) {
                String name = idx == 0 ? "classes.dex" : "classes" + (idx + 1) + ".dex";
                writer.writeEntry(name, mNewEntries.get(idx));
            }
        }
    }

    private static boolean isZipOrJar(Path path) {
        String name = path.getFileName().toString().toLowerCase(Locale.ROOT);
        return name.endsWith(".jar") || name.endsWith(".zip");
    }

    // Entry point for D8 compilation with support for --desugar-dependencies option
    // as well.
    public static void main(String[] args) throws CompilationFailedException, IOException {
        // Need to expand argfile arg in case our custom command line args are in the file.
        String[] expandedArgs = FlagFile.expandFlagFiles(args, null);
        List<String> argList = new ArrayList<>(Arrays.asList(expandedArgs));
        String desugarDependenciesPath = parseAndRemoveArg(argList, "--desugar-dependencies");
        String fileTmpPrefix = parseAndRemoveArg(argList, "--file-tmp-prefix");

        Path baseJar = null;
        List<JarInput> jarInputs = new ArrayList<>();
        Set<String> excludedEntries = new HashSet<>();
        Map<String, Path> extraFiles = new LinkedHashMap<>();
        List<String> d8Args = new ArrayList<>(argList.size());

        for (int i = 0; i < argList.size(); i++) {
            String arg = argList.get(i);
            if ("--base-jar".equals(arg)
                    || "--jar-input".equals(arg)
                    || "--jar-class".equals(arg)
                    || "--exclude-entry".equals(arg)
                    || "--add-file".equals(arg)) {
                if (i == argList.size() - 1) {
                    throw new CompilationFailedException("Missing argument to '" + arg + "'");
                }
                String value = argList.get(++i);
                switch (arg) {
                    case "--base-jar":
                        baseJar = Paths.get(value);
                        break;
                    case "--jar-input":
                        jarInputs.add(new JarInput(Paths.get(value)));
                        break;
                    case "--jar-class":
                        if (jarInputs.isEmpty()) {
                            throw new CompilationFailedException(
                                    "--jar-class must follow --jar-input");
                        }
                        jarInputs.get(jarInputs.size() - 1).mClassEntries.add(value);
                        break;
                    case "--exclude-entry":
                        excludedEntries.add(value);
                        break;
                    case "--add-file":
                        int eq = value.indexOf('=');
                        if (eq <= 0 || eq == value.length() - 1) {
                            throw new CompilationFailedException(
                                    "Expected <zip_name>=<file_path> for --add-file, got: "
                                            + value);
                        }
                        extraFiles.put(value.substring(0, eq), Paths.get(value.substring(eq + 1)));
                        break;
                }
            } else {
                d8Args.add(arg);
            }
        }

        // Use D8 command line parser to handle the normal D8 command line.
        D8Command.Builder builder =
                D8Command.parse(d8Args.toArray(new String[0]), new CommandLineOrigin());

        Set<Origin> programJarOrigins = new HashSet<>();
        for (JarInput ji : jarInputs) {
            programJarOrigins.add(ji.mJarOrigin);
            builder.addProgramResourceProvider(new FilteredJarResourceProvider(ji));
        }

        Path outputPath = builder.getOutputPath();
        OutputMode outputMode = builder.getOutputMode();
        if (baseJar != null
                && outputPath != null
                && baseJar.toAbsolutePath()
                        .normalize()
                        .equals(outputPath.toAbsolutePath().normalize())) {
            throw new CompilationFailedException("--base-jar and --output must be different files");
        }
        if (outputPath != null && isZipOrJar(outputPath)) {
            if (outputMode == OutputMode.DexFilePerClassFile) {
                builder.setProgramConsumer(
                        new PerClassArchiveConsumer(
                                outputPath, baseJar, excludedEntries, extraFiles));
            } else if (outputMode == OutputMode.DexIndexed) {
                if (baseJar != null || !excludedEntries.isEmpty()) {
                    throw new CompilationFailedException(
                            "--base-jar and --exclude-entry require --file-per-class-file");
                }
                builder.setProgramConsumer(new IndexedArchiveConsumer(outputPath, extraFiles));
            } else if (baseJar != null || !excludedEntries.isEmpty() || !extraFiles.isEmpty()) {
                throw new CompilationFailedException(
                        "--base-jar, --exclude-entry, and --add-file are not supported with "
                                + outputMode);
            }
        } else if (baseJar != null || !excludedEntries.isEmpty() || !extraFiles.isEmpty()) {
            throw new CompilationFailedException(
                    "--base-jar, --exclude-entry, and --add-file require .jar/.zip --output");
        }

        if (desugarDependenciesPath != null) {
            final Path desugarDependencies = Paths.get(desugarDependenciesPath);
            if (builder.getDesugarGraphConsumer() != null) {
                throw new CompilationFailedException("Too many desugar graph consumers.");
            }
            Deps deps = new Deps(fileTmpPrefix, programJarOrigins);
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
