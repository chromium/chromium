// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import static java.nio.charset.StandardCharsets.UTF_8;

import com.sun.tools.javac.file.FSInfo;
import com.sun.tools.javac.file.JavacFileManager;
import com.sun.tools.javac.file.PathFileObject.CannotCreateUriError;
import com.sun.tools.javac.main.Option;
import com.sun.tools.javac.util.Context;

import java.io.ByteArrayInputStream;
import java.io.Closeable;
import java.io.EOFException;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.Reader;
import java.io.Writer;
import java.net.URI; // nocheck
import java.net.URISyntaxException;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.CharsetDecoder;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.time.DateTimeException;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Comparator;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.zip.DataFormatException;
import java.util.zip.Inflater;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;

import javax.lang.model.SourceVersion;
import javax.lang.model.element.Modifier;
import javax.lang.model.element.NestingKind;
import javax.tools.FileObject;
import javax.tools.JavaFileObject;
import javax.tools.JavaFileObject.Kind;
import javax.tools.StandardLocation;

/**
 * A {@link JavacFileManager} subclass that lazily indexes {@link StandardLocation#CLASS_PATH} JAR
 * files directly from their raw ZIP central directories instead of opening every JAR through {@code
 * ZipFileSystem} and running {@code Files.walkFileTree}.
 *
 * <p>Directories, non-JAR entries, ZIP64/comment/stub/encrypted archives, and Multi-Release JARs
 * (when {@code --release} / {@code multi-release} is active) delegate cleanly to {@code super} on a
 * per-entry basis via synthetic {@link FallbackLocation}s while preserving exact classpath order.
 * All locations other than {@link StandardLocation#CLASS_PATH} delegate directly to {@code super}.
 */
public class LazyJavacFileManager extends JavacFileManager {
    // ZIP record signatures (little-endian 32-bit values).
    private static final long LOCSIG = 0x04034b50L;
    private static final long CENSIG = 0x02014b50L;
    private static final long ENDSIG = 0x06054b50L;

    // ZIP header layout offsets and sizes.
    private static final int ENDHDR = 22;
    private static final int CENHDR = 46;
    private static final int LOCHDR = 30;
    private static final int ENDDSK = 4;
    private static final int ENDSTR = 6;
    private static final int ENDTOT = 10;
    private static final int ENDSIZ = 12;
    private static final int ENDOFF = 16;
    private static final int ENDCOM = 20;
    private static final int CENFLG = 8;
    private static final int CENHOW = 10;
    private static final int CENTIM = 12;
    private static final int CENSIZ = 20;
    private static final int CENLEN = 24;
    private static final int CENNAM = 28;
    private static final int CENEXT = 30;
    private static final int CENCOM = 32;
    private static final int CENOFF = 42;
    private static final int LOCNAM = 26;
    private static final int LOCEXT = 28;
    private static final int ZIP64_MAGICCOUNT = 0xFFFF;
    private static final long ZIP64_MAGICVAL = 0xFFFFFFFFL;

    // Leave 256 KiB headroom below Integer.MAX_VALUE so LOCHDR (30) + max LOCNAM (65535) +
    // max LOCEXT (65535) + LOCAL_HEADER_SLACK (128) + compSize can never overflow int, and
    // byte[] allocations never exceed VM array size limits (Integer.MAX_VALUE - 8).
    private static final long MAX_ENTRY_SIZE = Integer.MAX_VALUE - 0x40000;

    // Covers EOCD + entire CEN in one pread64 for >95% of Chromium interface JARs while keeping
    // scratch memory small.
    private static final int TAIL_READ_SIZE = 32 * 1024;
    // Extra bytes read speculatively with a local header so entries whose local extra field is
    // slightly larger than their CEN extra field still complete in a single pread64.
    private static final int LOCAL_HEADER_SLACK = 128;

    private static final byte[] META_INF_MANIFEST = "META-INF/MANIFEST.MF".getBytes(UTF_8);
    private static final byte[] META_INF_VERSIONS = "META-INF/versions/".getBytes(UTF_8);
    private static final byte[] DOT_CLASS = ".class".getBytes(UTF_8);
    private static final byte[] DOT_JAVA = ".java".getBytes(UTF_8);
    private static final byte[] DOT_HTML = ".html".getBytes(UTF_8);

    private final Object mInflaterLock = new Object();
    private FSInfo mCachedFsInfo;
    private ClassPathIndex mClassPathIndex;
    private Inflater mSharedInflater;

    public LazyJavacFileManager(Context context) {
        super(context, /* register= */ true, null);
    }

    @Override
    public void setContext(Context context) {
        super.setContext(context);
        mCachedFsInfo = FSInfo.instance(context);
        invalidateClassPathIndex();
    }

    private void invalidateClassPathIndex() {
        if (mClassPathIndex != null) {
            try {
                mClassPathIndex.close();
            } catch (IOException ignored) {
            }
            mClassPathIndex = null;
        }
    }

    private ClassPathIndex getClassPathIndex() throws IOException {
        if (mClassPathIndex == null) {
            mClassPathIndex = new ClassPathIndex(getLocationAsPaths(StandardLocation.CLASS_PATH));
        }
        return mClassPathIndex;
    }

