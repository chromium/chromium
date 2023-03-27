// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import android.app.Activity;
import android.content.Context;
import android.text.TextUtils;

import androidx.test.InstrumentationRegistry;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseActivityTestRule;

import java.io.File;
import java.io.OutputStreamWriter;
import java.io.Writer;

/**
 * Base ActivityTestRule for WebEngine instrumentation tests.
 *
 * This rule contains some common setup needed to deal with WebEngine's multiple classloaders.
 */
abstract class WebEngineActivityTestRule<T extends Activity> extends BaseActivityTestRule<T> {
    private static final String COMMAND_LINE_FILE = "weblayer-command-line";

    public WebEngineActivityTestRule(Class<T> clazz) {
        super(clazz);
    }

    /**
     * Writes the command line file. This can be useful if a test needs to dynamically add command
     * line arguments before WebLayer has been loaded.
     */
    public void writeCommandLineFile() throws Exception {
        // The CommandLine instance we have here will not be picked up in the
        // implementation since they use different class loaders, so we need to write
        // all the switches to the WebLayer command line file.
        try (Writer writer = new OutputStreamWriter(
                     InstrumentationRegistry.getInstrumentation().getTargetContext().openFileOutput(
                             COMMAND_LINE_FILE, Context.MODE_PRIVATE),
                     "UTF-8")) {
            writer.write(TextUtils.join(" ", CommandLine.getJavaSwitchesOrNull()));
        }
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    writeCommandLineFile();
                    base.evaluate();
                } finally {
                    new File(InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getFilesDir(),
                            COMMAND_LINE_FILE)
                            .delete();
                }
            }
        };
    }
}
