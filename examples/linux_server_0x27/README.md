

To compile the client you may need `libssl-dev`

```sh
openssl genpkey -algorithm RSA -out private_key.pem
openssl rsa -pubout -in private_key.pem -out public_key.pem
openssl rsa -pubout -in private_key.pem -outform DER -out public_key.der
```

https://github.com/amosnier/sha-2/