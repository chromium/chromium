To regenerate the key/certificate pair for the signed-exchanges in this
directory, run `generate-test-certs.sh`.

The signed exchange files in this directory are generated using
`generate-test-sxgs.sh`. it requires command-line tools in the
[webpackage repository][1]. To install them, run:
```
go get -u github.com/WICG/webpackage/go/signedexchange/cmd/...
export PATH=$PATH:$(go env GOPATH)/bin
```
The revision of the tools used to generate the test files is `cf19833`.

[1]: https://github.com/WICG/webpackage
