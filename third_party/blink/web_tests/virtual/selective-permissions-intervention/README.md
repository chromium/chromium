# Selective Permissions Intervention

This virtual test suite runs tests with the `SelectivePermissionsIntervention` feature flag enabled.

It is used to verify the behavior of the intervention, specifically ensuring that when an ad-tagged resource requests restricted APIs (like Geolocation), the request is intercepted and a corresponding `InspectorIssue` is generated for DevTools.
