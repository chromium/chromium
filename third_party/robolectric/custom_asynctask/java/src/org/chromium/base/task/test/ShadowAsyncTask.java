package org.chromium.base.task.test;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;

import java.util.concurrent.Callable;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

@Implements(AsyncTask.class)
public class ShadowAsyncTask<Result> {
    @RealObject
    private AsyncTask<Result> realAsyncTask;

    private final FutureTask<Result> future;
    private final Callable<Result> worker;
    private @AsyncTask.Status int status = AsyncTask.Status.PENDING;

    public ShadowAsyncTask() {
        worker = new Callable<Result>() {
            @Override
            public Result call() {
                return getBridge().doInBackground();
            }
        };
        future = new FutureTask<Result>(worker) {
            @Override
            protected void done() {
                status = AsyncTask.Status.FINISHED;
                try {
                    final Result result = get();

                    try {
                        ShadowApplication.getInstance().getForegroundThreadScheduler().post(
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        if (realAsyncTask instanceof BackgroundOnlyAsyncTask)
                                            return;
                                        getBridge().onPostExecute(result);
                                    }
                                });
                    } catch (Throwable t) {
                        throw new OnPostExecuteException(t);
                    }
                } catch (CancellationException e) {
                    ShadowApplication.getInstance().getForegroundThreadScheduler().post(
                            new Runnable() {
                                @Override
                                public void run() {
                                    getBridge().onCancelled();
                                }
                            });
                } catch (InterruptedException e) {
                    // Ignore.
                } catch (OnPostExecuteException e) {
                    throw new RuntimeException(e.getCause());
                } catch (Throwable t) {
                    throw new RuntimeException(
                            "An error occured while executing doInBackground()", t.getCause());
                }
            }
        };
    }

    @Implementation
    public boolean isCancelled() {
        return future.isCancelled();
    }

    @Implementation
    public boolean cancel(boolean mayInterruptIfRunning) {
        return future.cancel(mayInterruptIfRunning);
    }

    @Implementation
    public Result get() throws InterruptedException, ExecutionException {
        return future.get();
    }

    @Implementation
    public Result get(long timeout, TimeUnit unit)
            throws InterruptedException, ExecutionException, TimeoutException {
        return future.get(timeout, unit);
    }

    public AsyncTask<Result> executeInRobolectric() {
        status = AsyncTask.Status.RUNNING;
        getBridge().onPreExecute();

        ShadowApplication.getInstance().getBackgroundThreadScheduler().post(new Runnable() {
            @Override
            public void run() {
                future.run();
            }
        });

        return realAsyncTask;
    }

    @SuppressWarnings("unchecked")
    @Implementation
    public AsyncTask<Result> executeOnExecutor(Executor executor) {
        status = AsyncTask.Status.RUNNING;
        getBridge().onPreExecute();

        executor.execute(new Runnable() {
            @Override
            public void run() {
                future.run();
            }
        });

        return realAsyncTask;
    }

    @Implementation
    public @AsyncTask.Status int getStatus() {
        return status;
    }

    private ShadowAsyncTaskBridge<Result> getBridge() {
        return new ShadowAsyncTaskBridge<>(realAsyncTask);
    }

    private static class OnPostExecuteException extends Exception {
        public OnPostExecuteException(Throwable throwable) {
            super(throwable);
        }
    }
}
