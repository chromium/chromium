// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.ContextWrapper;
import android.os.Bundle;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.InflateException;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.fragment.app.FragmentController;
import androidx.fragment.app.FragmentHostCallback;
import androidx.fragment.app.FragmentManager;

import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.lang.reflect.Constructor;

/**
 * A base class for RemoteFragmentImpls that need to host child Fragments.
 *
 * Because Fragments created in WebLayer use the AndroidX library from WebLayer's ClassLoader, we
 * can't attach Fragments created here directly to the embedder's Fragment tree, and have to create
 * a local FragmentController to manage them. This class handles creating the FragmentController,
 * and forwards all Fragment lifecycle events from the RemoteFragment in the embedder's Fragment
 * tree to child Fragments of this class.
 */
public abstract class FragmentHostingRemoteFragmentImpl extends RemoteFragmentImpl {
    // The WebLayer-wrapped context object. This context gets assets and resources from WebLayer,
    // not from the embedder. Use this for the most part, especially to resolve WebLayer-specific
    // resource IDs.
    private Context mContext;

    private boolean mStarted;
    private FragmentController mFragmentController;

    protected static class RemoteFragmentContext
            extends ContextWrapper implements LayoutInflater.Factory2 {
        private static final Class<?>[] VIEW_CONSTRUCTOR_ARGS =
                new Class[] {Context.class, AttributeSet.class};

        public RemoteFragmentContext(Context webLayerContext) {
            super(webLayerContext);
        }

        // This method is needed to work around a LayoutInflater bug in Android <N.  Before
        // LayoutInflater creates an instance of a View, it needs to look up the class by name to
        // get a reference to its Constructor. As an optimization, it caches this name to
        // Constructor mapping. This cache causes issues if a class gets loaded multiple times with
        // different ClassLoaders. In some UIs, some AndroidX Views get loaded early on with the
        // embedding app's ClassLoader, so the Constructor from that ClassLoader's version of the
        // class gets cached. When the WebLayer implementation later tries to inflate the same
        // class, it instantiates a version from the wrong ClassLoader, which leads to a
        // ClassCastException when casting that View to its original class. This was fixed in
        // Android N, but to work around it on L & M, we inflate the Views manually here, which
        // bypasses LayoutInflater's cache.
        @Override
        public View onCreateView(View parent, String name, Context context, AttributeSet attrs) {
            // If the class doesn't have a '.' in its name, it's probably a built-in Android View,
            // which are often referenced by just their class names with no package prefix. For
            // these classes we can return null to fall back to LayoutInflater's default behavior.
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
        public View onCreateView(String name, Context context, AttributeSet attrs) {
            return null;
        }

        private LayoutInflater getLayoutInflater() {
            return (LayoutInflater) getBaseContext().getSystemService(
                    Context.LAYOUT_INFLATER_SERVICE);
        }
    }

    private static class RemoteFragmentHostCallback extends FragmentHostCallback<Context> {
        private final FragmentHostingRemoteFragmentImpl mFragmentImpl;

        private RemoteFragmentHostCallback(FragmentHostingRemoteFragmentImpl fragmentImpl) {
            super(fragmentImpl.getWebLayerContext(), new Handler(), 0);
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
            // This is always false.
            return mFragmentImpl.getView() != null;
        }

        @Override
        public View onFindViewById(int id) {
            // This is always null.
            return onHasView() ? mFragmentImpl.getView().findViewById(id) : null;
        }
    }

    protected FragmentHostingRemoteFragmentImpl() {
        super();
    }

    @Override
    protected void onAttach(Context embedderContext) {
        StrictModeWorkaround.apply();
        super.onAttach(embedderContext);

        mContext = createRemoteFragmentContext(embedderContext);
        mFragmentController =
                FragmentController.createController(new RemoteFragmentHostCallback(this));
        mFragmentController.attachHost(null);

        // Some appcompat functionality depends on Fragments being hosted from within an
        // AppCompatActivity, which performs some static initialization. Even if we're running
        // within an AppCompatActivity, it will be from the embedder's ClassLoader, so in WebLayer's
        // ClassLoader the initialization hasn't occurred. Creating an AppCompatDelegate manually
        // here will perform the necessary initialization.
        if (getActivity() != null) {
            AppCompatDelegate.create(getActivity(), null);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        StrictModeWorkaround.apply();
        super.onCreate(savedInstanceState);
        mFragmentController.dispatchCreate();
    }

    @Override
    protected void onDestroyView() {
        StrictModeWorkaround.apply();
        super.onDestroyView();
        mFragmentController.dispatchDestroyView();
    }

    @Override
    protected void onDestroy() {
        StrictModeWorkaround.apply();
        super.onDestroy();
        mFragmentController.dispatchDestroy();
    }

    @Override
    protected void onDetach() {
        StrictModeWorkaround.apply();
        super.onDetach();
        mContext = null;

        // If the Fragment is retained, onDestroy won't be called during configuration changes. We
        // have to create a new FragmentController that's attached to the correct Context when
        // reattaching this Fragment, so destroy the existing one here.
        if (!mFragmentController.getSupportFragmentManager().isDestroyed()) {
            mFragmentController.dispatchDestroy();
            assert mFragmentController.getSupportFragmentManager().isDestroyed();
        }
    }

    @Override
    protected void onStart() {
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
    protected void onStop() {
        super.onStop();
        mFragmentController.dispatchStop();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mFragmentController.dispatchResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mFragmentController.dispatchPause();
    }

    public FragmentManager getSupportFragmentManager() {
        return mFragmentController.getSupportFragmentManager();
    }

    /**
     * Returns the RemoteFragmentContext that should be used in the child Fragment tree.
     *
     * Implementations will typically wrap embedderContext with ClassLoaderContextWrapperFactory,
     * and possibly set a Theme.
     */
    protected abstract RemoteFragmentContext createRemoteFragmentContext(Context embedderContext);

    protected Context getWebLayerContext() {
        return mContext;
    }
}
