// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

/** A collection of test GURLs. */
public class JUnitTestGURLs {
    public static final GURL EXAMPLE_URL = new GURL("https://www.example.com/");
    public static final GURL HTTP_URL = new GURL("http://www.example.com/");
    public static final GURL URL_1 = new GURL("https://www.one.com/");
    public static final GURL URL_1_WITH_PATH = new GURL("https://www.one.com/some_path.html");
    public static final GURL URL_1_WITH_PDF_PATH = new GURL("https://www.one.com/some_path.pdf");
    public static final GURL URL_2 = new GURL("https://www.two.com/");
    public static final GURL URL_3 = new GURL("https://www.three.com/");
    public static final GURL MAPS_URL = new GURL("https://maps.google.com/");
    public static final GURL SEARCH_URL = new GURL("https://www.google.com/search?q=test");
    public static final GURL SEARCH_2_URL = new GURL("https://www.google.com/search?q=query");
    public static final GURL INITIAL_URL = new GURL("https://initial.com");
    public static final GURL NTP_URL = new GURL("chrome://newtab/");
    public static final GURL NTP_NATIVE_URL = new GURL("chrome-native://newtab/");
    public static final GURL RED_1 = new GURL("https://www.red.com/page1");
    public static final GURL RED_2 = new GURL("https://www.red.com/page2");
    public static final GURL RED_3 = new GURL("https://www.red.com/page3");
    public static final GURL BLUE_1 = new GURL("https://www.blue.com/page1");
    public static final GURL BLUE_2 = new GURL("https://www.blue.com/page2");
    public static final GURL BLUE_3 = new GURL("https://www.blue.com/page3");
    public static final GURL TEXT_FRAGMENT_URL =
            new GURL("https://www.example.com/#:~:text=selector");
    public static final GURL INVALID_URL = new GURL("http://0x100.0/");
    public static final GURL GOOGLE_URL = new GURL("http://www.google.com/");
    public static final GURL GOOGLE_URL_DOGS = new GURL("http://www.google.com/dogs");
    public static final GURL GOOGLE_URL_DOG = new GURL("http://www.google.com/dog");
    public static final GURL GOOGLE_URL_CAT = new GURL("http://www.google.com/cat");
    public static final GURL ABOUT_BLANK = new GURL("about:blank");
    public static final GURL CHROME_ABOUT = new GURL("chrome://about");
}
