This suite runs tests with --enable-features=UseIDNA2008NonTransitional.

To check the results against the baseline, run the following commands:
```

# wpt/url:
diff third_party/blink/web_tests/platform/linux/external/wpt/url/a-element-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/a-element-expected.txt

diff third_party/blink/web_tests/platform/linux/external/wpt/url/a-element-origin-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/a-element-origin-expected.txt

diff third_party/blink/web_tests/platform/linux/external/wpt/url/a-element-origin-xhtml-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/a-element-origin-xhtml-expected.txt

diff third_party/blink/web_tests/platform/linux/external/wpt/url/a-element-xhtml-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/a-element-xhtml-expected.txt

diff third_party/blink/web_tests/platform/wpt/url/toascii.window-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/toascii.window-expected.txt

diff third_party/blink/web_tests/platform/linux/external/wpt/url/url-constructor.any-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/url-constructor.any-expected.txt

diff third_party/blink/web_tests/platform/linux/external/wpt/url/url-constructor.any.worker-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/url-constructor.any.worker-expected.txt

diff third_party/blink/web_tests/platform/linux/external/wpt/url/url-origin.any-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/url-origin.any-expected.txt

diff third_party/blink/web_tests/platform/linux/external/wpt/url/url-origin.any.worker-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/url-origin.any.worker-expected.txt

diff third_party/blink/web_tests/platform/linux/external/wpt/url/url-setters-a-area.window-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/url-setters-a-area.window-expected.txt

diff third_party/blink/web_tests/platform/linux/external/wpt/url/url-setters.any-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/url-setters.any-expected.txt

diff third_party/blink/web_tests/platform/linux/external/wpt/url/url-setters.any.worker-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/external/wpt/url/url-setters.any.worker-expected.txt


# fast/url:
diff third_party/blink/web_tests/fast/url/idna2003-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/fast/url/idna2003-expected.txt

diff third_party/blink/web_tests/fast/url/idna2008-expected.txt \
     third_party/blink/web_tests/virtual/idna-2008/fast/url/idna2008-expected.txt
```