    private InputStream inflate(byte[] buf, int off, int len, int uncompSize) throws IOException {
        synchronized (mInflaterLock) {
            if (mSharedInflater == null) {
                mSharedInflater = new Inflater(/* nowrap= */ true);
            }
            mSharedInflater.reset();
            mSharedInflater.setInput(buf, off, len);
            byte[] out = new byte[Math.max(uncompSize, 1)];
            int total = 0;
            try {
                while (!mSharedInflater.finished()) {
                    if (total == out.length) {
                        if (out.length >= MAX_ENTRY_SIZE) {
                            throw new ZipException("Inflated entry exceeds max size");
                        }
                        int newCap =
                                (int) Math.min(MAX_ENTRY_SIZE, Math.max((long) out.length * 2, 64));
                        out = Arrays.copyOf(out, newCap);
                    }
                    int n = mSharedInflater.inflate(out, total, out.length - total);
                    if (n == 0) {
                        if (mSharedInflater.finished()) {
                            break;
                        }
                        throw new EOFException("Unexpected end of ZLIB input stream");
                    }
                    total += n;
                }
            } catch (DataFormatException e) {
                throw new ZipException(e.getMessage());
            }
            return new ByteArrayInputStream(out, 0, total);
        }
    }

    @Override
    public boolean handleOption(Option option, String value) {
        boolean ok = super.handleOption(option, value);
        if (option == Option.CLASS_PATH || option == Option.MULTIRELEASE) {
            invalidateClassPathIndex();
        }
        return ok;
    }

    @Override
    public void setLocation(Location location, Iterable<? extends File> path) throws IOException {
        super.setLocation(location, path);
        if (location == StandardLocation.CLASS_PATH) {
            invalidateClassPathIndex();
        }
    }

    @Override
    public void setLocationFromPaths(Location location, Collection<? extends Path> path)
            throws IOException {
        super.setLocationFromPaths(location, path);
        if (location == StandardLocation.CLASS_PATH) {
            invalidateClassPathIndex();
        }
    }

    @Override
    public Iterable<JavaFileObject> list(
            Location location, String packageName, Set<Kind> kinds, boolean recurse)
            throws IOException {
        if (location != StandardLocation.CLASS_PATH) {
            return super.list(location, packageName, kinds, recurse);
        }
        Objects.requireNonNull(packageName);
        Objects.requireNonNull(kinds);
        return getClassPathIndex().list(packageName, kinds, recurse);
    }

    @Override
    public String inferBinaryName(Location location, JavaFileObject file) {
        if (file instanceof LazyJarFileObject lfo) {
            Objects.requireNonNull(location);
            if (location.isModuleOrientedLocation()) {
                throw new IllegalArgumentException(
                        "location is module-oriented: " + location.getName());
            }
            if (getLocationAsPaths(location) == null) {
                return null;
            }
            return lfo.inferBinaryName();
        }
        return super.inferBinaryName(location, file);
    }

    @Override
    public boolean isSameFile(FileObject a, FileObject b) {
        Objects.requireNonNull(a);
        Objects.requireNonNull(b);
        if (a instanceof LazyJarFileObject && b instanceof LazyJarFileObject) {
            return a.equals(b);
        }
        if (a instanceof LazyJarFileObject || b instanceof LazyJarFileObject) {
            return a.toUri().equals(b.toUri());
        }
        return super.isSameFile(a, b);
    }

    @Override
    public boolean contains(Location location, FileObject fo) throws IOException {
        if (fo instanceof LazyJarFileObject lfo) {
            Objects.requireNonNull(location);
            if (!hasLocation(location)) {
                // Delegate to super with a real PathFileObject to preserve exact unknown-location
                // IllegalArgumentException semantics vs unset standard locations.
                return super.contains(location, getJavaFileObject(lfo.mJar.mCanonicalPath));
            }
            if (location.isModuleOrientedLocation()) {
                for (Set<Location> modLocs : listLocationsForModules(location)) {
                    for (Location modLoc : modLocs) {
                        if (contains(modLoc, lfo)) {
                            return true;
                        }
                    }
                }
                return false;
            }
            Collection<? extends Path> paths = getLocationAsPaths(location);
            if (paths == null) {
                return false;
            }
            Path jarCanon = lfo.mJar.mCanonicalPath;
            for (Path p : paths) {
                if (p.equals(lfo.mJar.mUserPath)
                        || mCachedFsInfo.getCanonicalFile(p).equals(jarCanon)) {
                    return true;
                }
            }
            return false;
        }
        return super.contains(location, fo);
    }

    @Override
    public JavaFileObject getJavaFileForInput(Location location, String className, Kind kind)
            throws IOException {
        if (location != StandardLocation.CLASS_PATH) {
            return super.getJavaFileForInput(location, className, kind);
        }
        Objects.requireNonNull(className);
        Objects.requireNonNull(kind);
        if (kind != Kind.SOURCE && kind != Kind.CLASS) {
            throw new IllegalArgumentException("Invalid kind: " + kind);
        }
        int lastDot = className.lastIndexOf('.');
        String pkgSlash = lastDot < 0 ? "" : className.substring(0, lastDot).replace('.', '/');
        String baseName =
                (lastDot < 0 ? className : className.substring(lastDot + 1)) + kind.extension;
        return (JavaFileObject)
                getClassPathIndex()
                        .getFile(
                                pkgSlash,
                                baseName,
                                fb -> super.getJavaFileForInput(fb, className, kind));
    }

