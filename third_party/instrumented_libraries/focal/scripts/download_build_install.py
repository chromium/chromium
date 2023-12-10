#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Downloads, builds (with instrumentation) and installs shared libraries."""

import argparse
import ast
import errno
import fcntl
import multiprocessing
import os
import glob
import re
import shlex
import shutil
import subprocess
import sys

SCRIPT_ABSOLUTE_PATH = os.path.dirname(os.path.abspath(__file__))


def unescape_flags(s):
    """Un-escapes build flags received from GN.

    GN escapes build flags as if they are to be inserted directly into a command
    line, wrapping each flag in double quotes. When flags are passed via
    CFLAGS/LDFLAGS instead, double quotes must be dropped.
    """
    if not s:
        return []
    try:
        return ast.literal_eval(s)
    except (SyntaxError, ValueError):
        return shlex.split(s)


def real_path(path_relative_to_gn):
    """Returns the absolute path to a file.

    GN generates paths relative to the build directory, which is one
    level above the location of this script. This function converts them to
    absolute paths.
    """
    return os.path.realpath(
        os.path.join(SCRIPT_ABSOLUTE_PATH, "..", path_relative_to_gn))


class InstrumentedPackageBuilder(object):
    """Checks out and builds a single instrumented package."""
    def __init__(self, args, clobber):
        self._cc = args.cc
        self._cxx = args.cxx
        self._extra_configure_flags = unescape_flags(
            args.extra_configure_flags)
        self._libdir = args.libdir
        self._package = args.package
        self._patches = [real_path(patch) for patch in args.patch]
        self._pre_build = real_path(args.pre_build) if args.pre_build else None
        self._verbose = args.verbose
        self._clobber = clobber
        self._working_dir = os.path.join(real_path(args.intermediate_dir),
                                         self._package, "")

        product_dir = real_path(args.product_dir)
        self._destdir = os.path.join(product_dir, "instrumented_libraries")
        self._source_archives_dir = os.path.join(product_dir,
                                                 "instrumented_libraries",
                                                 "sources", self._package)

        self._cflags = unescape_flags(args.cflags)
        if args.sanitizer_ignorelist:
            ignorelist_file = real_path(args.sanitizer_ignorelist)
            self._cflags += ["-fsanitize-blacklist=%s" % ignorelist_file]

        self._ldflags = unescape_flags(args.ldflags)

        self.init_build_env(eval(args.env))

        self._git_url = args.git_url
        self._git_revision = args.git_revision

        self._make_targets = unescape_flags(args.make_targets)

        # Initialized later.
        self._source_dir = ""
        self._source_archives = ""

    def init_build_env(self, args_env):
        self._build_env = os.environ.copy()

        self._build_env.update(dict(args_env))

        self._build_env["CC"] = self._cc
        self._build_env["CXX"] = self._cxx

        self._build_env["CFLAGS"] = " ".join(self._cflags)
        self._build_env["CXXFLAGS"] = " ".join(self._cflags)
        self._build_env["LDFLAGS"] = " ".join(self._ldflags)

        # libappindicator1 needs this.
        self._build_env["CSC"] = "/usr/bin/mono-csc"

    def shell_call(self, command, env=None, cwd=None, shell=False):
        """Wrapper around subprocess.Popen().

        Calls command with specific environment and verbosity using
        subprocess.Popen().
        """
        child = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=env,
            shell=shell,
            cwd=cwd,
        )
        stdout = child.communicate()[0].decode("utf-8")
        if self._verbose or child.returncode:
            print(stdout)
        if child.returncode:
            raise Exception("Failed to run: %s" % command)
        return stdout

    def maybe_download_source(self):
        """Checks out the source code (if needed).

        Checks out the source code for the package, if required (i.e. unless running
        in no-clobber mode). Initializes self._source_dir and self._source_archives.
        """
        command = ""
        get_fresh_source = self._clobber or not os.path.exists(
            self._working_dir)
        if get_fresh_source:
            shutil.rmtree(self._working_dir, ignore_errors=True)
            os.makedirs(self._working_dir)

            if self._git_url:
                command = ["git", "clone", self._git_url]
                self.shell_call(command, cwd=self._working_dir)
            else:
                # Download one source package at a time, otherwise, there will
                # be connection errors in gnutls_handshake().
                lock = open("apt-source-lock", "w")
                fcntl.flock(lock, fcntl.LOCK_EX)
                command = ["apt-get", "source", self._package]
                self.shell_call(command, cwd=self._working_dir)
                fcntl.flock(lock, fcntl.LOCK_UN)

        (dirpath, dirnames, filenames) = next(os.walk(self._working_dir))

        if len(dirnames) != 1:
            raise Exception("`%s' must create exactly one subdirectory." %
                            command)
        self._source_component = dirnames[0]
        self._source_dir = os.path.join(dirpath, self._source_component, "")
        if self._git_url:
            self.shell_call(["git", "checkout", self._git_revision],
                            cwd=self._source_dir)
        else:
            if len(filenames) == 0:
                raise Exception("Can't find source files after `%s'." %
                                command)
            self._source_archives = [
                os.path.join(dirpath, filename) for filename in filenames
            ]

        return get_fresh_source

    def patch_source(self):
        for patch in self._patches:
            self.shell_call(["patch", "-p1", "-i", patch],
                            cwd=self._source_dir)
        if self._pre_build:
            self.shell_call([self._pre_build], cwd=self._source_dir)

    def copy_source_archives(self):
        """Copies the downloaded source archives to the output dir.

        For license compliance purposes, every Chromium build that includes
        instrumented libraries must include their full source code.
        """
        shutil.rmtree(self._source_archives_dir, ignore_errors=True)
        os.makedirs(self._source_archives_dir)
        if self._git_url:
            dest = os.path.join(self._source_archives_dir,
                                self._source_component)
            shutil.copytree(self._source_dir, dest)
        else:
            for filename in self._source_archives:
                shutil.copy(filename, self._source_archives_dir)
        for patch in self._patches:
            shutil.copy(patch, self._source_archives_dir)

    def download_build_install(self):
        got_fresh_source = self.maybe_download_source()
        if got_fresh_source:
            self.patch_source()
            self.copy_source_archives()

        if not os.path.exists(self.dest_libdir()):
            os.makedirs(self.dest_libdir())

        try:
            self.build_and_install()
        except Exception as exception:
            print("ERROR: Failed to build package %s. Have you "
                  "run src/third_party/instrumented_libraries/scripts/"
                  "install-build-deps.sh?" % self._package)
            raise

        # Touch a text file to indicate package is installed.
        stamp_file = os.path.join(self._destdir, "%s.txt" % self._package)
        open(stamp_file, "w").close()

        # Remove downloaded package and generated temporary build files. Failed
        # builds intentionally skip this step to help debug build failures.
        if self._clobber:
            self.shell_call(["rm", "-rf", self._working_dir])

    def fix_rpaths(self, directory):
        # TODO(eugenis): reimplement fix_rpaths.sh in Python.
        script = real_path("scripts/fix_rpaths.sh")
        self.shell_call([script, directory])

    def temp_dir(self):
        """Returns the directory which will be passed to `make install'."""
        return os.path.join(self._source_dir, "debian", "instrumented_build")

    def temp_libdir(self):
        """Returns the directory under temp_dir() containing the DSOs."""
        return os.path.join(self.temp_dir(), self._libdir)

    def dest_libdir(self):
        """Returns the final location of the DSOs."""
        return os.path.join(self._destdir, self._libdir)

    def cleanup_after_install(self):
        """Removes unneeded files in self.temp_libdir()."""
        # .la files are not needed, nuke them.
        # In case --no-static is not supported, nuke any static libraries we built.
        self.shell_call(
            "find %s -name *.la -or -name *.a | xargs rm -f" %
            self.temp_libdir(),
            shell=True,
        )
        # .pc files are not needed.
        self.shell_call(["rm", "-rf", "%s/pkgconfig" % self.temp_libdir()])

    def make(self, args, env=None, cwd=None):
        """Invokes `make'.

        Invokes `make' with the specified args, using self._build_env and
        self._source_dir by default.
        """
        if cwd is None:
            cwd = self._source_dir
        if env is None:
            env = self._build_env
        self.shell_call(["make"] + args, env=env, cwd=cwd)

    def make_install(self, args, **kwargs):
        """Invokes `make install'."""
        self.make(["install"] + args, **kwargs)

    def build_and_install(self):
        """Builds and installs the DSOs.

        Builds the package with ./configure + make, installs it to a temporary
        location, then moves the relevant files to their permanent location.
        """
        configure = os.path.join(self._source_dir, "configure")
        configure_exists = os.path.exists(configure)
        if configure_exists:
            configure_cmd = [
                configure,
                "--libdir=/%s/" % self._libdir,
            ] + self._extra_configure_flags
            self.shell_call(configure_cmd,
                            env=self._build_env,
                            cwd=self._source_dir)

        args = {
            # Some makefiles use BUILDROOT or INSTALL_ROOT instead of DESTDIR.
            "DESTDIR": self.temp_dir(),
            "BUILDROOT": self.temp_dir(),
            "INSTALL_ROOT": self.temp_dir(),
        }
        if not configure_exists:
            # Specify LIBDIR in case ./configure isn't used for this package.
            args['LIBDIR'] = '/%s/' % self._libdir
        make_args = ["%s=%s" % item for item in args.items()]
        self.make(make_args + self._make_targets)

        self.make_install(make_args)

        self.post_install()

    def post_install(self):
        self.cleanup_after_install()

        self.fix_rpaths(self.temp_libdir())

        # Now move the contents of the temporary destdir to their final place.
        # We only care for the contents of LIBDIR.
        self.shell_call("cp %s/* %s/ -rdf" %
                        (self.temp_libdir(), self.dest_libdir()),
                        shell=True)


