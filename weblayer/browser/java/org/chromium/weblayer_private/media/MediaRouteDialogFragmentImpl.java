// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.media;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.InflateException;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.view.Window;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentController;
import androidx.fragment.app.FragmentHostCallback;
import androidx.fragment.app.FragmentManager;

import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.weblayer_private.R;
import org.chromium.weblayer_private.RemoteFragmentImpl;
import org.chromium.weblayer_private.interfaces.IMediaRouteDialogFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.lang.reflect.Constructor;

/**
 * WebLayer's implementation of the client library's MediaRouteDialogFragment.
 *
 * This class is the impl-side representation of a client fragment which is added to the browser
 * fragment, and is parent to MediaRouter-related {@link DialogFragment} instances. This class will
 * automatically clean up the client-side fragment when the child fragment is detached.
 *
 * This class is modeled after {@link SiteSettingsFragmentImpl}, see it for details.
 */
public class MediaRouteDialogFragmentImpl extends RemoteFragmentImpl {
    // The WebLayer-wrapped context object. This context gets assets and resources from WebLayer,
    // not from the embedder. Use this for the most part, especially to resolve WebLayer-specific
    // resource IDs.
    private Context mContext;

    private boolean mStarted;
    private FragmentController mFragmentController;

    /**
     * A fake FragmentActivity needed to make the Fragment system happy.
     *
     * See {@link SiteSettingsFragmentImpl#PassthroughFragmentActivity}.
     * TODO(crbug.com/1123216): remove this class.
     */
    private static class PassthroughFragmentActivity extends FragmentActivity {
        private static final Class<?>[] VIEW_CONSTRUCTOR_ARGS =
                new Class[] {Context.class, AttributeSet.class};

        private final MediaRouteDialogFragmentImpl mFragmentImpl;

        int getThemeResource(int attr) {
            TypedValue value = new TypedValue();
            return getTheme().resolveAttribute(attr, value, true) ? value.resourceId : 0;
        }

