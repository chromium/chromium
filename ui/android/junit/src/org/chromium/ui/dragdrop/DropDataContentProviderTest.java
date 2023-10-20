// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.verify;

import android.net.Uri;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.FileNotFoundException;

/**
 * Test basic functionality of {@link DropDataContentProvider}.
 * <p>
 * The content provider acts as a wrapper for the impl class {@link DropDataProviderImpl} so the
 * tests will verify the wiring is correct.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class DropDataContentProviderTest {
    private DropDataContentProvider mDropDataContentProvider;

    @Mock public DropDataProviderImpl mDropDataProviderImplMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mDropDataContentProvider = new DropDataContentProvider();
        mDropDataContentProvider.setDropDataProviderImpl(mDropDataProviderImplMock);
    }

    @Test
    public void test_getType() {
        Uri dummyUri = Uri.parse("");

        mDropDataContentProvider.getType(dummyUri);

        verify(mDropDataProviderImplMock).getType(dummyUri);
    }

    @Test
    public void test_getStreamTypes() {
        Uri dummyUri = Uri.parse("");
        String dummyMimeType = "Type";

        mDropDataContentProvider.getStreamTypes(dummyUri, dummyMimeType);

        verify(mDropDataProviderImplMock).getStreamTypes(dummyUri, dummyMimeType);
    }

    @Test
    public void test_openFile() throws FileNotFoundException {
        Uri dummyUri = Uri.parse("");
        String dummyMode = "mode";

        mDropDataContentProvider.openFile(dummyUri, dummyMode);

        verify(mDropDataProviderImplMock).openFile(mDropDataContentProvider, dummyUri);
    }

    @Test
    public void test_query() throws FileNotFoundException {
        Uri dummyUri = Uri.parse("");
        String[] dummyProjections = new String[] {"proj1"};

        mDropDataContentProvider.query(
                dummyUri, dummyProjections, anyString(), new String[] {}, anyString());

        verify(mDropDataProviderImplMock).query(dummyUri, dummyProjections);
    }

    @Test
    public void test_call() throws FileNotFoundException {
        String dummyMethodName = "mode";
        String dummyArgs = "args";
        Bundle dummyBundle = new Bundle();

        mDropDataContentProvider.call(dummyMethodName, dummyArgs, dummyBundle);

        verify(mDropDataProviderImplMock).call(dummyMethodName, dummyArgs, dummyBundle);
    }
}