    @Override
    public FileObject getFileForInput(Location location, String packageName, String relativeName)
            throws IOException {
        if (location != StandardLocation.CLASS_PATH) {
            return super.getFileForInput(location, packageName, relativeName);
        }
        Objects.requireNonNull(packageName);
        if (!isRelativeUri(relativeName) || relativeName.endsWith("/")) {
            throw new IllegalArgumentException("Invalid relative name: " + relativeName);
        }
        String full =
                packageName.isEmpty()
                        ? relativeName
                        : packageName.replace('.', '/') + "/" + relativeName;
        int lastSlash = full.lastIndexOf('/');
        String pkgPath = lastSlash < 0 ? "" : full.substring(0, lastSlash);
        String baseName = lastSlash < 0 ? full : full.substring(lastSlash + 1);
        return getClassPathIndex()
                .getFile(
                        pkgPath,
                        baseName,
                        fb -> super.getFileForInput(fb, packageName, relativeName));
    }

    @Override
    public void flush() {
        invalidateClassPathIndex();
        super.flush();
    }

    @Override
    public void close() throws IOException {
        if (deferredCloseTimeout > 0) {
            super.close();
            return;
        }
        invalidateClassPathIndex();
        synchronized (mInflaterLock) {
            if (mSharedInflater != null) {
                mSharedInflater.end();
                mSharedInflater = null;
            }
        }
        super.close();
    }

    /**
     * Synthetic non-output location used to delegate a single fallback classpath entry to super.
     */
    private record FallbackLocation(int cpIndex) implements Location {
        @Override
        public String getName() {
            return "LAZY_CP_FALLBACK_" + cpIndex;
        }

        @Override
        public boolean isOutputLocation() {
            return false;
        }
    }

    @FunctionalInterface
    private interface FallbackLookup {
        FileObject lookup(FallbackLocation fb) throws IOException;
    }

    private record OrderedFileObject(int cpIndex, JavaFileObject fo) {}

    /**
     * Package-level index over all {@link StandardLocation#CLASS_PATH} paths.
     *
     * <p>Note: duplicate entry names within a single JAR are not deduplicated; standard JARs have
     * unique entries, keeping indexing linear with zero per-entry object allocation.
     */
    private final class ClassPathIndex implements Closeable {
        private final IndexedJar[] mJars;
        private final List<FallbackLocation> mFallbacks = new ArrayList<>();

        // Open-addressing hash table mapping raw UTF-8 package bytes -> pkgId. Representative
        // bytes for pkgId are read from mPkgHead[pkgId] in mJars[mEntryJar[head]].mCenBytes.
        private int[] mPkgTable;
        private int mNumPkgs;
        private int[] mPkgRepLen;
        private int[] mPkgHash;
        private int[] mPkgHead;
        private int[] mPkgTail;

        // Parallel linked-list arrays of indexed entries across all IndexedJars.
        private int mNumEntries;
        private int[] mEntryJar;
        private int[] mEntryCen;
        private int[] mEntryNext;

        ClassPathIndex(Collection<? extends Path> paths) throws IOException {
            int n = paths == null ? 0 : paths.size();
            int pkgCap = 16;
            if (n > 0) {
                pkgCap = 64;
                while (pkgCap < n * 4) {
                    pkgCap <<= 1;
                }
            }
            mJars = new IndexedJar[n];
            mPkgTable = new int[pkgCap];
            Arrays.fill(mPkgTable, -1);
            int pkgArrCap = Math.max(pkgCap / 2, 16);
            mPkgRepLen = new int[pkgArrCap];
            mPkgHash = new int[pkgArrCap];
            mPkgHead = new int[pkgArrCap];
            mPkgTail = new int[pkgArrCap];
            int entryCap = n == 0 ? 0 : Math.max(n * 32, 64);
            mEntryJar = new int[entryCap];
            mEntryCen = new int[entryCap];
            mEntryNext = new int[entryCap];
            if (n == 0) {
                return;
            }
            byte[] scratch = new byte[TAIL_READ_SIZE];
            int[] jarCens = new int[256];
            int cpIndex = 0;
            int jarCount = 0;
            for (Path path : paths) {
                int curCp = cpIndex++;
                if (!mCachedFsInfo.exists(path)) {
                    continue;
                }
                if (!mCachedFsInfo.isDirectory(path)) {
                    String fileName = path.getFileName().toString();
                    if (fileName.endsWith(".jar") || fileName.endsWith(".zip")) {
                        Path canon = mCachedFsInfo.getCanonicalFile(path);
                        IndexedJar jar = IndexedJar.tryOpen(curCp, path, canon, scratch);
                        if (jar != null) {
                            int maxEntries = jar.mCenBytes.length / CENHDR;
                            if (jarCens.length < maxEntries) {
                                jarCens = new int[Math.max(jarCens.length * 2, maxEntries)];
                            }
                            int count = jar.collectEntries(multiReleaseValue != null, jarCens);
                            if (count >= 0) {
                                int jarIdx = jarCount++;
                                mJars[jarIdx] = jar;
                                addJarEntries(jarIdx, jar.mCenBytes, jarCens, count);
                                continue;
                            }
                        }
                    }
                }
                // Delegate directories, multi-release JARs, ZIP64/comment/stub/corrupt archives,
                // non-JAR archives, etc. to stock JavacFileManager.
                FallbackLocation fb = new FallbackLocation(curCp);
                LazyJavacFileManager.super.setLocationFromPaths(fb, List.of(path));
                mFallbacks.add(fb);
            }
        }

