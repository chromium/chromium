const otMeta = document.createElement('meta');
otMeta.httpEquiv = 'origin-trial';
// Token generated with:
// generate_token.py https://not-web-platform.test:8444 FedCmIdpSigninStatus \
// --expire-timestamp=2000000000 --is-third-party --version 3
otMeta.content = 'AwA3XEmYRswcJLUrv4iWI4mFxJcns306ZPZ56qhUMJTHpzID1I9z0Dq9AVaxfWZ/nZax6w83hB7y1YDqlF0rGwsAAAB/eyJvcmlnaW4iOiAiaHR0cHM6Ly9ub3Qtd2ViLXBsYXRmb3JtLnRlc3Q6ODQ0NCIsICJmZWF0dXJlIjogIkZlZENtSWRwU2lnbmluU3RhdHVzIiwgImV4cGlyeSI6IDIwMDAwMDAwMDAsICJpc1RoaXJkUGFydHkiOiB0cnVlfQ==';
document.head.append(otMeta);
