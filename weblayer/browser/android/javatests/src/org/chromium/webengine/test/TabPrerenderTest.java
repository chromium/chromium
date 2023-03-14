// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.hamcrest.CoreMatchers.is;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.webengine.Navigation;
import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;

import java.util.concurrent.CountDownLatch;

/**
 * Tests prerendering when tab navigations happen before the UI is shown.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class TabPrerenderTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();
    @Rule
    public DigitalAssetLinksServerRule mDALServerRule = new DigitalAssetLinksServerRule();

    private Tab mTab;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchShell();

        mDALServerRule.setUpDigitalAssetLinks();

        WebSandbox sandbox = mActivityTestRule.getWebSandbox();
        WebEngine webEngine = runOnUiThreadBlocking(() -> sandbox.createWebEngine()).get();
        mTab = runOnUiThreadBlocking(() -> webEngine.getTabManager().createTab()).get();

        String scriptContents = "const div = document.createElement('div');"
                + "div.id = 'id1';"
                + "div.innerText = 'Div1';"
                + "document.body.appendChild(div);";
        mDALServerRule.getServer().setResponse("/script.js", scriptContents, null);
    }

    private void navigateToPage(String pageContents) throws Exception {
        String url = mDALServerRule.getServer().setResponse("/page.html", pageContents, null);

        CountDownLatch navigationCompletedLatch = new CountDownLatch(1);
        runOnUiThreadBlocking(()
                                      -> mTab.getNavigationController().registerNavigationObserver(
                                              new NavigationObserver() {
                                                  @Override
                                                  public void onNavigationCompleted(
                                                          Tab tab, Navigation navigation) {
                                                      navigationCompletedLatch.countDown();
                                                  }
                                              }));

        runOnUiThreadBlocking(() -> mTab.getNavigationController().navigate(url));
        navigationCompletedLatch.await();
    }

    private String executeScript(String scriptContents) {
        try {
            return runOnUiThreadBlocking(() -> mTab.executeScript(scriptContents, false)).get();
        } catch (Exception e) {
            Assert.fail("Unexpected exception executing script: " + e.getMessage());
            return null;
        }
    }

    @SmallTest
    @Test
    public void pageContentsAreAvailable() throws Exception {
        navigateToPage("<html><head></head><body><div id=\"id1\">Div1</div></body>");
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    executeScript("document.querySelector('#id1').innerText"), is("\"Div1\""));
        });
    }

    @SmallTest
    @Test
    public void blockingScriptChangesAreAvailable() throws Exception {
        navigateToPage("<html><head></head><body><script src=\"/script.js\"></script></body>");

        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    executeScript("document.getElementsByTagName('script').length"), is("1"));
            Criteria.checkThat(
                    executeScript("document.querySelector('#id1').innerText"), is("\"Div1\""));
        });
    }

    @SmallTest
    @Test
    public void asyncScriptChangesAreAvailable() throws Exception {
        navigateToPage(
                "<html><head></head><body><script async src=\"/script.js\"></script></body>");

        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    executeScript("document.getElementsByTagName('script').length"), is("1"));
            Criteria.checkThat(
                    executeScript("document.querySelector('#id1').innerText"), is("\"Div1\""));
        });
    }

    @SmallTest
    @Test
    public void deferredScriptChangesAreAvailable() throws Exception {
        navigateToPage(
                "<html><head></head><body><script defer src=\"/script.js\"></script></body>");
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    executeScript("document.getElementsByTagName('script').length"), is("1"));
            Criteria.checkThat(
                    executeScript("document.querySelector('#id1').innerText"), is("\"Div1\""));
        });
    }

    @MediumTest
    @Test
    public void asyncTasksInScriptsExecute() throws Exception {
        navigateToPage(
                "<html><head></head><body><script>setTimeout(() => document.body.appendChild(document.createElement('div')), 2000);</script></body>");

        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    executeScript("document.getElementsByTagName('div').length"), is("0"));
        });
        Thread.sleep(5000);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    executeScript("document.getElementsByTagName('div').length"), is("1"));
        });
    }
}