        private void addJarEntries(int jarIdx, byte[] cen, int[] cens, int count) {
            int prevPkgOff = -1;
            int prevPkgLen = -1;
            int prevPkgId = -2; // -2: none, -1: invalid package
            for (int i = 0; i < count; i++) {
                int c = cens[i];
                int nameOff = c + CENHDR;
                int nameLen = u2(cen, c + CENNAM);
                int lastSlash = lastIndexOf(cen, nameOff, nameOff + nameLen, (byte) '/');
                int pkgLen = lastSlash < 0 ? 0 : lastSlash - nameOff;
                int pkgId;
                if (prevPkgId != -2
                        && pkgLen == prevPkgLen
                        && (pkgLen == 0
                                || Arrays.equals(
                                        cen,
                                        nameOff,
                                        nameOff + pkgLen,
                                        cen,
                                        prevPkgOff,
                                        prevPkgOff + prevPkgLen))) {
                    pkgId = prevPkgId;
                } else {
                    if (pkgLen > 0 && !isValidPackage(cen, nameOff, pkgLen)) {
                        pkgId = -1;
                    } else {
                        pkgId = internPackage(cen, nameOff, pkgLen);
                    }
                    prevPkgOff = nameOff;
                    prevPkgLen = pkgLen;
                    prevPkgId = pkgId;
                }
                if (pkgId >= 0) {
                    appendEntry(pkgId, jarIdx, c);
                }
            }
        }

        private void appendEntry(int pkgId, int jarIdx, int cenOffset) {
            int e = mNumEntries++;
            if (e >= mEntryJar.length) {
                int newCap = Math.max(mEntryJar.length * 2, 64);
                mEntryJar = Arrays.copyOf(mEntryJar, newCap);
                mEntryCen = Arrays.copyOf(mEntryCen, newCap);
                mEntryNext = Arrays.copyOf(mEntryNext, newCap);
            }
            mEntryJar[e] = jarIdx;
            mEntryCen[e] = cenOffset;
            mEntryNext[e] = -1;
            int tail = mPkgTail[pkgId];
            if (tail == -1) {
                mPkgHead[pkgId] = e;
            } else {
                mEntryNext[tail] = e;
            }
            mPkgTail[pkgId] = e;
        }

        private int findPkgSlot(byte[] bytes, int off, int len, int hash) {
            int mask = mPkgTable.length - 1;
            int slot = hash & mask;
            while (true) {
                int id = mPkgTable[slot];
                if (id == -1) {
                    return slot;
                }
                if (mPkgHash[id] == hash && mPkgRepLen[id] == len) {
                    int head = mPkgHead[id];
                    byte[] rep = mJars[mEntryJar[head]].mCenBytes;
                    int repOff = mEntryCen[head] + CENHDR;
                    if (Arrays.equals(rep, repOff, repOff + len, bytes, off, off + len)) {
                        return slot;
                    }
                }
                slot = (slot + 1) & mask;
            }
        }

        private int internPackage(byte[] cen, int off, int len) {
            int hash = hashBytes(cen, off, len);
            int slot = findPkgSlot(cen, off, len, hash);
            int existing = mPkgTable[slot];
            if (existing != -1) {
                return existing;
            }
            if ((mNumPkgs + 1) * 3 > mPkgTable.length * 2) {
                rehashPackages();
                slot = findPkgSlot(cen, off, len, hash);
            }
            int newId = mNumPkgs++;
            if (newId >= mPkgRepLen.length) {
                int newCap = mPkgRepLen.length * 2;
                mPkgRepLen = Arrays.copyOf(mPkgRepLen, newCap);
                mPkgHash = Arrays.copyOf(mPkgHash, newCap);
                mPkgHead = Arrays.copyOf(mPkgHead, newCap);
                mPkgTail = Arrays.copyOf(mPkgTail, newCap);
            }
            mPkgRepLen[newId] = len;
            mPkgHash[newId] = hash;
            mPkgHead[newId] = -1;
            mPkgTail[newId] = -1;
            mPkgTable[slot] = newId;
            return newId;
        }

        private void rehashPackages() {
            int newCap = mPkgTable.length * 2;
            int[] newTable = new int[newCap];
            Arrays.fill(newTable, -1);
            int mask = newCap - 1;
            for (int id = 0; id < mNumPkgs; id++) {
                int slot = mPkgHash[id] & mask;
                while (newTable[slot] != -1) {
                    slot = (slot + 1) & mask;
                }
                newTable[slot] = id;
            }
            mPkgTable = newTable;
        }

        private int findPackage(byte[] bytes) {
            return mPkgTable[
                    findPkgSlot(bytes, 0, bytes.length, hashBytes(bytes, 0, bytes.length))];
        }

        Iterable<JavaFileObject> list(String packageName, Set<Kind> kinds, boolean recurse)
                throws IOException {
            if (recurse) {
                return listRecursive(packageName, kinds);
            }
            byte[] pkgBytes = packageName.replace('.', '/').getBytes(UTF_8);
            int pkgId = findPackage(pkgBytes);
            int e = pkgId >= 0 ? mPkgHead[pkgId] : -1;
            if (e == -1 && mFallbacks.isEmpty()) {
                return List.of();
            }
            List<JavaFileObject> result = new ArrayList<>();
            int fbIdx = 0;
            int fbCount = mFallbacks.size();
            while (e != -1 || fbIdx < fbCount) {
                if (fbIdx < fbCount
                        && (e == -1
                                || mFallbacks.get(fbIdx).cpIndex()
                                        < mJars[mEntryJar[e]].mCpIndex)) {
                    for (JavaFileObject fo :
                            LazyJavacFileManager.super.list(
                                    mFallbacks.get(fbIdx++),
                                    packageName,
                                    kinds,
                                    /* recurse= */ false)) {
                        result.add(fo);
                    }
                    continue;
                }
                IndexedJar jar = mJars[mEntryJar[e]];
                int cenOff = mEntryCen[e];
                Kind kind =
                        kindOfEntry(
                                jar.mCenBytes, cenOff + CENHDR, u2(jar.mCenBytes, cenOff + CENNAM));
                if (kinds.contains(kind)) {
                    result.add(new LazyJarFileObject(jar, cenOff, kind));
                }
                e = mEntryNext[e];
            }
            return result;
        }

