// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;

/**
 * A container for 3D mesh data, including vertex positions, texture coordinates, and indices. This
 * is used to represent custom geometry for rendering in spatial environments.
 */
@NullMarked
public final class XrMeshData {
    private final int mTextureId;
    private final int mIndexType;
    private final @Nullable ByteBuffer mPositions;
    private final @Nullable ByteBuffer mTextureCoords;
    private final @Nullable ByteBuffer mIndices;
    private float mCurrentScale = 1.0f;

    /**
     * Creates an array of {@link XrMeshData} with the specified size. Called by native code to
     * initialize an array before filling it.
     *
     * @param size The size of the array to create.
     * @return A new array of XrMeshData.
     */
    @CalledByNative
    public static XrMeshData[] createArray(int size) {
        return new XrMeshData[size];
    }

    /**
     * Constructs an {@link XrMeshData} instance from native buffers. Splits the interleaved vertex
     * buffer into separate positions and texture coordinates.
     *
     * @param textureId The ID of the texture associated with this mesh.
     * @param indexType The draw mode or index type for rendering.
     * @param interleavedVertices ByteBuffer containing interleaved vertex data (3 floats for
     *     position, 2 floats for texture coordinates).
     * @param indices ByteBuffer containing index data.
     * @throws IllegalArgumentException if interleavedVertices length is not a multiple of 5.
     */
    @CalledByNative
    public XrMeshData(
            int textureId,
            int indexType,
            @Nullable ByteBuffer interleavedVertices,
            @Nullable ByteBuffer indices) {
        mTextureId = textureId;
        mIndexType = indexType;

        if (interleavedVertices == null || interleavedVertices.remaining() == 0) {
            mPositions = null;
            mTextureCoords = null;
            mIndices = null;
            return;
        }

        FloatBuffer interleavedBuffer =
                interleavedVertices.order(ByteOrder.nativeOrder()).asFloatBuffer();
        int totalFloats = interleavedBuffer.remaining();
        if (totalFloats % 5 != 0) {
            throw new IllegalArgumentException(
                    "interleavedVertices length must be a multiple of 5");
        }

        int vCount = totalFloats / 5;
        int iCount = (indices != null) ? indices.remaining() / 4 : 0;

        mPositions = ByteBuffer.allocateDirect(vCount * 3 * 4).order(ByteOrder.nativeOrder());
        mTextureCoords = ByteBuffer.allocateDirect(vCount * 2 * 4).order(ByteOrder.nativeOrder());
        mIndices = ByteBuffer.allocateDirect(iCount * 4).order(ByteOrder.nativeOrder());

        FloatBuffer posBuffer = mPositions.asFloatBuffer();
        FloatBuffer texBuffer = mTextureCoords.asFloatBuffer();

        for (int i = 0; i < vCount; ++i) {
            posBuffer.put(interleavedBuffer.get());
            posBuffer.put(interleavedBuffer.get());
            posBuffer.put(interleavedBuffer.get());
            texBuffer.put(interleavedBuffer.get());
            texBuffer.put(interleavedBuffer.get());
        }

        mPositions.rewind();
        mTextureCoords.rewind();

        if (mIndices != null && indices != null) {
            mIndices.put(indices.order(ByteOrder.nativeOrder()));
            mIndices.rewind();
        }
    }

    /** Returns the texture ID associated with this mesh. */
    public int getTextureId() {
        return mTextureId;
    }

    /** Returns the index type / draw mode of the mesh. */
    public int getIndexType() {
        return mIndexType;
    }

    /** Returns the ByteBuffer containing vertex positions, or null if none. */
    public @Nullable ByteBuffer getPositions() {
        return mPositions;
    }

    /** Returns the ByteBuffer containing texture coordinates, or null if none. */
    public @Nullable ByteBuffer getTextureCoords() {
        return mTextureCoords;
    }

    /** Returns the ByteBuffer containing indices, or null if none. */
    public @Nullable ByteBuffer getIndices() {
        return mIndices;
    }

    /** Returns a FloatBuffer view of the vertex positions, or null if none. */
    public @Nullable FloatBuffer getPositionsAsFloatBuffer() {
        return mPositions != null ? mPositions.asFloatBuffer() : null;
    }

    /** Returns a FloatBuffer view of the texture coordinates, or null if none. */
    public @Nullable FloatBuffer getTextureCoordsAsFloatBuffer() {
        return mTextureCoords != null ? mTextureCoords.asFloatBuffer() : null;
    }

    /** Returns an IntBuffer view of the indices, or null if none. */
    public @Nullable IntBuffer getIndicesAsIntBuffer() {
        return mIndices != null ? mIndices.asIntBuffer() : null;
    }

    /**
     * Scales the vertex positions of this mesh by the specified scale factor. The scaling is
     * applied relative to the original scale (1.0f).
     *
     * @param scale The new scale factor. Must be strictly positive.
     * @throws IllegalArgumentException if scale is non-positive.
     */
    public void applyScale(float scale) {
        if (scale <= 0.0f) {
            throw new IllegalArgumentException("Scale must be strictly positive: " + scale);
        }
        if (mPositions == null) return;
        if (scale == mCurrentScale) return;

        FloatBuffer buffer = mPositions.asFloatBuffer();
        float factor = scale / mCurrentScale;

        while (buffer.hasRemaining()) {
            int pos = buffer.position();
            buffer.put(pos, buffer.get(pos) * factor);
            buffer.position(pos + 1);
        }
        mPositions.rewind();
        mCurrentScale = scale;
    }

    /** Resets the scale factor to 1.0f, restoring the original vertex positions. */
    public void resetScale() {
        applyScale(1.0f);
    }
}