        private PassthroughFragmentActivity(MediaRouteDialogFragmentImpl fragmentImpl) {
            mFragmentImpl = fragmentImpl;
            attachBaseContext(mFragmentImpl.getWebLayerContext());
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
                getLayoutInflater().setFactory2(this);
            }
            AppCompatDelegate.create(this, null);
            // TODO(estade): this is necessary because MediaRouter dialogs crash if the theme has an
            // action bar. It's unclear why this is necessary when it's not in Chrome, and why
            // ContextThemeWrapper doesn't work.
            getTheme().applyStyle(R.style.Theme_BrowserUI, true);
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.LAYOUT_INFLATER_SERVICE.equals(name)) {
                return getLayoutInflater();
            }
            return getEmbedderActivity().getSystemService(name);
        }

        @Override
        public LayoutInflater getLayoutInflater() {
            return (LayoutInflater) getBaseContext().getSystemService(
                    Context.LAYOUT_INFLATER_SERVICE);
        }

        @Override
        public View onCreateView(View parent, String name, Context context, AttributeSet attrs) {
            if (name.indexOf('.') == -1) {
                return null;
            }

            Class<? extends View> clazz = null;
            try {
                clazz = context.getClassLoader().loadClass(name).asSubclass(View.class);
                LayoutInflater inflater = getLayoutInflater();
                if (inflater.getFilter() != null && !inflater.getFilter().onLoadClass(clazz)) {
                    throw new InflateException(attrs.getPositionDescription()
                            + ": Class not allowed to be inflated " + name);
                }

                Constructor<? extends View> constructor =
                        clazz.getConstructor(VIEW_CONSTRUCTOR_ARGS);
                constructor.setAccessible(true);
                View view = constructor.newInstance(new Object[] {context, attrs});
                if (view instanceof ViewStub) {
                    // Use the same Context when inflating ViewStub later.
                    ViewStub viewStub = (ViewStub) view;
                    viewStub.setLayoutInflater(inflater.cloneInContext(context));
                }
                return view;
            } catch (Exception e) {
                InflateException ie = new InflateException(attrs.getPositionDescription()
                        + ": Error inflating class "
                        + (clazz == null ? "<unknown>" : clazz.getName()));
                ie.initCause(e);
                throw ie;
            }
        }

        @Override
        public Window getWindow() {
            return getEmbedderActivity().getWindow();
        }

        @Override
        public Context getApplicationContext() {
            return getEmbedderActivity().getApplicationContext();
        }

        @Override
        public void startActivity(Intent intent) {
            getEmbedderActivity().startActivity(intent);
        }

        @Override
        public void setTitle(int titleId) {
            getEmbedderActivity().setTitle(mFragmentImpl.getWebLayerContext().getString(titleId));
        }

        @Override
        public void setTitle(CharSequence title) {
            getEmbedderActivity().setTitle(title);
        }

        private Activity getEmbedderActivity() {
            return mFragmentImpl.getActivity();
        }
    }

    private static class MediaRouteDialogFragmentHostCallback
            extends FragmentHostCallback<Context> {
        private final MediaRouteDialogFragmentImpl mFragmentImpl;

        private MediaRouteDialogFragmentHostCallback(MediaRouteDialogFragmentImpl fragmentImpl) {
            super(new PassthroughFragmentActivity(fragmentImpl), new Handler(), 0);
            mFragmentImpl = fragmentImpl;
        }

        @Override
        public Context onGetHost() {
            return mFragmentImpl.getWebLayerContext();
        }

        @Override
        public LayoutInflater onGetLayoutInflater() {
            Context context = mFragmentImpl.getWebLayerContext();
            return ((LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE))
                    .cloneInContext(context);
        }

        @Override
        public boolean onHasView() {
            return mFragmentImpl.getView() != null;
        }

        @Override
        public View onFindViewById(int id) {
            return onHasView() ? mFragmentImpl.getView().findViewById(id) : null;
        }
    }

    public MediaRouteDialogFragmentImpl(IRemoteFragmentClient remoteFragmentClient) {
        super(remoteFragmentClient);
    }

    @Override
    public void onAttach(Context context) {
        StrictModeWorkaround.apply();
        super.onAttach(context);

        mContext = ClassLoaderContextWrapperFactory.get(context);
        mFragmentController =
                FragmentController.createController(new MediaRouteDialogFragmentHostCallback(this));

        // Remove the host fragment as soon as the media router dialog fragment is detached.
        mFragmentController.getSupportFragmentManager().registerFragmentLifecycleCallbacks(
                new FragmentManager.FragmentLifecycleCallbacks() {
                    @Override
                    public void onFragmentDetached(FragmentManager fm, Fragment f) {
                        MediaRouteDialogFragmentImpl.this.removeFragmentFromFragmentManager();
                    }
                },
                false);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        StrictModeWorkaround.apply();
        mFragmentController.attachHost(null);

        super.onCreate(savedInstanceState);

        mFragmentController.dispatchCreate();
    }

    @Override
    public void onDestroyView() {
        StrictModeWorkaround.apply();
        super.onDestroyView();
        mFragmentController.dispatchDestroyView();
    }

    @Override
    public void onDestroy() {
        StrictModeWorkaround.apply();
        super.onDestroy();
        mFragmentController.dispatchDestroy();
    }

    @Override
    public void onDetach() {
        StrictModeWorkaround.apply();
        super.onDetach();
        mContext = null;
    }

    @Override
    public void onStart() {
        super.onStart();

        if (!mStarted) {
            mStarted = true;
            mFragmentController.dispatchActivityCreated();
        }
        mFragmentController.noteStateNotSaved();
        mFragmentController.execPendingActions();
        mFragmentController.dispatchStart();
    }

    @Override
    public void onStop() {
        super.onStop();
        mFragmentController.dispatchStop();
    }

    @Override
    public void onResume() {
        super.onResume();
        mFragmentController.dispatchResume();
    }

    @Override
    public void onPause() {
        super.onPause();
        mFragmentController.dispatchPause();
    }

    public IMediaRouteDialogFragment asIMediaRouteDialogFragment() {
        return new IMediaRouteDialogFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                StrictModeWorkaround.apply();
                return MediaRouteDialogFragmentImpl.this;
            }
        };
    }

    public static MediaRouteDialogFragmentImpl fromRemoteFragment(IRemoteFragment remoteFragment) {
        return (MediaRouteDialogFragmentImpl) remoteFragment;
    }

    public FragmentManager getSupportFragmentManager() {
        return mFragmentController.getSupportFragmentManager();
    }

    private Context getWebLayerContext() {
        return mContext;
    }
}
