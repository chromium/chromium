'use strict';
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// clang-format off
// /*grit-removed-lines:4*/
// Enums aren't natively available in JS, this will ensure a rewritten TS
// sourcemap.
var ExampleEnum;
(function (ExampleEnum) {
    ExampleEnum[ExampleEnum["SOME_EXAMPLE"] = 0] = "SOME_EXAMPLE";
    ExampleEnum[ExampleEnum["OTHER_EXAMPLE"] = 1] = "OTHER_EXAMPLE";
})(ExampleEnum || (ExampleEnum = {}));
class SpecialTypeScriptProperties {
    constructor() {
        /* Protected variables are typescript only specifiers */
        this.protectedValue = 0;
    }
}
class Derived extends SpecialTypeScriptProperties {
    constructor() {
        super(...arguments);
        /* So are private variables */
        this.privateValue = 0;
    }
    method() {
        console.log(this.privateValue);
        return this.protectedValue;
    }
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoicHJlcHJvY2Vzc2VkL29yaWdpbmFsX2ZpbGUudHMiLCJzb3VyY2VzIjpbIm9yaWdpbmFsX2ZpbGUudHMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBS0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EiLCJzb3VyY2VzQ29udGVudCI6WyIvLyBDb3B5cmlnaHQgMjAyMiBUaGUgQ2hyb21pdW0gQXV0aG9ycy4gQWxsIHJpZ2h0cyByZXNlcnZlZC5cbi8vIFVzZSBvZiB0aGlzIHNvdXJjZSBjb2RlIGlzIGdvdmVybmVkIGJ5IGEgQlNELXN0eWxlIGxpY2Vuc2UgdGhhdCBjYW4gYmVcbi8vIGZvdW5kIGluIHRoZSBMSUNFTlNFIGZpbGUuXG4vLyBjbGFuZy1mb3JtYXQgb2ZmXG5cbi8vIDxpZiBleHByPVwiaXNfbWFjb3N4XCI+XG5mdW5jdGlvbiB0aGlzSXNSZW1vdmVkKCk6IGJvb2xlYW4ge1xuICByZXR1cm4gdHJ1ZTtcbn1cbi8vIDwvaWY+XG5cbi8vIEVudW1zIGFyZW4ndCBuYXRpdmVseSBhdmFpbGFibGUgaW4gSlMsIHRoaXMgd2lsbCBlbnN1cmUgYSByZXdyaXR0ZW4gVFNcbi8vIHNvdXJjZW1hcC5cbmVudW0gRXhhbXBsZUVudW0ge1xuICBTT01FX0VYQU1QTEUgPSAwLFxuICBPVEhFUl9FWEFNUExFID0gMSxcbn1cblxuYWJzdHJhY3QgY2xhc3MgU3BlY2lhbFR5cGVTY3JpcHRQcm9wZXJ0aWVzIHtcbiAgLyogUHJpdmF0ZSB2YXJpYWJsZXMgYXJlIHR5cGVzY3JpcHQgb25seSBzcGVjaWZpZXJzICovXG4gIHByb3RlY3RlZCBwcm90ZWN0ZWRWYWx1ZTogbnVtYmVyID0gMDtcblxuICBhYnN0cmFjdCBtZXRob2QoKTogbnVtYmVyO1xufVxuXG5jbGFzcyBEZXJpdmVkIGV4dGVuZHMgU3BlY2lhbFR5cGVTY3JpcHRQcm9wZXJ0aWVzIHtcbiAgcHJpdmF0ZSBwcml2YXRlVmFsdWU6IG51bWJlciA9IDA7XG5cbiAgbWV0aG9kKCkge1xuICAgIGNvbnNvbGUubG9nKHRoaXMucHJpdmF0ZVZhbHVlKTtcbiAgICByZXR1cm4gdGhpcy5wcm90ZWN0ZWRWYWx1ZTtcbiAgfVxufVxuIl19
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoib3JpZ2luYWxfZmlsZS5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbInByZXByb2Nlc3NlZC9vcmlnaW5hbF9maWxlLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7QUFBQSw0REFBNEQ7QUFDNUQseUVBQXlFO0FBQ3pFLDZCQUE2QjtBQUM3QixtQkFBbUI7QUFFbkIsMkJBQTJCO0FBRTNCLHlFQUF5RTtBQUN6RSxhQUFhO0FBQ2IsSUFBSyxXQUdKO0FBSEQsV0FBSyxXQUFXO0lBQ2QsNkRBQWdCLENBQUE7SUFDaEIsK0RBQWlCLENBQUE7QUFDbkIsQ0FBQyxFQUhJLFdBQVcsS0FBWCxXQUFXLFFBR2Y7QUFFRCxNQUFlLDJCQUEyQjtJQUExQztRQUNFLHdEQUF3RDtRQUM5QyxtQkFBYyxHQUFXLENBQUMsQ0FBQztJQUd2QyxDQUFDO0NBQUE7QUFFRCxNQUFNLE9BQVEsU0FBUSwyQkFBMkI7SUFBakQ7O1FBQ0UsOEJBQThCO1FBQ3RCLGlCQUFZLEdBQVcsQ0FBQyxDQUFDO0lBTW5DLENBQUM7SUFKQyxNQUFNO1FBQ0osT0FBTyxDQUFDLEdBQUcsQ0FBQyxJQUFJLENBQUMsWUFBWSxDQUFDLENBQUM7UUFDL0IsT0FBTyxJQUFJLENBQUMsY0FBYyxDQUFDO0lBQzdCLENBQUM7Q0FDRjtBQUVELDBoREFBMGhEIn0=