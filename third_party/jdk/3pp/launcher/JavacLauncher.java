// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import com.sun.tools.javac.file.CacheFSInfo;
import com.sun.tools.javac.file.FSInfo;
import com.sun.tools.javac.file.JavacFileManager;
import com.sun.tools.javac.main.Main;
import com.sun.tools.javac.util.Context;
import com.sun.tools.javac.util.Options;

import java.io.IOException;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Optional; // nocheck
import java.util.Set;

import javax.tools.JavaFileManager;

/**
 * Custom javac launcher used by Chromium's jdk-tools-graalvm and error_prone native-images.
 *
 * <p>When {@code -XDcrFastClassPath} is passed (and not overridden with {@code
 * -XDcrFastClassPath=false}), enables Chromium's classpath optimizations:
 *
 * <ul>
 *   <li>Skips opening every classpath JAR upfront to inspect {@code META-INF/MANIFEST.MF} for
 *       {@code Class-Path} attributes.
 *   <li>Caches parent-directory canonical paths on Linux to avoid repeated {@code realpath}
 *       traversals for shared directory prefixes.
 *   <li>Lazily indexes classpath JARs directly from raw ZIP central directories via {@link
 *       LazyJavacFileManager}.
 *   <li>When {@code -XDcrClassPathUsageOut=<path>} is also passed and compilation succeeds, writes
 *       one line per classpath JAR that was actually used during compilation.
 * </ul>
 *
 * <p>Without {@code -XDcrFastClassPath}, behaves identically to stock OpenJDK {@code javac}.
 */
public class JavacLauncher {
    private static boolean isFastClassPathEnabled(Options options) {
        String val = options.get("crFastClassPath");
        return val != null && !"false".equals(val);
    }

    public static void main(String[] args) {
        Context context =
                new Context() {
                    @Override
                    public <T> void put(Class<T> clazz, Factory<T> fac) {
                        // Main.compile() calls CacheFSInfo.preRegister(context) after
                        // parsing options and before Locations processes -classpath.
                        if (clazz == FSInfo.class
                                && isFastClassPathEnabled(Options.instance(this))) {
                            put(FSInfo.class, new FastCacheFSInfo());
                            return;
                        }
                        super.put(clazz, fac);
                    }
                };
        // Capture LazyJavacFileManager directly because --release wraps it in a
        // DelegatingJavaFileManager inside Context.
        LazyJavacFileManager[] lazyFmRef = new LazyJavacFileManager[1];
        context.put(
                JavaFileManager.class,
                (Context.Factory<JavaFileManager>)
                        c -> {
                            if (isFastClassPathEnabled(Options.instance(c))) {
                                LazyJavacFileManager fm = new LazyJavacFileManager(c);
                                lazyFmRef[0] = fm;
                                return fm;
                            }
                            return new JavacFileManager(c, /* register= */ true, null);
                        });
        // Main.compile() flushes/closes compilation outputs; skip
        // fileManager.close() since the process exits immediately.
        Main.Result result = new Main("javac").compile(args, context);
        LazyJavacFileManager lazyFm = lazyFmRef[0];
        if (result == Main.Result.OK && lazyFm != null) {
            String usageOut = Options.instance(context).get("crClassPathUsageOut");
            if (usageOut != null
                    && !usageOut.isEmpty()
                    && !"crClassPathUsageOut".equals(usageOut)) {
                try {
                    lazyFm.writeUsedClassPathJars(Path.of(usageOut));
                } catch (IOException e) {
                    System.err.println("Failed to write classpath usage to " + usageOut + ": " + e);
                    System.exit(Main.Result.SYSERR.exitCode);
                }
            }
        }
        System.exit(result.exitCode);
    }

    /**
     * {@link CacheFSInfo} subclass that skips manifest {@code Class-Path} expansion and caches
     * parent-directory canonical paths on Linux so {@code Locations.SearchPath.addFile} does not
     * repeat {@code realpath}/{@code readlink} traversal of shared directory prefixes for every
     * classpath JAR.
     */
    private static final class FastCacheFSInfo extends CacheFSInfo {
        private static final boolean IS_LINUX =
                System.getProperty("os.name", "").startsWith("Linux");

        private final Path mCwdRealPath;
        private final Set<Path> mNonSymlinkPaths = Collections.synchronizedSet(new HashSet<>());

        FastCacheFSInfo() {
            mCwdRealPath = IS_LINUX ? super.getCanonicalFile(Path.of("")) : null;
        }

        @Override
        public List<Path> getJarClassPath(Path file) {
            return List.of();
        }

        @Override
        protected Optional<BasicFileAttributes> maybeReadAttributes(Path file) {
            if (!IS_LINUX || file.getFileSystem() != FileSystems.getDefault()) {
                return super.maybeReadAttributes(file);
            }
            try {
                BasicFileAttributes attrs =
                        Files.readAttributes(
                                file, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
                if (!attrs.isSymbolicLink()) {
                    mNonSymlinkPaths.add(file);
                    return Optional.of(attrs);
                }
                return Optional.of(Files.readAttributes(file, BasicFileAttributes.class));
            } catch (IOException e) {
                return Optional.empty();
            }
        }

        @Override
        public Path getCanonicalFile(Path file) {
            if (!IS_LINUX || file.getFileSystem() != FileSystems.getDefault()) {
                return super.getCanonicalFile(file);
            }
            Path cached = canonicalPathCache.get(file);
            if (cached != null) {
                return cached;
            }
            Path resolved = resolveCanonical(file);
            canonicalPathCache.putIfAbsent(file, resolved);
            return resolved;
        }

        private Path resolveCanonical(Path file) {
            Path cur = file.isAbsolute() ? file.getRoot() : mCwdRealPath;
            int count = file.getNameCount();
            for (int i = 0; i < count; i++) {
                Path seg = file.getName(i);
                String s = seg.toString();
                if (s.isEmpty() || ".".equals(s)) {
                    continue;
                }
                if ("..".equals(s)) {
                    // cur is already canonical (contains no symlinks), so moving to its
                    // parent matches POSIX realpath semantics.
                    Path parent = cur.getParent();
                    if (parent != null) {
                        cur = parent;
                    }
                    continue;
                }
                Path next = cur.resolve(seg);
                if (i == count - 1 && mNonSymlinkPaths.contains(file)) {
                    cur = next;
                    continue;
                }
                Path cached = canonicalPathCache.get(next);
                if (cached != null) {
                    cur = cached;
                    continue;
                }
                try {
                    BasicFileAttributes attrs =
                            Files.readAttributes(
                                    next, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
                    Path resolved = attrs.isSymbolicLink() ? next.toRealPath() : next;
                    canonicalPathCache.putIfAbsent(next, resolved);
                    cur = resolved;
                } catch (IOException e) {
                    return super.getCanonicalFile(file);
                }
            }
            return cur;
        }

        @Override
        public void clearCache() {
            super.clearCache();
            mNonSymlinkPaths.clear();
        }
    }
}
