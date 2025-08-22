#!/bin/sh

# cd into project root
cd $(dirname $(realpath $0))/../

if test -z "$(which openssl 2> /dev/null)"; then
  echo "Please install OpenSSL"
  exit -1
fi

openssl req -x509 -noenc -days 3650 -newkey rsa:2048 -keyout ./cert.key -out cert.pem -subj "/C=IT/O=Gloom Inc./OU=Gloom Gaming Department"
