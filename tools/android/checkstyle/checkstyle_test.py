#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for checkstyle.py."""

import inspect
import os
import re
import subprocess
import sys
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor

_THIS_DIR = os.path.dirname(__file__)
_SRC_ROOT = os.path.normpath(os.path.join(_THIS_DIR, '..', '..', '..'))
_CHECKSTYLE_PATH = os.path.abspath(os.path.join(_THIS_DIR, 'checkstyle.py'))


def _run_checkstyle_in_worker(content):
    with tempfile.NamedTemporaryFile(mode='w', suffix='.java') as f:
        f.write(content)
        f.flush()
        result = subprocess.run([sys.executable, _CHECKSTYLE_PATH, f.name],
                                capture_output=True,
                                text=True,
                                cwd=_SRC_ROOT)
        return result.stdout + result.stderr


# Decorator to associate the test .java with each method so it can be retrieved
# in order to run checkstyle concurrently.
def java(content):

    def decorator(func):
        setattr(func, '_java_content', content)
        return func

    return decorator


class CheckstyleTest(unittest.TestCase):
    _executor = None
    _results = {}

    @classmethod
    def setUpClass(cls):
        cls._executor = ThreadPoolExecutor()
        # Run checkstyle on all tests at once to not be too slow.
        for name, method in inspect.getmembers(cls, inspect.isfunction):
            if hasattr(method, '_java_content'):
                content = method._java_content
                cls._results[name] = cls._executor.submit(
                    _run_checkstyle_in_worker, content)

    @classmethod
    def tearDownClass(cls):
        cls._executor.shutdown()

    def _result(self):
        return self._results[self._testMethodName].result()

    def _check(self, value=None):
        result = self._result()
        if value:
            self.assertIn(value, result)
        else:
            self.assertEqual('', result)

    @java("""
class A {
    public static void main(String args[]) {
    }
}
""")
    def test_ArrayTypeStyle(self):
        self._check('Array brackets at illegal position')

    @java("""
import java.util.HashSet;
class A {
    void test() {
        new HashSet<String>() {{
            add("foo");
        }};
    }
}
""")
    def test_AvoidDoubleBraceInitialization(self):
        self._check('Avoid double brace initialization')

    @java("""
class A {
    void test(int a) {
        switch (a) {
            default:
                break;
            case 1:
                break;
        }
    }
}
""")
    def test_DefaultComesLast(self):
        self._check('Default should be last label in the switch')

    @java("""
class A {
    void test() {
        ;
    }
}
""")
    def test_EmptyStatement(self):
        self._check('Empty statement')

    @java("""
class A {
    void test() {
        int a;
        int b = a = 1;
    }
}
""")
    def test_InnerAssignment(self):
        self._check('Inner assignments should be avoided')

    @java("""
class A {
    void test() {
        int a; int b;
    }
}
""")
    def test_OneStatementPerLine(self):
        self._check('Only one statement per line allowed')

    @java("""
interface A {
    public void test();
}
""")
    def test_RedundantModifier(self):
        self._check("Redundant 'public' modifier")

    @java("""
class A {
    void test(String s) {
        if (s == "foo") {
        }
    }
}
""")
    def test_StringLiteralEquality(self):
        self._check('Literal Strings should be compared using equals()')

    @java("""
import java.util.*;
class A {
}
""")
    def test_AvoidStarImport(self):
        self._check("Using the '.*' form of import should be avoided")

    @java("""
class A {
    void test() {
        try {
        } catch (Exception e) {
        }
    }
}
""")
    def test_IllegalCatch(self):
        self._check()

    @java("""
class A {
    static public void main(String[] args) {
    }
}
""")
    def test_ModifierOrder(self):
        self._check('modifier out of order')

    @java("""
import java.lang.String;
class A {
    String s;
}
""")
    def test_RedundantImport(self):
        self._check('Redundant import from the java.lang package')

    @java("""
import java.util.List;
class A {
}
""")
    def test_UnusedImports(self):
        self._check('Unused import')

    @java("""
public class A {
}
""")
    def test_JavadocType(self):
        self._check()

    @java("""
/** A class */
public class A {
    public void test() {
    }
}
""")
    def test_JavadocMethod(self):
        self._check()

    @java("""
package MyPackage;
class A {
}
""")
    def test_PackageName(self):
        self._check("Name 'MyPackage' must match pattern")

    @java("""
class A {
    boolean test(boolean a) {
        return a == true;
    }
}
""")
    def test_SimplifyBooleanExpression(self):
        self._check('Expression can be simplified')

    @java("""
class A {
    boolean test(boolean a) {
        if (a) {
            return true;
        } else {
            return false;
        }
    }
}
""")
    def test_SimplifyBooleanReturn(self):
        self._check('Conditional logic can be removed')

    @java("""
class a {
}
""")
    def test_TypeName(self):
        self._check("Name 'a' must match pattern")

    @java("""
class A {
    static final int my_const = 1;
}
""")
    def test_ConstantName(self):
        self._check('Static final field names must either be all caps')

    @java("""
class A {
    private int myMember;
}
""")
    def test_MemberName(self):
        self._check('Non-public, non-static field names start with m')

    @java("""
class A {
    static int myStatic;
}
""")
    def test_StaticVariableName(self):
        self._check('Static field names start with s')

    @java("""
class A {
    void MyMethod() {
    }
}
""")
    def test_MethodName(self):
        self._check('Method names should start with a lower case letter')

    @java("""
class A {
    void test(int MyParam) {
    }
}
""")
    def test_ParameterName(self):
        self._check("Name 'MyParam' must match pattern")

    @java("""
class A {
    void test() {
        final int MY_VAR = 1; // bad
    }
}
""")
    def test_LocalFinalVariableName_bad(self):
        self._check('Local variables should be camel-cased')

    @java("""
class A {
    void test() {
        final int myVar2 = 1; // good
    }
}
""")
    def test_LocalFinalVariableName_good(self):
        self._check()

    @java("""
class A {
    void test() {
        int mMyVar = 1;
    }
}
""")
    def test_LocalVariableName_bad(self):
        self._check('Local variables should be camel-cased')

    @java("""
class A {
    void test() {
        int myVar = 1;
    }
}
""")
    def test_LocalVariableName_good(self):
        self._check()

    @java("""
class A {
    void test(int a) {
        switch (a) {
            case 1:
                a++;
            case 2:
                a++;
        }
    }
}
""")
    def test_FallThrough(self):
        self._check('Fall through from previous branch')

    @java("""
class A {
    protected void finalize() {
    }
}
""")
    def test_NoFinalizer(self):
        self._check('finalize() is banned')

    @java("""
class A {
    void test(int a) {
        if (a == 1)
            a++;
    }
}
""")
    def test_NeedBraces(self):
        self._check("'if' construct must use '{}'s")

    @java("""
class A {
    void test() {
        StringBuffer sb = new StringBuffer();
    }
}
""")
    def test_StringBufferCheck(self):
        self._check('Avoid StringBuffer; use StringBuilder instead')

    @java("""
import android.app.AlertDialog;
class A {
    void test() {
        new AlertDialog.Builder(null);
    }
}
""")
    def test_AlertDialogCheck(self):
        self._check('Avoid android.app.AlertDialog')

    @java("""
import android.content.Context;
import android.preference.PreferenceManager;
class A {
    void test(Context context) {
        PreferenceManager.getDefaultSharedPreferences(context);
    }
}
""")
    def test_UseSharedPreferencesManagerFromChromeCheck(self):
        self._check('Use SharedPreferencesManager instead to access app-wide')

    @java("""
import android.content.Context;
import android.preference.PreferenceManager;
class A {
    void test(Context context) {
        PreferenceManager.getDefaultSharedPreferences(context);
    }
}
""")
    def test_DefaultSharedPreferencesCheck(self):
        self._check('Use ContextUtils.getAppSharedPreferences() instead')

    @java("""
import java.util.concurrent.ConcurrentHashMap;
class A {
    void test() {
        new ConcurrentHashMap<String, String>();
    }
}
""")
    def test_ConcurrentHashMapCheck(self):
        self._check('ConcurrentHashMap has a bug on some Android versions')

    @java("""
import android.os.StrictMode;
class A {
    void test() {
        try (StrictModeContext x = StrictModeContext.allowDiskWrites()) {
        }
    }
}
""")
    def test_StrictModeContextIgnoredCheck(self):
        self._check('Please name the StrictModeContext variable "ignored"')

    @java("""
class A {
    void test() {
        System.exit(1);
    }
}
""")
    def test_SystemExitCheck(self):
        self._check('Throw an exception instead of calling System#exit')

    @java("""
import android.widget.Button;
class MyButton extends Button {
    public MyButton(android.content.Context c) { super(c); }
}
""")
    def test_AndroidButtonWidgetCheck(self):
        self._check('Use org.chromium.ui.widget.ButtonCompat instead')

    @java("""
import android.widget.TextView;
class A {
    void test(TextView tv) {
        tv.setTextColor(0);
        tv.setTextSize(1);
    }
}
""")
    def test_SetTextColorAndSetTextSizeCheck(self):
        self._check('please use #setTextAppearance')

    @java("""
import androidx.test.core.app.ActivityScenario;
import android.app.Activity;
class A {
    void test() {
        ActivityScenario.launch(Activity.class);
    }
}
""")
    def test_ActivityScenarioLaunch(self):
        self._check('Launching an ActivityScenario manually is error prone')

    @java("""
import com.google.common.base.Optional;
class A {
    Optional<String> s;
}
""")
    def test_GuavaOptional(self):
        self._check('Use java.util.Optional instead')

    @java("""
import java.util.HashSet;
class A {
    void test() {
        new HashSet<String>() {};
    }
}
""")
    def test_CollectionSubclass(self):
        self._check('Subclassing collections is uncommon')

    @java("""
import android.view.accessibility.AccessibilityManager;
class A {
    AccessibilityManager am;
}
""")
    def test_AccessibilityManagerUsage(self):
        self._check('Use org.chromium.ui.accessibility.AccessibilityState')

    @java("""
class A {
    int style = R.style.ThemeOverlay_BrowserUI_Fullscreen;
}
class R { public static class style {
    public static final int ThemeOverlay_BrowserUI_Fullscreen = 1;
}}
""")
    def test_FullscreenDialogs(self):
        self._check('Fullscreen AlertDialogs must use FullscreenAlertDialog')

    @java("""
class A {
    // dummy
}
""")
    def test_InclusiveLanguageCheck(self):
        self._check('Please use inclusive language where possible')

    @java("""
public class MyPreferenceFragment extends PreferenceFragment {}
""")
    def test_SettingsFragmentCheck(self):
        self._check('Top level settings fragment should be named with suffix')

    @java("""
class A {
    void test(/* comment */ int p) {}
}
""")
    def test_ParamComments_1(self):
        self._check(
            'Parameter comments should use the ErrorProne-aware syntax')

    @java("""
class A {
    void test(int p, /* comment */ int b) {}
}
""")
    def test_ParamComments_2(self):
        self._check(
            'Parameter comments should use the ErrorProne-aware syntax')

    @java("""
class A {
    void test(int p, int b /* comment */) {}
}
""")
    def test_ParamComments_3(self):
        self._check(
            'Parameter comments should use the ErrorProne-aware syntax')

    @java("""
class A {
    void test(int p, int b /* comment */, int x) {}
}
""")
    def test_ParamComments_4(self):
        self._check(
            'Parameter comments should use the ErrorProne-aware syntax')

    @java("""
/**
 * comment * */
class A {}
""")
    def test_ClosingJavadocs(self):
        self._check("Closing a javadoc only takes a single")

    @java("""
class A {
    interface Observer { void onAction(); }
    void addObserver(Observer o) {}
    void removeObserver(Observer o) {}
    void test() {
        removeObserver(this::onAction);
    }
    void onAction() {}
}
""")
    def test_RemoveObserverMethodReference(self):
        self._check('Each method reference creates a new instance')

    @java("""
import org.mockito.MockitoAnnotations;
class A {
    void test() {
        MockitoAnnotations.initMocks(this);
    }
}
""")
    def test_MockitoInitMocks(self):
        self._check('Prefer to use MockitoJUnit.Rule for consistency')

    @java("""
import android.view.Window;
class A {
    void test(Window w) {
        WindowCompat.setDecorFitsSystemWindows(w, false);
    }
}
""")
    def test_SetDecorFitsSystemWindowsCheck(self):
        self._check('setDecorFitsSystemWindows will change the window')

    @java("""
import com.google.android.material.elevation.ElevationOverlayProvider;
class A {
    ElevationOverlayProvider p;
}
""")
    def test_ElevationOverlayProvider(self):
        self._check('Elevation based surface color is being deprecated')

    @java("""
import android.view.ContextMenu;
class A {
    ContextMenu m;
}
""")
    def test_AndroidContextMenu(self):
        self._check("Android context menu has been proven not compatible")

    @java("""
enum MyEnum { A, B }
""")
    def test_IntDefsNotEnums(self):
        self._check('Use an @IntDef / @StringDef / @LongDef')

    @java("""
import androidx.annotation.VisibleForTesting;
class A {
    @VisibleForTesting
    void testForTesting() {}
}
""")
    def test_VisibleForTestingForTesting(self):
        self._check(
            'There is no need to add @VisibleForTesting to test-only methods')

    @java("""
import android.content.Context;
class A {
    void test(Context c) {
        c.getResources().getString(0);
    }
}
""")
    def test_ContextGetString_bad(self):
        self._check('Use Context.getString()')

    @java("""
import android.content.Context;
class A {
    void test(Context c) {
        c.getString(0);
        thing.getContext().getString(0);
    }
}
""")
    def test_ContextGetString_good(self):
        self._check()

    @java("""
import org.robolectric.annotation.Config;
@Config(minSdk = 21)
class A {}
""")
    def test_RobolecticMinSdk(self):
        self._check(
            '@Config(minSdk=...) parameterizes tests across every SDK level')

    @java("""
import androidx.annotation.Nullable;
import org.chromium.build.annotations.NullMarked;
@NullMarked class MarkImportsAsUsed<T extends @Nullable Object> {}
""")
    def test_WrongNullable_AndroidX(self):
        self._check('Use org.chromium.build.annotations.Nullable instead of ')

    @java("""
import javax.annotation.Nullable;
import org.chromium.build.annotations.NullMarked;
@NullMarked class MarkImportsAsUsed<T extends @Nullable Object> {}
""")
    def test_WrongNullable_JavaX(self):
        self._check('Use org.chromium.build.annotations.Nullable instead of ')

    @java("""
import androidx.annotation.Nullable;
class MarkImportsAsUsed<T extends @Nullable Object> {}
""")
    def test_WrongNullable_NonNullMarked(self):
        self._check()

    @java("""
class MarkImportsAsUsed<T extends @NonNull Object> {}
""")
    def test_NonNull(self):
        self._check('Values are @NonNull by default. Use @NonNull')

if __name__ == '__main__':
    # Only Chromium Linux checkouts have a Java runtime.
    if sys.platform == 'linux':
        unittest.main()
