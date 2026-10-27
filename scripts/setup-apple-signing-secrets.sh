#!/usr/bin/env bash
# Populate THE's repository Actions secrets from local files/environment.
# Passwords are entered privately; no credential values are logged or retained.
set +x
set -euo pipefail

repo=adesutherland/THE
cert_dir="${APPLE_CERT_DIR:-$HOME/Documents/Certs}"
mode="${1:-}"
case "$mode" in
  ''|--available|--check) ;;
  *) echo "Usage: bash $0 [--available|--check]" >&2; exit 2 ;;
esac

names=(
  APPLE_DEVELOPER_ID_CERTIFICATE_BASE64
  APPLE_DEVELOPER_ID_CERTIFICATE_PASSWORD
  APPLE_DEVELOPER_ID_IDENTITY
  APPLE_DEVELOPER_ID_INSTALLER_CERTIFICATE_BASE64
  APPLE_DEVELOPER_ID_INSTALLER_CERTIFICATE_PASSWORD
  APPLE_DEVELOPER_ID_INSTALLER_IDENTITY
  APPLE_ID
  APPLE_APP_SPECIFIC_PASSWORD
  APPLE_TEAM_ID
)
values=()
missing=()

# Validate each existing Base64 file against its local export without printing it.
for stem in developer-id-application developer-id-installer; do
  if [[ -f "$cert_dir/$stem.txt" && -f "$cert_dir/$stem.p12" ]]; then
    if ! base64 -D -i "$cert_dir/$stem.txt" 2>/dev/null | cmp -s - "$cert_dir/$stem.p12"; then
      echo "$stem.txt does not match its .p12 export." >&2
      exit 1
    fi
  fi
done

for name in "${names[@]}"; do
  value="${!name:-}"
  if [[ -z "$value" ]]; then
    case "$name" in
      APPLE_DEVELOPER_ID_CERTIFICATE_BASE64)
        if [[ -f "$cert_dir/developer-id-application.txt" ]]; then
          value="$(tr -d '\r\n' < "$cert_dir/developer-id-application.txt")"
        fi ;;
      APPLE_DEVELOPER_ID_INSTALLER_CERTIFICATE_BASE64)
        if [[ -f "$cert_dir/developer-id-installer.txt" ]]; then
          value="$(tr -d '\r\n' < "$cert_dir/developer-id-installer.txt")"
        fi ;;
      APPLE_DEVELOPER_ID_IDENTITY)
        value='Developer ID Application: Adrian   Sutherland (S4ESV5CK46)' ;;
      APPLE_DEVELOPER_ID_INSTALLER_IDENTITY)
        value='Developer ID Installer: Adrian   Sutherland (S4ESV5CK46)' ;;
      APPLE_TEAM_ID) value=S4ESV5CK46 ;;
    esac
  fi
  if [[ -z "$value" && -z "$mode" && -t 0 ]]; then
    if [[ "$name" == *_PASSWORD ]]; then
      read -r -s -p "$name: " value
      printf '\n' >&2
    elif [[ "$name" == APPLE_ID ]]; then
      read -r -p 'Apple Developer account email: ' value
    fi
  fi
  if [[ -z "$value" ]]; then missing+=("$name"); fi
  values+=("$value")
done

if [[ "$mode" == --check ]]; then
  for i in "${!names[@]}"; do
    if [[ -n "${values[$i]}" ]]; then printf 'Available: %s\n' "${names[$i]}"
    else printf 'Missing: %s\n' "${names[$i]}"; fi
  done
  exit 0
fi

if [[ -z "$mode" && ${#missing[@]} -gt 0 ]]; then
  printf 'Missing credential: %s\n' "${missing[@]}" >&2
  echo 'Run this script in your local terminal to enter them privately.' >&2
  exit 1
fi

# Check any supplied export password before changing GitHub. -nokeys never
# exports the private key; SHA-1 below is the public certificate fingerprint.
check_export() {
  local encoded="$1" password="$2" expected="$3" fingerprint
  [[ -n "$encoded" && -n "$password" ]] || return 0
  export THE_SIGNING_EXPORT_PASSWORD="$password"
  fingerprint="$(printf '%s' "$encoded" | base64 -D | openssl pkcs12 -clcerts -nokeys -passin env:THE_SIGNING_EXPORT_PASSWORD 2>/dev/null | openssl x509 -noout -fingerprint -sha1 2>/dev/null | sed 's/.*=//;s/://g')" || fingerprint=''
  if [[ -z "$fingerprint" ]]; then
    fingerprint="$(printf '%s' "$encoded" | base64 -D | openssl pkcs12 -legacy -clcerts -nokeys -passin env:THE_SIGNING_EXPORT_PASSWORD 2>/dev/null | openssl x509 -noout -fingerprint -sha1 2>/dev/null | sed 's/.*=//;s/://g')" || fingerprint=''
  fi
  unset THE_SIGNING_EXPORT_PASSWORD
  [[ "$fingerprint" == "$expected" ]] || {
    echo 'Certificate export password or certificate identity does not match. No secrets uploaded.' >&2
    exit 1
  }
}
check_export "${values[0]}" "${values[1]}" 9AB0CBB6E1C3A4FCF72F3E0DE7BC1B0770A2DD68
check_export "${values[3]}" "${values[4]}" E583E2693CDBB3C3665EE47E12571A4706DA279B

gh auth status >/dev/null 2>&1 || { echo 'Run gh auth login first.' >&2; exit 1; }
for i in "${!names[@]}"; do
  [[ -n "${values[$i]}" ]] || continue
  printf '%s' "${values[$i]}" | env -u GH_DEBUG gh secret set "${names[$i]}" --repo "$repo" >/dev/null
  printf 'Set: %s\n' "${names[$i]}"
done
if [[ ${#missing[@]} -gt 0 ]]; then
  printf 'Still needed: %s\n' "${missing[@]}"
fi
echo 'Configured secret names on GitHub:'
gh secret list --repo "$repo" --json name --jq '.[].name'
