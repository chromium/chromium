(function () {
  var id = "{{GET[id]}}";
  var expect = "{{GET[expect]}}";
  var testName = "{{GET[testName]}}";
  var reportLocation = "resources/report.py?op=retrieve&id=" + id;

  var reportTest = async_test(testName);
  reportTest.step(function () {

    var report = new XMLHttpRequest();
    report.onload = reportTest.step_func(function () {

        var data = JSON.parse(report.responseText);

        if (data.error) {
          assert_equals("error", expect, data.error);
        } else if (data.url) {
          assert_equals("url", expect, data.url);
        }

        reportTest.done();
    });

    report.open("GET", reportLocation, true);
    report.send();
  });
})();
