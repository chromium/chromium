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

import static org.tensorflow.lite.support.common.SupportPreconditions.checkArgument;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;

import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

import java.util.Arrays;

/** Represents the type of color space of an image. */
public enum ColorSpaceType {
    /** Each pixel has red, green, and blue color components. */
    RGB {
        // The channel axis should always be 3 for RGB images.
        private static final int CHANNEL_VALUE = 3;

        @Override
        Bitmap convertTensorBufferToBitmap(TensorBuffer buffer) {
            return ImageConversions.convertRgbTensorBufferToBitmap(buffer);
        }

        @Override
        int getChannelValue() {
            return CHANNEL_VALUE;
        }

        @Override
        int[] getNormalizedShape(int[] shape) {
            switch (shape.length) {
                    // The shape is in (h, w, c) format.
                case 3:
                    return insertValue(shape, BATCH_DIM, BATCH_VALUE);
                case 4:
                    return shape;
                default:
                    throw new IllegalArgumentException(getShapeInfoMessage()
                            + "The provided image shape is " + Arrays.toString(shape));
            }
        }

        @Override
        String getShapeInfoMessage() {
            return "The shape of a RGB image should be (h, w, c) or (1, h, w, c), and channels"
                    + " representing R, G, B in order. ";
        }

        @Override
        Config toBitmapConfig() {
            return Config.ARGB_8888;
        }
    },

    /** Each pixel is a single element representing only the amount of light. */
    GRAYSCALE {
        // The channel axis should always be 1 for grayscale images.
        private static final int CHANNEL_VALUE = 1;

        @Override
        Bitmap convertTensorBufferToBitmap(TensorBuffer buffer) {
            return ImageConversions.convertGrayscaleTensorBufferToBitmap(buffer);
        }

        @Override
        int getChannelValue() {
            return CHANNEL_VALUE;
        }

        @Override
        int[] getNormalizedShape(int[] shape) {
            switch (shape.length) {
                    // The shape is in (h, w) format.
                case 2:
                    int[] shapeWithBatch = insertValue(shape, BATCH_DIM, BATCH_VALUE);
                    return insertValue(shapeWithBatch, CHANNEL_DIM, CHANNEL_VALUE);
                case 4:
                    return shape;
                default:
                    // (1, h, w) and (h, w, 1) are potential grayscale image shapes. However, since
                    // they both have three dimensions, it will require extra info to differentiate
                    // between them. Since we haven't encountered real use cases of these two
                    // shapes, they are not supported at this moment to avoid confusion. We may want
                    // to revisit it in the future.
                    throw new IllegalArgumentException(getShapeInfoMessage()
                            + "The provided image shape is " + Arrays.toString(shape));
            }
        }

        @Override
        String getShapeInfoMessage() {
            return "The shape of a grayscale image should be (h, w) or (1, h, w, 1). ";
        }

        @Override
        Config toBitmapConfig() {
            return Config.ALPHA_8;
        }
    };

    private static final int BATCH_DIM = 0; // The first element of the normalizaed shape.
    private static final int BATCH_VALUE = 1; // The batch axis should always be one.
    private static final int HEIGHT_DIM = 1; // The second element of the normalizaed shape.
    private static final int WIDTH_DIM = 2; // The third element of the normalizaed shape.
    private static final int CHANNEL_DIM = 3; // The fourth element of the normalizaed shape.

    /**
     * Converts a bitmap configuration into the corresponding color space type.
     *
     * @throws IllegalArgumentException if the config is unsupported
     */
    static ColorSpaceType fromBitmapConfig(Config config) {
        switch (config) {
            case ARGB_8888:
                return ColorSpaceType.RGB;
            case ALPHA_8:
                return ColorSpaceType.GRAYSCALE;
            default:
                throw new IllegalArgumentException(
                        "Bitmap configuration: " + config + ", is not supported yet.");
        }
    }

    /**
     * Verifies if the given shape matches the color space type.
     *
     * @throws IllegalArgumentException if {@code shape} does not match the color space type
     */
    void assertShape(int[] shape) {
        int[] normalizedShape = getNormalizedShape(shape);
        checkArgument(isValidNormalizedShape(normalizedShape),
                getShapeInfoMessage() + "The provided image shape is " + Arrays.toString(shape));
    }

    /**
     * Converts a {@link TensorBuffer} that represents an image to a Bitmap with the color space
     * type.
     *
     * @throws IllegalArgumentException if the shape of buffer does not match the color space type
     */
    abstract Bitmap convertTensorBufferToBitmap(TensorBuffer buffer);

    /**
     * Returns the width of the given shape corresponding to the color space type.
     *
     * @throws IllegalArgumentException if {@code shape} does not match the color space type
     */
    int getWidth(int[] shape) {
        assertShape(shape);
        return getNormalizedShape(shape)[WIDTH_DIM];
    }

    /**
     * Returns the height of the given shape corresponding to the color space type.
     *
     * @throws IllegalArgumentException if {@code shape} does not match the color space type
     */
    int getHeight(int[] shape) {
        assertShape(shape);
        return getNormalizedShape(shape)[HEIGHT_DIM];
    }

    abstract int getChannelValue();

    /**
     * Gets the normalized shape in the form of (1, h, w, c). Sometimes, a given shape may not have
     * batch or channel axis.
     */
    abstract int[] getNormalizedShape(int[] shape);

    abstract String getShapeInfoMessage();

    /** Converts the color space type to the corresponding bitmap config. */
    abstract Config toBitmapConfig();

    /** Inserts a value at the specified position and return the new array. */
    private static int[] insertValue(int[] array, int pos, int value) {
        int[] newArray = new int[array.length + 1];
        for (int i = 0; i < pos; i++) {
            newArray[i] = array[i];
        }
        newArray[pos] = value;
        for (int i = pos + 1; i < newArray.length; i++) {
            newArray[i] = array[i - 1];
        }
        return newArray;
    }

    protected boolean isValidNormalizedShape(int[] shape) {
        if (shape[BATCH_DIM] == BATCH_VALUE && shape[HEIGHT_DIM] > 0 && shape[WIDTH_DIM] > 0
                && shape[CHANNEL_DIM] == getChannelValue()) {
            return true;
        }
        return false;
    }
}
