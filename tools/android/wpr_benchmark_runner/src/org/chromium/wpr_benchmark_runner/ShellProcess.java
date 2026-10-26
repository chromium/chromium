// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import android.app.UiAutomation;
import android.os.ParcelFileDescriptor;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructPollfd;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.EOFException;
import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

@NullMarked
final class ShellProcess implements AutoCloseable {
    private final ParcelFileDescriptor[] mPipes;
    private final byte[] mBuffer = new byte[8192];
    private int mOffset;
    private int mSize;
    private boolean mErrorsOpen = true;

    ShellProcess(UiAutomation automation, String command) throws IOException {
        mPipes = automation.executeShellCommandRwe(command);
        try {
            for (ParcelFileDescriptor pipe : mPipes) {
                FileDescriptor fd = pipe.getFileDescriptor();
                Os.fcntlInt(
                        fd,
                        OsConstants.F_SETFL,
                        Os.fcntlInt(fd, OsConstants.F_GETFL, 0) | OsConstants.O_NONBLOCK);
            }
        } catch (ErrnoException error) {
            try {
                close();
            } catch (IOException cleanup) {
                error.addSuppressed(cleanup);
            }
            throw new IOException("Could not configure shell pipes", error);
        }
    }

    int readByte(Deadline deadline) throws IOException {
        deadline.remainingMillis();
        if (mOffset == mSize) {
            int size = read(mBuffer, 0, mBuffer.length, deadline);
            if (size == -1) {
                throw new EOFException("Shell output closed");
            }
            mSize = size;
            mOffset = 0;
        }
        return mBuffer[mOffset++] & 255;
    }

    int read(byte[] bytes, int offset, int length, Deadline deadline) throws IOException {
        deadline.remainingMillis();
        if (mOffset < mSize) {
            int size = Math.min(length, mSize - mOffset);
            System.arraycopy(mBuffer, mOffset, bytes, offset, size);
            mOffset += size;
            return size;
        }
        FileDescriptor fd = mPipes[0].getFileDescriptor();
        while (true) {
            await(fd, OsConstants.POLLIN, deadline);
            try {
                int size = Os.read(fd, bytes, offset, length);
                return size == 0 ? -1 : size;
            } catch (ErrnoException error) {
                if (error.errno != OsConstants.EINTR && error.errno != OsConstants.EAGAIN) {
                    throw new IOException("Shell read failed", error);
                }
            }
        }
    }

    void write(byte[] bytes, Deadline deadline) throws IOException {
        FileDescriptor fd = mPipes[1].getFileDescriptor();
        for (int offset = 0; offset < bytes.length; ) {
            await(fd, OsConstants.POLLOUT, deadline);
            try {
                offset += Os.write(fd, bytes, offset, bytes.length - offset);
            } catch (ErrnoException error) {
                if (error.errno != OsConstants.EINTR && error.errno != OsConstants.EAGAIN) {
                    throw new IOException("Shell write failed", error);
                }
            }
        }
    }

    void finishInput() throws IOException {
        mPipes[1].close();
    }

    private void await(FileDescriptor fd, int events, Deadline deadline) throws IOException {
        StructPollfd operation = new StructPollfd();
        operation.fd = fd;
        operation.events = (short) events;
        StructPollfd errors = new StructPollfd();
        errors.fd = mPipes[2].getFileDescriptor();
        errors.events = (short) OsConstants.POLLIN;
        while (true) {
            try {
                StructPollfd[] descriptors =
                        mErrorsOpen
                                ? new StructPollfd[] {operation, errors}
                                : new StructPollfd[] {operation};
                Os.poll(descriptors, (int) Math.min(Integer.MAX_VALUE, deadline.remainingMillis()));
                if (mErrorsOpen && errors.revents != 0) {
                    readError();
                }
                if (operation.revents != 0) {
                    return;
                }
            } catch (ErrnoException error) {
                if (error.errno != OsConstants.EINTR) {
                    throw new IOException("Shell poll failed", error);
                }
            }
        }
    }

    private void readError() throws ErrnoException, IOException {
        byte[] buffer = new byte[4096];
        try {
            int size = Os.read(mPipes[2].getFileDescriptor(), buffer, 0, buffer.length);
            if (size == 0) {
                mErrorsOpen = false;
                return;
            }
            throw new IOException(
                    "Shell command failed: " + new String(buffer, 0, size, StandardCharsets.UTF_8));
        } catch (ErrnoException error) {
            if (error.errno != OsConstants.EINTR && error.errno != OsConstants.EAGAIN) {
                throw error;
            }
        }
    }

    @Override
    public void close() throws IOException {
        @Nullable IOException failure = null;
        for (int index : new int[] {1, 0, 2}) {
            try {
                mPipes[index].close();
            } catch (IOException error) {
                if (failure == null) {
                    failure = error;
                } else {
                    failure.addSuppressed(error);
                }
            }
        }
        if (failure != null) {
            throw failure;
        }
    }
}