class DebianBuilder(InstrumentedPackageBuilder):
    """Builds a package using Debian's build system.

    TODO(spang): Probably the rest of the packages should also use this method..
    """
    def init_build_env(self, args_env):
        self._build_env = os.environ.copy()

        self._build_env.update(dict(args_env))

        self._build_env["CC"] = self._cc
        self._build_env["CXX"] = self._cxx

        self._build_env["DEB_CFLAGS_APPEND"] = " ".join(self._cflags)
        self._build_env["DEB_CXXFLAGS_APPEND"] = " ".join(self._cflags)
        self._build_env["DEB_LDFLAGS_APPEND"] = " ".join(self._ldflags)
        self._build_env["DEB_BUILD_OPTIONS"] = (
            "nocheck notest nodoc nostrip parallel=%d" % os.cpu_count())

    def build_and_install(self):
        self.build_debian_packages()
        self.install_packaged_libs()

    def build_debian_packages(self):
        configure_cmd = ["dpkg-buildpackage", "-B", "-uc"]
        self.shell_call(configure_cmd,
                        env=self._build_env,
                        cwd=self._source_dir)

    def install_packaged_libs(self):
        for deb_file in self.get_deb_files():
            self.shell_call(["dpkg-deb", "-x", deb_file, self.temp_dir()])

        dpkg_arch_cmd = ["dpkg-architecture", "-qDEB_HOST_MULTIARCH"]
        dpkg_arch = self.shell_call(dpkg_arch_cmd).strip()
        lib_dirs = [
            "usr/lib/%s" % dpkg_arch,
            "lib/%s" % dpkg_arch,
        ]
        lib_paths = [
            path for lib_dir in lib_dirs for path in glob.glob(
                os.path.join(self.temp_dir(), lib_dir, "*.so.*"))
        ]
        for lib_path in lib_paths:
            dest_path = os.path.join(self.dest_libdir(),
                                     os.path.basename(lib_path))
            try:
                os.unlink(dest_path)
            except OSError as exception:
                if exception.errno != errno.ENOENT:
                    raise
            if os.path.islink(lib_path):
                if self._verbose:
                    print("linking %s" % os.path.basename(lib_path))
                os.symlink(os.readlink(lib_path), dest_path)
            elif os.path.isfile(lib_path):
                if self._verbose:
                    print("copying %s" % os.path.basename(lib_path))
                shutil.copy(lib_path, dest_path)

    def get_deb_files(self):
        deb_files = []
        files_file = os.path.join(self._source_dir, "debian/files")

        for line in open(files_file, "r").read().splitlines():
            filename, category, section = line.split(" ")
            if not filename.endswith(".deb"):
                continue
            pathname = os.path.join(self._source_dir, "..", filename)
            deb_files.append(pathname)

        return deb_files


