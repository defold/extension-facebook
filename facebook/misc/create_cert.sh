#!/usr/bin/env bash
set -eu

openssl req -x509 -newkey rsa:2048 -days 30 -subj "/CN=Defold Team"         \
    -nodes -keyout "key.pem" -out "cert.pem"  \
    > /dev/null 2>&1
