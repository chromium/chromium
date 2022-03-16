This suite runs tests with:
    --enable-blink-features=HighlightOverlayPainting

When debugging tests locally, results will only match TestExpectations
if you also enable the related features with one of the following:

    --enable-blink-features=CSSSpellingGrammarErrors,HighlightAPI
    --enable-blink-test-features
    --run-web-tests

More details: <https://crbug.com/1147859>
