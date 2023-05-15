// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.webengine.shell

import android.os.Bundle

import org.chromium.webengine.Tab
import org.chromium.webengine.TabListObserver
import org.chromium.webengine.WebEngine
import org.chromium.webengine.WebFragment
import org.chromium.webengine.WebSandbox

import androidx.activity.addCallback
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.guava.await

class WebEngineSinglePageActivity : AppCompatActivity() {

    private lateinit var mWebEngine : WebEngine;

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.spa)

        lifecycleScope.launch {
            val sandbox : WebSandbox = WebSandbox.create(getApplicationContext()).await()
            mWebEngine = sandbox.createWebEngine().await()

            mWebEngine.getTabManager().registerTabListObserver(object : TabListObserver {
                override fun onTabAdded(webEngine : WebEngine, tab : Tab)  {
                    tab.setActive();
                }
            })

            mWebEngine.getTabManager().getActiveTab()!!.getNavigationController().navigate("https://sadchonks.com")
            supportFragmentManager.beginTransaction()
                .add(R.id.fragment_container_view, mWebEngine.getFragment())
                .setReorderingAllowed(true)
                .commit()
        }
    }

    override fun onBackPressed() {
        lifecycleScope.launch {
            if (!mWebEngine.tryNavigateBack().await()) {
                finish();
            }
        }
    }
}