        private Iterable<JavaFileObject> listRecursive(String packageName, Set<Kind> kinds)
                throws IOException {
            byte[] prefix = packageName.replace('.', '/').getBytes(UTF_8);
            // Collect (cpIndex, JavaFileObject) pairs and preserve classpath order.
            List<OrderedFileObject> ordered = new ArrayList<>();
            for (int id = 0; id < mNumPkgs; id++) {
                int head = mPkgHead[id];
                byte[] rep = mJars[mEntryJar[head]].mCenBytes;
                int off = mEntryCen[head] + CENHDR;
                int len = mPkgRepLen[id];
                if (prefix.length > 0
                        && (len < prefix.length
                                || (len > prefix.length && rep[off + prefix.length] != '/')
                                || !Arrays.equals(
                                        rep, off, off + prefix.length, prefix, 0, prefix.length))) {
                    continue;
                }
                for (int e = head; e != -1; e = mEntryNext[e]) {
                    IndexedJar jar = mJars[mEntryJar[e]];
                    int cenOff = mEntryCen[e];
                    Kind kind =
                            kindOfEntry(
                                    jar.mCenBytes,
                                    cenOff + CENHDR,
                                    u2(jar.mCenBytes, cenOff + CENNAM));
                    if (kinds.contains(kind)) {
                        ordered.add(
                                new OrderedFileObject(
                                        jar.mCpIndex, new LazyJarFileObject(jar, cenOff, kind)));
                    }
                }
            }
            for (FallbackLocation fb : mFallbacks) {
                for (JavaFileObject fo :
                        LazyJavacFileManager.super.list(
                                fb, packageName, kinds, /* recurse= */ true)) {
                    ordered.add(new OrderedFileObject(fb.cpIndex(), fo));
                }
            }
            ordered.sort(Comparator.comparingInt(a -> a.cpIndex));
            List<JavaFileObject> result = new ArrayList<>(ordered.size());
            for (OrderedFileObject o : ordered) {
                result.add(o.fo);
            }
            return result;
        }

        FileObject getFile(String pkgSlash, String baseName, FallbackLookup fallbackLookup)
                throws IOException {
            byte[] pkgBytes = pkgSlash.getBytes(UTF_8);
            byte[] baseBytes = baseName.getBytes(UTF_8);
            int pkgId = findPackage(pkgBytes);
            int e = pkgId >= 0 ? mPkgHead[pkgId] : -1;
            int fbIdx = 0;
            int fbCount = mFallbacks.size();
            while (e != -1 || fbIdx < fbCount) {
                if (fbIdx < fbCount
                        && (e == -1
                                || mFallbacks.get(fbIdx).cpIndex()
                                        < mJars[mEntryJar[e]].mCpIndex)) {
                    FileObject fo = fallbackLookup.lookup(mFallbacks.get(fbIdx++));
                    if (fo != null) {
                        return fo;
                    }
                    continue;
                }
                IndexedJar jar = mJars[mEntryJar[e]];
                int cenOff = mEntryCen[e];
                int nameOff = cenOff + CENHDR;
                int nameLen = u2(jar.mCenBytes, cenOff + CENNAM);
                int baseStart = pkgBytes.length == 0 ? nameOff : nameOff + pkgBytes.length + 1;
                if (nameOff + nameLen - baseStart == baseBytes.length
                        && Arrays.equals(
                                jar.mCenBytes,
                                baseStart,
                                nameOff + nameLen,
                                baseBytes,
                                0,
                                baseBytes.length)) {
                    Kind kind = kindOfEntry(jar.mCenBytes, nameOff, nameLen);
                    return new LazyJarFileObject(jar, cenOff, kind);
                }
                e = mEntryNext[e];
            }
            return null;
        }

        @Override
        public void close() throws IOException {
            for (IndexedJar jar : mJars) {
                if (jar != null) {
                    jar.close();
                }
            }
            for (FallbackLocation fb : mFallbacks) {
                try {
                    LazyJavacFileManager.super.setLocationFromPaths(fb, List.of());
                } catch (IOException ignored) {
                }
            }
        }
    }

    /**
     * A JAR whose raw central directory has been read into memory; its {@link FileChannel} is
     * opened lazily only if an entry's bytes are actually read.
     */
    private static final class IndexedJar implements Closeable {
        final int mCpIndex;
        final Path mUserPath;
        final Path mCanonicalPath;
        final byte[] mCenBytes;
        private final long mFileSize;
        private FileChannel mChan;
        private String mAbsUri;

        private IndexedJar(
                int cpIndex, Path userPath, Path canonicalPath, byte[] cenBytes, long fileSize) {
            mCpIndex = cpIndex;
            mUserPath = userPath;
            mCanonicalPath = canonicalPath;
            mCenBytes = cenBytes;
            mFileSize = fileSize;
        }