class LibcapBuilder(InstrumentedPackageBuilder):
    def build_and_install(self):
        # libcap2 doesn't have a configure script
        build_args = ["CC", "CXX", "CFLAGS", "CXXFLAGS", "LDFLAGS"]
        make_args = [
            "%s=%s" % (name, self._build_env[name]) for name in build_args
        ]
        self.make(make_args)

        install_args = [
            "DESTDIR=%s" % self.temp_dir(),
            "lib=%s" % self._libdir,
            # Skip a step that requires sudo.
            "RAISE_SETFCAP=no",
        ]
        self.make_install(install_args)

        self.cleanup_after_install()

        self.fix_rpaths(self.temp_libdir())

        # Now move the contents of the temporary destdir to their final place.
        # We only care for the contents of LIBDIR.
        self.shell_call("cp %s/* %s/ -rdf" %
                        (self.temp_libdir(), self.dest_libdir()),
                        shell=True)


class LibcurlBuilder(DebianBuilder):
    def build_and_install(self):
        super().build_and_install()
        # The libcurl packages don't specify a default libcurl.so, but this is
        # required since libcurl.so is dlopen()ed by crashpad.  Normally,
        # libcurl.so is installed by one of libcurl-{gnutls,nss,openssl}-dev.
        # Doing a standalone instrumented build of a dev package is tricky,
        # so we manually symlink libcurl.so instead.
        libcurl_so = os.path.join(self.dest_libdir(), "libcurl.so")
        if not os.path.exists(libcurl_so):
            os.symlink("libcurl.so.4", libcurl_so)


