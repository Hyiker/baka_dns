# Baka the domain name server

A definition-implemented Domain Name Server(Relay)

## Roadmap

- [x] database
- [x] Base DNS relay Function(RFC1035, RFC1034)
- [x] RR Type support
- [x] cache
- [x] concurrent support
- [ ] EDNS(RFC2671) (upstream)
- [x] DOT(RFC7858, RFC8310)(self-signed naive version, downstream support)

## Use

**Create the target dir** `mkdir build`

**Configure cmake** `cd build && cmake ..`

**Create the Pem for SSL, apply and trust it (optional)** in _server.c_, alt paths for public pem and private key with *SSL_CERT_PATH* and *SSL_KEY_PATH*, and set your passphrase with *SSL_PASSPHRASE*

**Compile the project** `make`

**Run it with args** `src/baka_dns --relay=/root/Projects/baka_dns/dnsrelay.txt -v --dot`

## Features

- 🌲Hash bucket tree database for resource record storage
- ⚡️️LRU linked list cache
- ✔ EDNS0 support
- 🤔DOT(DNS over TLS) support

## Reference

- DNS definition & recommended implementation: [RFC1035](https://tools.ietf.org/html/rfc1035), [RFC1034](https://tools.ietf.org/html/rfc1034)
- [RFC1035 Header](http://www.tcpipguide.com/free/t_DNSMessageHeaderandQuestionSectionFormat.htm)