        static IndexedJar tryOpen(int cpIndex, Path userPath, Path canonicalPath, byte[] scratch) {
            try (FileChannel ch = FileChannel.open(userPath, StandardOpenOption.READ)) {
                long size = ch.size();
                if (size < ENDHDR) {
                    return null;
                }
                int tailLen = (int) Math.min(size, scratch.length);
                long tailStart = size - tailLen;
                if (!readFully(ch, tailStart, scratch, 0, tailLen)) {
                    return null;
                }
                int eocdIdx = tailLen - ENDHDR;
                // Require an uncommented, single-disk EOCD directly at EOF; archives with ZIP
                // comments, multi-disk metadata, ZIP64, or prepended stubs fall back to super.
                if (u4(scratch, eocdIdx) != ENDSIG
                        || u2(scratch, eocdIdx + ENDCOM) != 0
                        || u2(scratch, eocdIdx + ENDDSK) != 0
                        || u2(scratch, eocdIdx + ENDSTR) != 0) {
                    return null;
                }
                int totalEntries = u2(scratch, eocdIdx + ENDTOT);
                long cdSize = u4(scratch, eocdIdx + ENDSIZ);
                long cdOffset = u4(scratch, eocdIdx + ENDOFF);
                if (totalEntries == ZIP64_MAGICCOUNT
                        || cdSize == ZIP64_MAGICVAL
                        || cdOffset == ZIP64_MAGICVAL) {
                    return null;
                }
                long eocdFileOff = tailStart + eocdIdx;
                if (cdSize > MAX_ENTRY_SIZE || cdOffset + cdSize != eocdFileOff) {
                    return null;
                }
                int cdSizeInt = (int) cdSize;
                byte[] cenBytes;
                if (cdOffset >= tailStart) {
                    int rel = (int) (cdOffset - tailStart);
                    cenBytes = Arrays.copyOfRange(scratch, rel, rel + cdSizeInt);
                } else {
                    cenBytes = new byte[cdSizeInt];
                    if (!readFully(ch, cdOffset, cenBytes, 0, cdSizeInt)) {
                        return null;
                    }
                }
                return new IndexedJar(cpIndex, userPath, canonicalPath, cenBytes, size);
            } catch (IOException e) {
                return null;
            }
        }

        /**
         * Validates all CEN entries and writes non-directory entry offsets into {@code outCens}
         * (which must be at least {@code mCenBytes.length / CENHDR} long). Returns entry count, or
         * {@code -1} if this JAR needs fallback (multi-release, ZIP64/oversize/encrypted entry,
         * non-normalized path, unsupported compression, or corrupt header).
         */
        int collectEntries(boolean checkMultiRelease, int[] outCens) {
            byte[] cen = mCenBytes;
            int limit = cen.length;
            int count = 0;
            boolean hasManifest = false;
            boolean hasVersions = false;
            for (int pos = 0; pos < limit; ) {
                if (limit - pos < CENHDR || u4(cen, pos) != CENSIG) {
                    return -1;
                }
                int flags = u2(cen, pos + CENFLG);
                int how = u2(cen, pos + CENHOW);
                int nameLen = u2(cen, pos + CENNAM);
                int extLen = u2(cen, pos + CENEXT);
                int comLen = u2(cen, pos + CENCOM);
                int nameOff = pos + CENHDR;
                if (limit - nameOff < nameLen + extLen + comLen) {
                    return -1;
                }
                int next = nameOff + nameLen + extLen + comLen;
                if (nameLen > 0 && cen[nameOff + nameLen - 1] != '/') {
                    if ((flags & 1) != 0
                            || (how != ZipEntry.STORED && how != ZipEntry.DEFLATED)
                            || cen[nameOff] == '/'
                            || containsDoubleSlash(cen, nameOff, nameLen)) {
                        return -1;
                    }
                    if (u4(cen, pos + CENOFF) == ZIP64_MAGICVAL
                            || u4(cen, pos + CENSIZ) > MAX_ENTRY_SIZE
                            || u4(cen, pos + CENLEN) > MAX_ENTRY_SIZE) {
                        return -1;
                    }
                    if (checkMultiRelease && cen[nameOff] == 'M') {
                        if (!hasManifest
                                && nameLen == META_INF_MANIFEST.length
                                && Arrays.equals(
                                        cen,
                                        nameOff,
                                        nameOff + META_INF_MANIFEST.length,
                                        META_INF_MANIFEST,
                                        0,
                                        META_INF_MANIFEST.length)) {
                            hasManifest = true;
                        } else if (!hasVersions
                                && nameLen > META_INF_VERSIONS.length
                                && Arrays.equals(
                                        cen,
                                        nameOff,
                                        nameOff + META_INF_VERSIONS.length,
                                        META_INF_VERSIONS,
                                        0,
                                        META_INF_VERSIONS.length)) {
                            hasVersions = true;
                        }
                        if (hasManifest && hasVersions) {
                            return -1;
                        }
                    }
                    outCens[count++] = pos;
                }
                pos = next;
            }
            return count;
        }

        String absUri() {
            if (mAbsUri == null) {
                mAbsUri = mCanonicalPath.toUri().toString();
            }
            return mAbsUri;
        }

        private synchronized FileChannel channel() throws IOException {
            if (mChan == null) {
                mChan = FileChannel.open(mCanonicalPath, StandardOpenOption.READ);
            }
            return mChan;
        }

