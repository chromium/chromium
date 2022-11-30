/*
 * Copyright (C) 2015 The Gifplayer Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package jp.tomorrowkey.android.gifplayer;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.util.Log;

/**
 * A base GIF Drawable with support for animations.
 *
 * Inspired by http://code.google.com/p/android-gifview/
 */
public class BaseGifDrawable extends Drawable implements Runnable, Animatable,
    android.os.Handler.Callback {

    private static final String TAG = "GifDrawable";

    // Max decoder pixel stack size
    private static final int MAX_STACK_SIZE = 4096;
    private static final int MAX_BITS = 4097;

    // Frame disposal methods
    private static final int DISPOSAL_METHOD_UNKNOWN = 0;
    private static final int DISPOSAL_METHOD_LEAVE = 1;
    private static final int DISPOSAL_METHOD_BACKGROUND = 2;
    private static final int DISPOSAL_METHOD_RESTORE = 3;

    // Message types
    private static final int READ_FRAME_REQ = 10;
    private static final int READ_FRAME_RESP = 11;
    private static final int RESET_DECODER = 12;

    // Specifies the minimum amount of time before a subsequent frame will be rendered.
    private static final int MIN_FRAME_SCHEDULE_DELAY_MS = 5;

    private static final byte[] NETSCAPE2_0 = "NETSCAPE2.0".getBytes();

    private static Paint sPaint;
    private static Paint sScalePaint;

    protected final BaseGifImage mGifImage;
    private final byte[] mData;

    private int mPosition;
    protected int mIntrinsicWidth;
    protected int mIntrinsicHeight;

    private int mWidth;
    private int mHeight;

    protected Bitmap mBitmap;
    protected int[] mColors;
    private boolean mScale;
    private float mScaleFactor;

    // The following are marked volatile because they are read/written in the background decoder
    // thread and read from the UI thread.  No further synchronization is needed because their
    // values will only ever change from at most once, and it is safe to lazily detect the change
    // in the UI thread.
    private volatile boolean mError;
    private volatile boolean mDone;
    private volatile boolean mAnimateOnLoad = true;

    private int mBackgroundColor;
    private boolean mLocalColorTableUsed;
    private int mLocalColorTableSize;
    private int[] mLocalColorTable;
    private int[] mActiveColorTable;
    private boolean mInterlace;

    // Each frame specifies a sub-region of the image that should be updated.  The values are
    // clamped to the GIF dimensions if they exceed the intrinsic dimensions.
    private int mFrameX, mFrameY, mFrameWidth, mFrameHeight;

    // This specifies the width of the actual data within a GIF frame.  It will be equal to
    // mFrameWidth unless the frame sub-region was clamped to prevent exceeding the intrinsic
    // dimensions.
    private int mFrameStep;

    private byte[] mBlock = new byte[256];
    private int mDisposalMethod = DISPOSAL_METHOD_BACKGROUND;
    private boolean mTransparency;
    private int mTransparentColorIndex;

    // LZW decoder working arrays
    private short[] mPrefix = new short[MAX_STACK_SIZE];
    private byte[] mSuffix = new byte[MAX_STACK_SIZE];
    private byte[] mPixelStack = new byte[MAX_STACK_SIZE + 1];
    private byte[] mPixels;

    private boolean mBackupSaved;
    private int[] mBackup;

    private int mFrameCount;

    private long mLastFrameTime;

    private boolean mRunning;
    protected int mFrameDelay;
    private int mNextFrameDelay;
    protected boolean mScheduled;
    private boolean mAnimationEnabled = true;
    private final Handler mHandler = new Handler(Looper.getMainLooper(), this);
    private static DecoderThread sDecoderThread;
    private static Handler sDecoderHandler;

    private boolean mRecycled;
    protected boolean mFirstFrameReady;
    private boolean mEndOfFile;
    private int mLoopCount = 0; // 0 to repeat endlessly.
    private int mLoopIndex = 0;

    private final Bitmap.Config mBitmapConfig;
    private boolean mFirstFrame = true;

    public BaseGifDrawable(BaseGifImage gifImage, Bitmap.Config bitmapConfig) {
        this.mBitmapConfig = bitmapConfig;

        // Create the background decoder thread, if necessary.
        if (sDecoderThread == null) {
            sDecoderThread = new DecoderThread();
            sDecoderThread.start();
            sDecoderHandler = new Handler(sDecoderThread.getLooper(), sDecoderThread);
        }

        if (sPaint == null) {
            sPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
            sScalePaint = new Paint(Paint.FILTER_BITMAP_FLAG);
            sScalePaint.setFilterBitmap(true);
        }

        mGifImage = gifImage;
        mData = gifImage.getData();
        mPosition = mGifImage.mHeaderSize;
        mFrameWidth = mFrameStep = mIntrinsicWidth = gifImage.getWidth();
        mFrameHeight = mIntrinsicHeight = gifImage.getHeight();
        mBackgroundColor = mGifImage.mBackgroundColor;
        mError = mGifImage.mError;

        if (!mError) {
            try {
                mBitmap = Bitmap.createBitmap(mIntrinsicWidth, mIntrinsicHeight, mBitmapConfig);
                if (mBitmap == null) {
                    throw new OutOfMemoryError("Cannot allocate bitmap");
                }

                int pixelCount = mIntrinsicWidth * mIntrinsicHeight;
                mColors = new int[pixelCount];
                mPixels = new byte[pixelCount];

                mWidth = mIntrinsicHeight;
                mHeight = mIntrinsicHeight;

                // Read the first frame
                sDecoderHandler.sendMessage(sDecoderHandler.obtainMessage(READ_FRAME_REQ, this));
            } catch (OutOfMemoryError e) {
                mError = true;
            }
        }
    }

    /**
     * Sets the loop count for multi-frame animation.
     */
    public void setLoopCount(int loopCount) {
        mLoopCount = loopCount;
    }

    /**
     * Returns the loop count for multi-frame animation.
     */
    public int getLoopCount() {
        return mLoopCount;
    }

    /**
     * Sets whether to start animation on load or not.
     */
    public void setAnimateOnLoad(boolean animateOnLoad) {
        mAnimateOnLoad = animateOnLoad;
    }

    /**
     * Returns {@code true} if the GIF is valid and {@code false} otherwise.
     */
    public boolean isValid() {
        return !mError && mFirstFrameReady;
    }

    public void onRecycle() {
        if (mBitmap != null) {
            mBitmap.recycle();
        }
        mBitmap = null;
        mRecycled = true;
    }

    /**
     * Enables or disables the GIF from animating. GIF animations are enabled by default.
     */
    public void setAnimationEnabled(boolean animationEnabled) {
        if (mAnimationEnabled == animationEnabled) {
            return;
        }

        mAnimationEnabled = animationEnabled;
        if (mAnimationEnabled) {
            start();
        } else {
            stop();
        }
    }

    /**
     * Posts a RESET_DECODER request to the decoder thread so that the decoder starts decoding back
     * from the start of the GIF.
     */
    public void requestReset() {
        sDecoderHandler.sendMessage(sDecoderHandler.obtainMessage(RESET_DECODER, this));
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);
        mWidth = bounds.width();
        mHeight =  bounds.height();
        mScale = mWidth != mIntrinsicWidth && mHeight != mIntrinsicHeight;
        if (mScale) {
            mScaleFactor = Math.max((float) mWidth / mIntrinsicWidth,
                    (float) mHeight / mIntrinsicHeight);
        }

        if (!mError && !mRecycled) {
            // Request that the decoder reset itself
            sDecoderHandler.sendMessage(sDecoderHandler.obtainMessage(RESET_DECODER, this));
        }
    }

    @Override
    public boolean setVisible(boolean visible, boolean restart) {
        boolean changed = super.setVisible(visible, restart);
        if (visible) {
            if (changed || restart) {
                start();
            }
        } else {
            stop();
        }
        return changed;
    }

    @Override
    public void draw(Canvas canvas) {
        if (mError || mWidth == 0 || mHeight == 0 || mRecycled || !mFirstFrameReady) {
            return;
        }

        if (mScale) {
            canvas.save();
            canvas.scale(mScaleFactor, mScaleFactor, 0, 0);
            canvas.drawBitmap(mBitmap, 0, 0, sScalePaint);
            canvas.restore();
        } else {
            canvas.drawBitmap(mBitmap, 0, 0, sPaint);
        }

        if (mRunning) {
            if (!mScheduled) {
                // Schedule the next frame at mFrameDelay milliseconds from the previous frame or
                // the minimum sceduling delay from now, whichever is later.
                mLastFrameTime = Math.max(
                    mLastFrameTime + mFrameDelay,
                    SystemClock.uptimeMillis() + MIN_FRAME_SCHEDULE_DELAY_MS);
                scheduleSelf(this, mLastFrameTime);
            }
        } else if (!mDone) {
            start();
        } else {
            unscheduleSelf(this);
        }
    }

    @Override
    public int getIntrinsicWidth() {
        return mIntrinsicWidth;
    }

    @Override
    public int getIntrinsicHeight() {
        return mIntrinsicHeight;
    }

    @Override
    public int getOpacity() {
        return PixelFormat.UNKNOWN;
    }

    @Override
    public void setAlpha(int alpha) {
    }

    @Override
    public void setColorFilter(ColorFilter cf) {
    }

    @Override
    public boolean isRunning() {
        return mRunning;
    }

    @Override
    public void start() {
        if (!isRunning()) {
            mRunning = true;
            if (!mAnimateOnLoad) {
                mDone = true;
            }
            mLastFrameTime = SystemClock.uptimeMillis();
            run();
        }
    }

    @Override
    public void stop() {
        if (isRunning()) {
            unscheduleSelf(this);
        }
    }

    @Override
    public void scheduleSelf(Runnable what, long when) {
        if (mAnimationEnabled) {
            super.scheduleSelf(what, when);
            mScheduled = true;
        }
    }

    @Override
    public void unscheduleSelf(Runnable what) {
        super.unscheduleSelf(what);
        mRunning = false;
    }

    /**
     * Moves to the next frame.
     */
    @Override
    public void run() {
        if (mRecycled) {
            return;
        }

        // Send request to decoder to read the next frame
        if (!mDone) {
            sDecoderHandler.sendMessage(sDecoderHandler.obtainMessage(READ_FRAME_REQ, this));
        }
    }

    /**
     * Restarts decoding the image from the beginning.  Called from the background thread.
     */
    private void reset() {
        // Return to the position of the first image frame in the stream.
        mPosition = mGifImage.mHeaderSize;
        mBackupSaved = false;
        mFrameCount = 0;
        mDisposalMethod = DISPOSAL_METHOD_UNKNOWN;
    }

    /**
     * Restarts animation if a limited number of loops of animation have been previously done.
     */
    public void restartAnimation() {
        if (mDone && mLoopCount > 0) {
            reset();
            mDone = false;
            mLoopIndex = 0;
            run();
        }
    }

    /**
     * Reads color table as 256 RGB integer values.  Called from the background thread.
     *
     * @param ncolors int number of colors to read
     */
    private void readColorTable(int[] colorTable, int ncolors) {
        for (int i = 0; i < ncolors; i++) {
            int r = mData[mPosition++] & 0xff;
            int g = mData[mPosition++] & 0xff;
            int b = mData[mPosition++] & 0xff;
            colorTable[i] = 0xff000000 | (r << 16) | (g << 8) | b;
        }
    }

    /**
     * Reads GIF content blocks.  Called from the background thread.
     *
     * @return true if the next frame has been parsed successfully, false if EOF
     *         has been reached
     */
    private void readNextFrame() {
        // Don't clear the image if it is a terminator.
        if ((mData[mPosition] & 0xff) == 0x3b) {
          mEndOfFile = true;
          return;
        }
        disposeOfLastFrame();

        mDisposalMethod = DISPOSAL_METHOD_UNKNOWN;
        mTransparency = false;

        mEndOfFile = false;
        mNextFrameDelay = 100;
        mLocalColorTable = null;

        while (true) {
            int code = mData[mPosition++] & 0xff;
            switch (code) {
                case 0:     // Empty block, ignore
                    break;
                case 0x21: // Extension.  Extensions precede the corresponding image.
                    code = mData[mPosition++] & 0xff;
                    switch (code) {
                        case 0xf9: // graphics control extension
                            readGraphicControlExt();
                            break;
                        case 0xff: // application extension
                            readBlock();
                            boolean netscape = true;
                            for (int i = 0; i < NETSCAPE2_0.length; i++) {
                                if (mBlock[i] != NETSCAPE2_0[i]) {
                                    netscape = false;
                                    break;
                                }
                            }
                            if (netscape) {
                                readNetscapeExtension();
                            } else {
                                skip(); // don't care
                            }
                            break;
                        case 0xfe:// comment extension
                            skip();
                            break;
                        case 0x01:// plain text extension
                            skip();
                            break;
                        default: // uninteresting extension
                            skip();
                    }
                    break;

                case 0x2C: // Image separator
                    readBitmap();
                    return;

                case 0x3b: // Terminator
                    mEndOfFile = true;
                    return;

                default:  // We don't know what this is. Just skip it.
                    break;
            }
        }
    }

    /**
     * Disposes of the previous frame.  Called from the background thread.
     */
    private void disposeOfLastFrame() {
        if (mFirstFrame) {
          mFirstFrame = false;
          return;
        }
        switch (mDisposalMethod) {
            case DISPOSAL_METHOD_UNKNOWN:
            case DISPOSAL_METHOD_LEAVE: {
                mBackupSaved = false;
                break;
            }
            case DISPOSAL_METHOD_RESTORE: {
                if (mBackupSaved) {
                    System.arraycopy(mBackup, 0, mColors, 0, mBackup.length);
                }
                break;
            }
            case DISPOSAL_METHOD_BACKGROUND: {
                mBackupSaved = false;

                // Fill last image rect area with background color
                int color = 0;
                if (!mTransparency) {
                    color = mBackgroundColor;
                }
                for (int i = 0; i < mFrameHeight; i++) {
                    int n1 = (mFrameY + i) * mIntrinsicWidth + mFrameX;
                    int n2 = n1 + mFrameWidth;
                    for (int k = n1; k < n2; k++) {
                        mColors[k] = color;
                    }
                }
                break;
            }
        }
    }

    /**
     * Reads Graphics Control Extension values.  Called from the background thread.
     */
    private void readGraphicControlExt() {
        mPosition++; // Block size, fixed

        int packed = mData[mPosition++] & 0xff; // Packed fields

        mDisposalMethod = (packed & 0x1c) >> 2;  // Disposal method
        mTransparency = (packed & 1) != 0;
        mNextFrameDelay = readShort() * 10; // Delay in milliseconds

        // It seems that there are broken tools out there that set a 0ms or 10ms
        // timeout when they really want a "default" one.
        // Following WebKit's lead (http://trac.webkit.org/changeset/73295)
        // we use 10 frames per second as the default frame rate.
        if (mNextFrameDelay <= 10) {
            mNextFrameDelay = 100;
        }

        mTransparentColorIndex = mData[mPosition++] & 0xff;

        mPosition++; // Block terminator - ignore
    }

    /**
     * Reads Netscape extension to obtain iteration count.  Called from the background thread.
     */
    private void readNetscapeExtension() {
        int count;
        do {
            count = readBlock();
        } while ((count > 0) && !mError);
    }

    /**
     * Reads next frame image.  Called from the background thread.
     */
    private void readBitmap() {
        mFrameX = readShort(); // (sub)image position & size
        mFrameY = readShort();

        int width = readShort();
        int height = readShort();

        // Clamp the frame dimensions to the intrinsic dimensions.
        mFrameWidth = Math.min(width, mIntrinsicWidth - mFrameX);
        mFrameHeight = Math.min(height, mIntrinsicHeight - mFrameY);

        // The frame step is set to the specfied frame width before clamping.
        mFrameStep = width;

        // Increase the size of the decoding buffer if necessary.
        int framePixelCount = width * height;
        if (framePixelCount > mPixels.length) {
            mPixels = new byte[framePixelCount];
        }

        int packed = mData[mPosition++] & 0xff;
        // 3 - sort flag
        // 4-5 - reserved lctSize = 2 << (packed & 7);
        // 6-8 - local color table size
        mInterlace = (packed & 0x40) != 0;
        mLocalColorTableUsed = (packed & 0x80) != 0; // 1 - local color table flag interlace
        mLocalColorTableSize = (int) Math.pow(2, (packed & 0x07) + 1);

        if (mLocalColorTableUsed) {
            if (mLocalColorTable == null) {
                mLocalColorTable = new int[256];
            }
            readColorTable(mLocalColorTable, mLocalColorTableSize);
            mActiveColorTable = mLocalColorTable;
        } else {
            mActiveColorTable = mGifImage.mGlobalColorTable;
            if (mGifImage.mBackgroundIndex == mTransparentColorIndex) {
                mBackgroundColor = 0;
            }
        }
        int savedColor = 0;
        if (mTransparency) {
            savedColor = mActiveColorTable[mTransparentColorIndex];
            mActiveColorTable[mTransparentColorIndex] = 0;
        }

        if (mActiveColorTable == null) {
            mError = true;
        }

        if (mError) {
            return;
        }

        decodeBitmapData();

        skip();

        if (mError) {
            return;
        }

        if (mDisposalMethod == DISPOSAL_METHOD_RESTORE) {
            backupFrame();
        }

        populateImageData();

        if (mTransparency) {
            mActiveColorTable[mTransparentColorIndex] = savedColor;
        }

        mFrameCount++;
    }

    /**
     * Stores the relevant portion of the current frame so that it can be restored
     * before the next frame is rendered.  Called from the background thread.
     */
    private void backupFrame() {
        if (mBackupSaved) {
            return;
        }

        if (mBackup == null) {
            mBackup = null;
            try {
                mBackup = new int[mColors.length];
            } catch (OutOfMemoryError e) {
                Log.e(TAG, "GifDrawable.backupFrame threw an OOME", e);
            }
        }

        if (mBackup != null) {
            System.arraycopy(mColors, 0, mBackup, 0, mColors.length);
            mBackupSaved = true;
        }
    }

    /**
     * Decodes LZW image data into pixel array.  Called from the background thread.
     */
    private void decodeBitmapData() {
        int npix = mFrameWidth * mFrameHeight;

        // Initialize GIF data stream decoder.
        int dataSize = mData[mPosition++] & 0xff;
        int clear = 1 << dataSize;
        int endOfInformation = clear + 1;
        int available = clear + 2;
        int oldCode = -1;
        int codeSize = dataSize + 1;
        int codeMask = (1 << codeSize) - 1;
        for (int code = 0; code < clear; code++) {
            mPrefix[code] = 0; // XXX ArrayIndexOutOfBoundsException
            mSuffix[code] = (byte) code;
        }

        // Decode GIF pixel stream.
        int datum = 0;
        int bits = 0;
        int first = 0;
        int top = 0;
        int pi = 0;
        while (pi < npix) {
            int blockSize = mData[mPosition++] & 0xff;
            if (blockSize == 0) {
                break;
            }

            int blockEnd = mPosition + blockSize;
            while (mPosition < blockEnd) {
                datum += (mData[mPosition++] & 0xff) << bits;
                bits += 8;

                while (bits >= codeSize) {
                    // Get the next code.
                    int code = datum & codeMask;
                    datum >>= codeSize;
                    bits -= codeSize;

                    // Interpret the code
                    if (code == clear) {
                        // Reset decoder.
                        codeSize = dataSize + 1;
                        codeMask = (1 << codeSize) - 1;
                        available = clear + 2;
                        oldCode = -1;
                        continue;
                    }

                    // Check for explicit end-of-stream
                    if (code == endOfInformation) {
                        mPosition = blockEnd;
                        return;
                    }

                    if (oldCode == -1) {
                        mPixels[pi++] = mSuffix[code];
                        oldCode = code;
                        first = code;
                        continue;
                    }

                    int inCode = code;
                    if (code >= available) {
                        mPixelStack[top++] = (byte) first;
                        code = oldCode;
                        if (top == MAX_BITS) {
                            mError =  true;
                            return;
                        }
                    }

                    while (code >= clear) {
                        if (code >= MAX_BITS || code == mPrefix[code]) {
                            mError =  true;
                            return;
                        }

                        mPixelStack[top++] = mSuffix[code];
                        code = mPrefix[code];

                        if (top == MAX_BITS) {
                            mError =  true;
                            return;
                        }
                    }

                    first = mSuffix[code];
                    mPixelStack[top++] = (byte) first;

                    // Add new code to the dictionary
                    if (available < MAX_STACK_SIZE) {
                        mPrefix[available] = (short) oldCode;
                        mSuffix[available] = (byte) first;
                        available++;

                        if (((available & codeMask) == 0) && (available < MAX_STACK_SIZE)) {
                            codeSize++;
                            codeMask += available;
                        }
                    }

                    oldCode = inCode;

                    // Drain the pixel stack.
                    do {
                        mPixels[pi++] = mPixelStack[--top];
                    } while (top > 0);
                }
            }
        }

        while (pi < npix) {
            mPixels[pi++] = 0; // clear missing pixels
        }
    }

    /**
     * Populates the color array with pixels for the next frame.
     */
    private void populateImageData() {

        // Copy each source line to the appropriate place in the destination
        int pass = 1;
        int inc = 8;
        int iline = 0;
        for (int i = 0; i < mFrameHeight; i++) {
            int line = i;
            if (mInterlace) {
                if (iline >= mFrameHeight) {
                    pass++;
                    switch (pass) {
                        case 2:
                            iline = 4;
                            break;
                        case 3:
                            iline = 2;
                            inc = 4;
                            break;
                        case 4:
                            iline = 1;
                            inc = 2;
                            break;
                        default:
                            break;
                    }
                }
                line = iline;
                iline += inc;
            }
            line += mFrameY;
            if (line < mIntrinsicHeight) {
                int k = line * mIntrinsicWidth;
                int dx = k + mFrameX; // start of line in dest
                int dlim = dx + mFrameWidth; // end of dest line

                // It is unnecesary to test if dlim is beyond the edge of the destination line,
                // since mFrameWidth is clamped to a maximum of mIntrinsicWidth - mFrameX.

                int sx = i * mFrameStep; // start of line in source
                while (dx < dlim) {
                    // map color and insert in destination
                    int index = mPixels[sx++] & 0xff;
                    int c = mActiveColorTable[index];
                    if (c != 0) {
                        mColors[dx] = c;
                    }
                    dx++;
                }
            }
        }
    }

    /**
     * Reads next variable length block from input.  Called from the background thread.
     *
     * @return number of bytes stored in "buffer"
     */
    private int readBlock() {
        int blockSize = mData[mPosition++] & 0xff;
        if (blockSize > 0) {
            System.arraycopy(mData, mPosition, mBlock, 0, blockSize);
            mPosition += blockSize;
        }
        return blockSize;
    }

    /**
     * Reads next 16-bit value, LSB first.  Called from the background thread.
     */
    private int readShort() {
        // read 16-bit value, LSB first
        int byte1 = mData[mPosition++] & 0xff;
        int byte2 = mData[mPosition++] & 0xff;
        return byte1 | (byte2 << 8);
    }

    /**
     * Skips variable length blocks up to and including next zero length block.
     * Called from the background thread.
     */
    private void skip() {
        int blockSize;
        do {
            blockSize = mData[mPosition++] & 0xff;
            mPosition += blockSize;
        } while (blockSize > 0);
    }

    @Override
    public boolean handleMessage(Message msg) {
        if (msg.what == BaseGifDrawable.READ_FRAME_RESP) {
            mFrameDelay = msg.arg1;
            if (mBitmap != null) {
                mBitmap.setPixels(mColors, 0, mIntrinsicWidth,
                        0, 0, mIntrinsicWidth, mIntrinsicHeight);
                postProcessFrame(mBitmap);
                mFirstFrameReady = true;
                mScheduled = false;
                invalidateSelf();
            }
            return true;
        }

        return false;
    }

    /**
     * Gives a subclass a chance to apply changes to the mutable bitmap
     * before showing the frame.
     */
    protected void postProcessFrame(Bitmap bitmap) {
    }

    /**
     * Background thread that handles reading and decoding frames of GIF images.
     */
    private static class DecoderThread extends HandlerThread
            implements android.os.Handler.Callback {
        private static final String DECODER_THREAD_NAME = "GifDecoder";

        public DecoderThread() {
            super(DECODER_THREAD_NAME);
        }

        @Override
        public boolean handleMessage(Message msg) {
            BaseGifDrawable gif = (BaseGifDrawable) msg.obj;
            if (gif == null || gif.mBitmap == null || gif.mRecycled) {
                return true;
            }

            switch (msg.what) {

                case READ_FRAME_REQ:
                    // Processed on background thread
                    do {
                        try {
                            gif.readNextFrame();
                        } catch (ArrayIndexOutOfBoundsException e) {
                            gif.mEndOfFile = true;
                        }

                        // Check for EOF
                        if (gif.mEndOfFile) {
                            if (gif.mFrameCount == 0) {
                                // could not read first frame
                                gif.mError = true;
                            } else if (gif.mFrameCount > 1) {
                                if (gif.mLoopCount == 0 || ++gif.mLoopIndex < gif.mLoopCount) {
                                    // Repeat the animation
                                    gif.reset();
                                } else {
                                    gif.mDone = true;
                                }
                            } else {
                                // Only one frame.  Mark as done.
                                gif.mDone = true;
                            }
                        }
                    } while (gif.mEndOfFile && !gif.mError && !gif.mDone);
                    gif.mHandler.sendMessage(gif.mHandler.obtainMessage(READ_FRAME_RESP,
                            gif.mNextFrameDelay, 0));
                    return true;

                case RESET_DECODER:
                    gif.reset();
                    return true;
            }

            return false;
        }
    }
}
