package org.chromium.base.task.test;

import org.chromium.base.task.AsyncTask;
import org.robolectric.annotation.internal.DoNotInstrument;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

/**
 * Bridge between shadows and {@link org.chromium.base.task.AsyncTask}.
 */
@DoNotInstrument
public class ShadowAsyncTaskBridge<Result> {
    private AsyncTask<Result> asyncTask;

    public ShadowAsyncTaskBridge(AsyncTask<Result> asyncTask) {
        this.asyncTask = asyncTask;
    }

    public Result doInBackground() {
        return ReflectionHelpers.callInstanceMethod(asyncTask, "doInBackground");
    }

    public void onPreExecute() {
        ReflectionHelpers.callInstanceMethod(asyncTask, "onPreExecute");
    }

    public void onPostExecute(Result result) {
        ReflectionHelpers.callInstanceMethod(
                asyncTask, "onPostExecute", ClassParameter.from(Object.class, result));
    }

    public void onCancelled() {
        ReflectionHelpers.callInstanceMethod(asyncTask, "onCancelled");
    }
}