        InputStream openEntry(int cenOffset, LazyJavacFileManager fm) throws IOException {
            byte[] cen = mCenBytes;
            long locOff = u4(cen, cenOffset + CENOFF);
            int nameLen = u2(cen, cenOffset + CENNAM);
            int cenExtLen = u2(cen, cenOffset + CENEXT);
            int how = u2(cen, cenOffset + CENHOW);
            int compSize = (int) u4(cen, cenOffset + CENSIZ);
            int uncompSize = (int) u4(cen, cenOffset + CENLEN);

            if (locOff > mFileSize - LOCHDR) {
                throw new ZipException("Truncated local header in " + mUserPath);
            }
            int readLen =
                    (int)
                            Math.min(
                                    (long) LOCHDR
                                            + nameLen
                                            + cenExtLen
                                            + compSize
                                            + LOCAL_HEADER_SLACK,
                                    mFileSize - locOff);
            byte[] buf = new byte[readLen];
            FileChannel ch = channel();
            if (!readFully(ch, locOff, buf, 0, readLen)) {
                throw new ZipException("Unexpected EOF in " + mUserPath);
            }
            if (u4(buf, 0) != LOCSIG) {
                throw new ZipException("Invalid local header signature in " + mUserPath);
            }
            int locNameLen = u2(buf, LOCNAM);
            int locExtLen = u2(buf, LOCEXT);
            int dataOff = LOCHDR + locNameLen + locExtLen;
            if ((long) dataOff + compSize > readLen) {
                buf = new byte[compSize];
                if (!readFully(ch, locOff + dataOff, buf, 0, compSize)) {
                    throw new ZipException("Unexpected EOF reading entry in " + mUserPath);
                }
                dataOff = 0;
            }
            if (how == ZipEntry.STORED) {
                return new ByteArrayInputStream(buf, dataOff, compSize);
            }
            return fm.inflate(buf, dataOff, compSize, uncompSize);
        }

        @Override
        public synchronized void close() throws IOException {
            if (mChan != null) {
                mChan.close();
                mChan = null;
            }
        }
    }

    /** Lightweight {@link JavaFileObject} backed directly by an {@link IndexedJar} CEN entry. */
    private final class LazyJarFileObject implements JavaFileObject {
        final IndexedJar mJar;
        private final int mCenOffset;
        private final Kind mKind;
        private String mEntryName;
        private String mBinaryName;

        LazyJarFileObject(IndexedJar jar, int cenOffset, Kind kind) {
            mJar = jar;
            mCenOffset = cenOffset;
            mKind = kind;
        }

        private String entryName() {
            if (mEntryName == null) {
                int nameOff = mCenOffset + CENHDR;
                int nameLen = u2(mJar.mCenBytes, mCenOffset + CENNAM);
                mEntryName = new String(mJar.mCenBytes, nameOff, nameLen, UTF_8);
            }
            return mEntryName;
        }

        String inferBinaryName() {
            if (mBinaryName == null) {
                int nameOff = mCenOffset + CENHDR;
                int nameLen = u2(mJar.mCenBytes, mCenOffset + CENNAM);
                int end;
                if (mKind != Kind.OTHER) {
                    end = nameOff + nameLen - mKind.extension.length();
                } else {
                    int dot = lastIndexOf(mJar.mCenBytes, nameOff, nameOff + nameLen, (byte) '.');
                    int slash = lastIndexOf(mJar.mCenBytes, nameOff, nameOff + nameLen, (byte) '/');
                    end = dot > slash ? dot : nameOff + nameLen;
                }
                mBinaryName =
                        new String(mJar.mCenBytes, nameOff, end - nameOff, UTF_8).replace('/', '.');
            }
            return mBinaryName;
        }

        @Override
        public Kind getKind() {
            return mKind;
        }

        @Override
        public boolean isNameCompatible(String simpleName, Kind k) {
            Objects.requireNonNull(simpleName);
            Objects.requireNonNull(k);
            if (k == Kind.OTHER && mKind != Kind.OTHER) {
                return false;
            }
            String en = entryName();
            int slash = en.lastIndexOf('/');
            String base = slash < 0 ? en : en.substring(slash + 1);
            return base.equals(simpleName + k.extension);
        }

        @Override
        public NestingKind getNestingKind() {
            return null;
        }

        @Override
        public Modifier getAccessLevel() {
            return null;
        }

        @Override
        public URI toUri() {
            try {
                String encodedPath = new URI(null, null, "/" + entryName(), null).getRawPath();
                return URI.create("jar:" + mJar.absUri() + "!" + encodedPath);
            } catch (URISyntaxException e) {
                throw new CannotCreateUriError(entryName(), e);
            }
        }

        @Override
        public String getName() {
            return mJar.mUserPath + "(/" + entryName() + ")";
        }

        @Override
        public InputStream openInputStream() throws IOException {
            return mJar.openEntry(mCenOffset, LazyJavacFileManager.this);
        }

        @Override
        public OutputStream openOutputStream() {
            throw new UnsupportedOperationException();
        }

        @Override
        public Reader openReader(boolean ignoreEncodingErrors) throws IOException {
            CharsetDecoder decoder = getDecoder(getEncodingName(), ignoreEncodingErrors);
            return new InputStreamReader(openInputStream(), decoder);
        }

        @Override
        public CharSequence getCharContent(boolean ignoreEncodingErrors) throws IOException {
            CharBuffer cb = getCachedContent(this);
            if (cb == null) {
                try (InputStream in = openInputStream()) {
                    ByteBuffer bb = makeByteBuffer(in);
                    JavaFileObject prev = log.useSource(this);
                    try {
                        cb = decode(bb, ignoreEncodingErrors);
                    } finally {
                        log.useSource(prev);
                    }
                    recycleByteBuffer(bb);
                    if (!ignoreEncodingErrors) {
                        cache(this, cb);
                    }
                }
            }
            return cb;
        }

