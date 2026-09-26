#!/bin/bash
# Make the Developer ID certificate and the notarisation key which CI has
# as secrets ready for mac-package.sh, which is told of them through
# GITHUB_ENV. The certificate goes into a keychain of its own, which lasts
# as long as the job does.
#
# Needs MACOS_CERT_P12 (the certificate and its key, exported as .p12 and
# base64-encoded), MACOS_CERT_PASSWORD, and optionally NOTARY_KEY (an App
# Store Connect API key, the .p8's contents), NOTARY_KEY_ID and
# NOTARY_ISSUER_ID

set -e

kc=$RUNNER_TEMP/sign.keychain-db
p12=$RUNNER_TEMP/cert.p12
pw=$(openssl rand -base64 24)

echo "$MACOS_CERT_P12" | base64 --decode > "$p12"
security create-keychain -p "$pw" "$kc"
security set-keychain-settings -lut 21600 "$kc"
security unlock-keychain -p "$pw" "$kc"
security import "$p12" -k "$kc" -P "$MACOS_CERT_PASSWORD" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple: -s -k "$pw" "$kc" >/dev/null
security list-keychains -d user -s "$kc" \
   $(security list-keychains -d user | tr -d '"')
rm -f "$p12"

id=$(security find-identity -v -p codesigning "$kc" \
     | sed -n 's/.*"\(Developer ID Application: .*\)"/\1/p' | head -1)
if [ -z "$id" ]; then
   echo "no Developer ID Application identity in MACOS_CERT_P12" >&2
   exit 1
fi
echo "MACOS_SIGN_IDENTITY=$id" >> "$GITHUB_ENV"

if [ -n "$NOTARY_KEY" ]; then
   echo "$NOTARY_KEY" > "$RUNNER_TEMP/notary.p8"
   {
      echo "NOTARY_KEY_FILE=$RUNNER_TEMP/notary.p8"
      echo "NOTARY_KEY_ID=$NOTARY_KEY_ID"
      echo "NOTARY_ISSUER_ID=$NOTARY_ISSUER_ID"
   } >> "$GITHUB_ENV"
fi