class Libpci3Builder(InstrumentedPackageBuilder):
    def package_version(self):
        """Guesses libpci3 version from source directory name."""
        dir_name = os.path.split(os.path.normpath(self._source_dir))[-1]
        match = re.match("pciutils-(\d+\.\d+\.\d+)", dir_name)
        if match is None:
            raise Exception(
                "Unable to guess libpci3 version from directory name: %s" %
                dir_name)
        return match.group(1)

    def temp_libdir(self):
        # DSOs have to be picked up from <source_dir>/lib, since `make install'
        # doesn't actualy install them anywhere.
        return os.path.join(self._source_dir, "lib")

    def build_and_install(self):
        # pciutils doesn't have a configure script
        # This build process follows debian/rules.
        self.shell_call(["mkdir", "-p", "%s-udeb/usr/bin" % self.temp_dir()])

        build_args = ["CC", "CXX", "CFLAGS", "CXXFLAGS", "LDFLAGS"]
        make_args = [
            "%s=%s" % (name, self._build_env[name]) for name in build_args
        ]
        make_args += [
            "LIBDIR=/%s/" % self._libdir,
            "PREFIX=/usr",
            "SBINDIR=/usr/bin",
            "IDSDIR=/usr/share/misc",
            "SHARED=yes",
            # pciutils fails to build due to unresolved libkmod symbols. The binary
            # package has no dependencies on libkmod, so it looks like it was
            # actually built without libkmod support.
            "LIBKMOD=no",
        ]
        self.make(make_args)

        # `make install' is not needed.
        self.fix_rpaths(self.temp_libdir())

        # Now install the DSOs to their final place.
        self.shell_call(
            "install -m 644 %s/libpci.so* %s" %
            (self.temp_libdir(), self.dest_libdir()),
            shell=True,
        )
        self.shell_call(
            "ln -sf libpci.so.%s %s/libpci.so.3" %
            (self.package_version(), self.dest_libdir()),
            shell=True,
        )


class MesonBuilder(InstrumentedPackageBuilder):
    def build_and_install(self):
        meson_cmd = [
            "meson",
            "build",
            ".",
            "--prefix",
            "/",
            "--libdir",
            self._libdir,
            "--sbindir",
            "bin",
            "-Db_lundef=false",
        ] + self._extra_configure_flags

        self.shell_call(meson_cmd, env=self._build_env, cwd=self._source_dir)
        self.shell_call(
            ["ninja", "-C", "build", "install"],
            {
                **self._build_env, "DESTDIR": self.temp_dir()
            },
            cwd=self._source_dir,
        )
        self.post_install()


class CmakeBuilder(InstrumentedPackageBuilder):
    def build_and_install(self):
        cmake_cmd = [
            "cmake",
            ".",
            "-DCMAKE_INSTALL_PREFIX=/usr",
            "-DCMAKE_INSTALL_LIBDIR=/%s/" % self._libdir,
        ] + self._extra_configure_flags
        self.shell_call(cmake_cmd, env=self._build_env, cwd=self._source_dir)

        args = ["DESTDIR", "BUILDROOT", "INSTALL_ROOT"]
        make_args = ["%s=%s" % (name, self.temp_dir()) for name in args]
        self.make(make_args)
        self.make_install(make_args)

        self.post_install()


