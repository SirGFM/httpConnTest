# HTTP Connectivity Test

Simple test for checking how different HTTP server configurations reply
to a cURL client. This client was written in C++11 mostly to simplify
resource management.

## Quick Guide

```sh
# Build everything
go build .
make -C curlClient main
# Run the server in port 8080 (the default)
./httpConnTest -mode default &
# Run the client
./curlClient/main http://localhost:8080/ echo
```

## Tested versions

* Go: 1.9.2 linux/amd64
* cURL: 7.35.0-1ubuntu2.20 (libcurl4-openssl-dev:amd64)
