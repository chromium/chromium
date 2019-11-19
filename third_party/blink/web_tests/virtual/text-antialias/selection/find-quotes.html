<html>
<head>
    <script>
        function canFind(target, specimen)
        {
            getSelection().empty();
            document.body.innerHTML = specimen;
            document.execCommand("FindString", false, target);
            var result = getSelection().rangeCount != 0;
            getSelection().empty();
            return result;
        }

        var apostrophe = "'";
        var hebrewPunctuationGeresh = String.fromCharCode(0x05F3);
        var hebrewPunctuationGershayim = String.fromCharCode(0x05F4);
        var leftDoubleQuotationMark = String.fromCharCode(0x201C);
        var leftSingleQuotationMark = String.fromCharCode(0x2018);
        var quotationMark = '"';
        var rightDoubleQuotationMark = String.fromCharCode(0x201D);
        var rightSingleQuotationMark = String.fromCharCode(0x2019);

        var success = true;

        var message = "FAILURE:";

        function testFindExpectingSuccess(targetName, specimenName)
        {
            var target = eval(targetName);
            var specimen = eval(specimenName);
            if (canFind(target, specimen))
                return;
            success = false;
            message += " Cannot find " + specimenName + " when searching for " + targetName + ".";
        }

        function testFindExpectingFailure(targetName, specimenName)
        {
            var target = eval(targetName);
            var specimen = eval(specimenName);
            if (!canFind(target, specimen))
                return;
            success = false;
            message += " Found " + specimenName + " when searching for " + targetName + ".";
        }

        function runTests()
        {
            if (window.testRunner)
                testRunner.dumpAsText();

            var singleQuotes = [ "apostrophe", "hebrewPunctuationGeresh", "leftSingleQuotationMark", "rightSingleQuotationMark" ];
            var doubleQuotes = [ "quotationMark", "hebrewPunctuationGershayim", "leftDoubleQuotationMark", "rightDoubleQuotationMark" ];

            for (var i = 0; i < singleQuotes.length; ++i) {
                for (var j = 0; j < singleQuotes.length; ++j)
                    testFindExpectingSuccess(singleQuotes[i], singleQuotes[j]);
            }

            for (var i = 0; i < doubleQuotes.length; ++i) {
                for (var j = 0; j < doubleQuotes.length; ++j)
                    testFindExpectingSuccess(doubleQuotes[i], doubleQuotes[j]);
            }

            for (var i = 0; i < singleQuotes.length; ++i) {
                for (var j = 0; j < doubleQuotes.length; ++j)
                    testFindExpectingFailure(singleQuotes[i], doubleQuotes[j]);
            }

            for (var i = 0; i < doubleQuotes.length; ++i) {
                for (var j = 0; j < singleQuotes.length; ++j)
                    testFindExpectingFailure(doubleQuotes[i], singleQuotes[j]);
            }

            if (success)
                message = "SUCCESS: Found all the quotes as expected.";

            document.body.innerHTML = message;
        }
    </script>
</head>
<body onload="runTests()"></body>
</html>