class NSSBuilder(InstrumentedPackageBuilder):
    def build_and_install(self):
        try:
            with multiprocessing.Semaphore():
                pass
        except (OSError, PermissionError):
            raise Exception('/dev/shm must be mounted')

        # Hardcoded paths.
        temp_dir = os.path.join(self._source_dir, "nss")
        temp_libdir = os.path.join(self._source_dir, "dist", "Release", "lib")

        self.shell_call(
            [
                os.path.join(temp_dir, "build.sh"),
                "--gyp",
                "--opt",
                "--msan",
                "--no-zdefs",
                "--system-nspr",
                "-Dsign_libs=0"
                "-Ddisable_tests=1",
            ],
            cwd=temp_dir,
            env=self._build_env,
        )

        self.fix_rpaths(temp_libdir)

        # 'make install' is not supported. Copy the DSOs manually.
        for (dirpath, dirnames, filenames) in os.walk(temp_libdir):
            for filename in filenames:
                if filename.endswith(".so"):
                    full_path = os.path.join(dirpath, filename)
                    if self._verbose:
                        print("download_build_install.py: installing " +
                              full_path)
                    shutil.copy(full_path, self.dest_libdir())


class StubBuilder(InstrumentedPackageBuilder):
    def download_build_install(self):
        self._touch(os.path.join(self._destdir, "%s.txt" % self._package))
        self.shell_call(["mkdir", "-p", self.dest_libdir()])
        self._touch(os.path.join(self.dest_libdir(),
                                 "%s.so.0" % self._package))

    def _touch(self, path):
        with open(path, "w"):
            pass


def main():
    parser = argparse.ArgumentParser(
        description="Download, build and install an instrumented package.")

    parser.add_argument("-p", "--package", required=True)
    parser.add_argument(
        "-i",
        "--product-dir",
        default=".",
        help="Relative path to the directory with chrome binaries",
    )
    parser.add_argument(
        "-m",
        "--intermediate-dir",
        default=".",
        help="Relative path to the directory for temporary build files",
    )
    parser.add_argument("--extra-configure-flags", default="")
    parser.add_argument("--cflags", default="")
    parser.add_argument("--ldflags", default="")
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("--cc")
    parser.add_argument("--cxx")
    parser.add_argument("--patch", nargs="*", action="extend", default=[])
    # This should be a shell script to run before building specific libraries.
    # This will be run after applying the patches above.
    parser.add_argument("--pre-build", default="")
    parser.add_argument("--build-method", default="destdir")
    parser.add_argument("--sanitizer-ignorelist", default="")
    # The LIBDIR argument to configure/make.
    parser.add_argument("--libdir", default="lib")
    parser.add_argument("--env", default="")
    parser.add_argument("--git-url", default="")
    parser.add_argument("--git-revision", default="")
    parser.add_argument("--make-targets", default="")

    # Ignore all empty arguments because in several cases gn passes them to the
    # script, but ArgumentParser treats them as positional arguments instead of
    # ignoring (and doesn't have such options).
    args = parser.parse_args([arg for arg in sys.argv[1:] if len(arg) != 0])

    # Clobber by default, unless the developer wants to hack on the package's
    # source code.
    clobber = os.environ.get("INSTRUMENTED_LIBRARIES_NO_CLOBBER", "") != "1"

    if args.build_method == "destdir":
        builder = InstrumentedPackageBuilder(args, clobber)
    elif args.build_method == "custom_nss":
        builder = NSSBuilder(args, clobber)
    elif args.build_method == "custom_libcap":
        builder = LibcapBuilder(args, clobber)
    elif args.build_method == "custom_libcurl":
        builder = LibcurlBuilder(args, clobber)
    elif args.build_method == "custom_libpci3":
        builder = Libpci3Builder(args, clobber)
    elif args.build_method == "debian":
        builder = DebianBuilder(args, clobber)
    elif args.build_method == "meson":
        builder = MesonBuilder(args, clobber)
    elif args.build_method == "cmake":
        builder = CmakeBuilder(args, clobber)
    elif args.build_method == "stub":
        builder = StubBuilder(args, clobber)
    else:
        raise Exception("Unrecognized build method: %s" % args.build_method)

    builder.download_build_install()


if __name__ == "__main__":
    main()