        @Override
        public Writer openWriter() {
            throw new UnsupportedOperationException();
        }

        @Override
        public long getLastModified() {
            int dosTime = u2(mJar.mCenBytes, mCenOffset + CENTIM);
            int dosDate = u2(mJar.mCenBytes, mCenOffset + CENTIM + 2);
            return dosToEpochMillis(dosDate, dosTime);
        }

        @Override
        public boolean delete() {
            return false;
        }

        @Override
        public boolean equals(Object obj) {
            if (this == obj) {
                return true;
            }
            if (!(obj instanceof LazyJarFileObject other)) {
                return false;
            }
            return mCenOffset == other.mCenOffset
                    && mJar.mCanonicalPath.equals(other.mJar.mCanonicalPath);
        }

        @Override
        public int hashCode() {
            return 31 * mJar.mCanonicalPath.hashCode() + mCenOffset;
        }

        @Override
        public String toString() {
            return "LazyJarFileObject[" + mJar.mUserPath + ":/" + entryName() + "]";
        }
    }

    // --- Low-level byte & ZIP helpers ---

    private static boolean readFully(
            FileChannel ch, long fileOffset, byte[] dst, int dstOff, int len) throws IOException {
        ByteBuffer buf = ByteBuffer.wrap(dst, dstOff, len);
        long pos = fileOffset;
        while (buf.hasRemaining()) {
            int n = ch.read(buf, pos);
            if (n < 0) {
                return false;
            }
            pos += n;
        }
        return true;
    }

    private static int u2(byte[] b, int off) {
        return (b[off] & 0xFF) | ((b[off + 1] & 0xFF) << 8);
    }

    private static long u4(byte[] b, int off) {
        return ((long) (b[off] & 0xFF))
                | ((long) (b[off + 1] & 0xFF) << 8)
                | ((long) (b[off + 2] & 0xFF) << 16)
                | ((long) (b[off + 3] & 0xFF) << 24);
    }

    private static boolean containsDoubleSlash(byte[] b, int off, int len) {
        int end = off + len - 1;
        for (int i = off; i < end; i++) {
            if (b[i] == '/' && b[i + 1] == '/') {
                return true;
            }
        }
        return false;
    }

    private static int lastIndexOf(byte[] b, int from, int to, byte target) {
        for (int i = to - 1; i >= from; i--) {
            if (b[i] == target) {
                return i;
            }
        }
        return -1;
    }

    private static int hashBytes(byte[] b, int off, int len) {
        int h = 0;
        for (int i = 0; i < len; i++) {
            h = 31 * h + b[off + i];
        }
        return h;
    }

    private static boolean isValidPackage(byte[] b, int off, int len) {
        int end = off + len;
        boolean atStart = true;
        for (int i = off; i < end; i++) {
            byte c = b[i];
            if (c < 0) {
                // Non-ASCII UTF-8: fall back to SourceVersion.isIdentifier per segment.
                return isValidPackageSlow(new String(b, off, len, UTF_8));
            }
            if (c == '/') {
                if (atStart) {
                    return false;
                }
                atStart = true;
            } else if (atStart) {
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$')) {
                    return false;
                }
                atStart = false;
            } else {
                if (!((c >= 'a' && c <= 'z')
                        || (c >= 'A' && c <= 'Z')
                        || (c >= '0' && c <= '9')
                        || c == '_'
                        || c == '$')) {
                    return false;
                }
            }
        }
        return !atStart;
    }

    private static boolean isValidPackageSlow(String pkg) {
        int start = 0;
        while (true) {
            int slash = pkg.indexOf('/', start);
            String seg = slash < 0 ? pkg.substring(start) : pkg.substring(start, slash);
            if (seg.isEmpty() || !SourceVersion.isIdentifier(seg)) {
                return false;
            }
            if (slash < 0) {
                return true;
            }
            start = slash + 1;
        }
    }

    private static boolean endsWith(byte[] b, int off, int len, byte[] suffix) {
        return len >= suffix.length
                && Arrays.equals(b, off + len - suffix.length, off + len, suffix, 0, suffix.length);
    }

    private static Kind kindOfEntry(byte[] b, int off, int len) {
        if (len == 0) {
            return Kind.OTHER;
        }
        return switch (b[off + len - 1]) {
            case 's' -> endsWith(b, off, len, DOT_CLASS) ? Kind.CLASS : Kind.OTHER;
            case 'a' -> endsWith(b, off, len, DOT_JAVA) ? Kind.SOURCE : Kind.OTHER;
            case 'l' -> endsWith(b, off, len, DOT_HTML) ? Kind.HTML : Kind.OTHER;
            default -> Kind.OTHER;
        };
    }

    private static long dosToEpochMillis(int dosDate, int dosTime) {
        int year = ((dosDate >> 9) & 0x7F) + 1980;
        int month = (dosDate >> 5) & 0x0F;
        int day = dosDate & 0x1F;
        int hour = (dosTime >> 11) & 0x1F;
        int minute = (dosTime >> 5) & 0x3F;
        int second = (dosTime & 0x1F) * 2;
        if (month < 1 || month > 12 || day < 1 || day > 31) {
            return 0L;
        }
        try {
            return LocalDateTime.of(year, month, day, hour, minute, second)
                    .atZone(ZoneId.systemDefault())
                    .toInstant()
                    .toEpochMilli();
        } catch (DateTimeException e) {
            return 0L;
        }
    }
}
