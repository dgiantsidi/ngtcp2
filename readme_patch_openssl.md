

The patch `picotls_openssl3.3.3.patch` has been created to support `ngtcp2` compilation in `Azure Linux` due to `OpenSSL` incompatibility (`PTLS_OPENSSL_HAVE_CHACHA20_POLY1305` is undefined, probably due to `OpenSSL` version 1.1.1 vs 3.3.3 which comes with Azure Linux).

To apply it, execute:

```sh
cd picotls
```

```sh 
patch -p1 < picotls_openssl3.3.3.patch
```