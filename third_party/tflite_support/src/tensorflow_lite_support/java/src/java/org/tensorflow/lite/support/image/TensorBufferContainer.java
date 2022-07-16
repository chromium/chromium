/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

package org.tensorflow.lite.support.image;

import android.graphics.Bitmap;
import android.util.Log;

import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Holds a {@link TensorBuffer} and converts it to other image formats as needed. */
final class TensorBufferContainer implements ImageContainer {
    private final TensorBuffer buffer;
    private final ColorSpaceType colorSpaceType;
    private static final String TAG = TensorBufferContainer.class.getSimpleName();

    /**
     * Creates a {@link TensorBufferContainer} object with the specified {@link
     * TensorImage#ColorSpaceType}.
     *
     * @throws IllegalArgumentException if the shape of the {@link TensorBuffer} does not match the
     *     specified color space type
     */
    static TensorBufferContainer create(TensorBuffer buffer, ColorSpaceType colorSpaceType) {
        return new TensorBufferContainer(buffer, colorSpaceType);
    }

    private TensorBufferContainer(TensorBuffer buffer, ColorSpaceType colorSpaceType) {
        colorSpaceType.assertShape(buffer.getShape());
        this.buffer = buffer;
        this.colorSpaceType = colorSpaceType;
    }

    @Override
    public TensorBufferContainer clone() {
        return create(TensorBuffer.createFrom(buffer, buffer.getDataType()), colorSpaceType);
    }

    @Override
    public Bitmap getBitmap() {
        if (buffer.getDataType() != DataType.UINT8) {
            // Print warning instead of throwing an exception. When using float models, users may
            // want to convert the resulting float image into Bitmap. That's fine to do so, as long
            // as they are aware of the potential accuracy lost when casting to uint8.
            Log.w(TAG,
                    "<Warning> TensorBufferContainer is holding a non-uint8 image. The conversion to Bitmap"
                            + " will cause numeric casting and clamping on the data value.");
        }

        return colorSpaceType.convertTensorBufferToBitmap(buffer);
    }

    @Override
    public TensorBuffer getTensorBuffer(DataType dataType) {
        // If the data type of buffer is desired, return it directly. Not making a defensive copy
        // for performance considerations. During image processing, users may need to set and get
        // the TensorBuffer many times. Otherwise, create another one with the expected data type.
        return buffer.getDataType() == dataType ? buffer
                                                : TensorBuffer.createFrom(buffer, dataType);
    }

    @Override
    public int getWidth() {
        return colorSpaceType.getWidth(buffer.getShape());
    }

    @Override
    public int getHeight() {
        return colorSpaceType.getHeight(buffer.getShape());
    }

    @Override
    public ColorSpaceType getColorSpaceType() {
        return colorSpaceType;
    }
}
