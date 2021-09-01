This virtual suite runs AppCache tests with the "AppCacheRequireOriginTrial"
base::Feature.

These all use the same origin trial token,
generated with the following script:
```
tools/origin_trials/generate_token.py http://127.0.0.1:8000 AppCache --expire-days=2000
```

Token:
```
AmZ7wG5YnWV9EawT19S1NX38Vn4e3I3pb8Sv3L2j73QvzELzyeCNpwjWASfhNESLU5WGQJHHYgrbfoI1PcTIaQwAAABmeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImlzU3ViZG9tYWluIjogZmFsc2UsICJmZWF0dXJlIjogIkFwcENhY2hlIiwgImV4cGlyeSI6IDE3NDM2Mjk4OTR9
```
