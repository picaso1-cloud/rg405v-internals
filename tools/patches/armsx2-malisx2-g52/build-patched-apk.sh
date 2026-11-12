#!/bin/bash
# Build a MaliSX2-enabled ARMSX2 nightly for Mali-G52 (RG405V).
#
#   ./build-patched-apk.sh <official-nightly.apk> [workdir]
#
# Needs: apktool (2.12+), java, python3, zipalign, apksigner, keytool.
# The output is debug-signed; uninstall any official nightly before installing.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
APK="${1:-}"
if [ ! -f "$APK" ]; then
  echo "usage: $0 <official-nightly.apk> [workdir]" >&2
  exit 1
fi
WORK="${2:-$HERE/build}"
APKTOOL_JAR="${APKTOOL_JAR:-/usr/share/android-apktool/apktool.jar}"
SMALI_MAIN="com.android.tools.smali.smali.Main"

rm -rf "$WORK"
mkdir -p "$WORK"

echo "== decompile =="
apktool d -f -o "$WORK/src" "$APK" >/dev/null

echo "== patch ek2.smali =="
python3 "$HERE/patch-ek2.py" "$WORK/src/smali/ek2.smali"

echo "== assemble classes.dex =="
java -cp "$APKTOOL_JAR" "$SMALI_MAIN" assemble --api 26 \
  -o "$WORK/classes.dex" "$WORK/src/smali"

echo "== swap classes.dex into the original apk =="
cp "$APK" "$WORK/patched.apk"
mkdir -p "$WORK/dex"
cp "$WORK/classes.dex" "$WORK/dex/classes.dex"
( cd "$WORK/dex" && zip -q -j ../patched.apk classes.dex )
zip -q -d "$WORK/patched.apk" "META-INF/*.RSA" "META-INF/*.SF" "META-INF/*.MF" 2>/dev/null || true

echo "== align + sign =="
zipalign -f 4 "$WORK/patched.apk" "$WORK/aligned.apk"
KS="$WORK/debug.keystore"
if [ ! -f "$KS" ]; then
  keytool -genkeypair -v -keystore "$KS" -storepass android \
    -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 \
    -validity 10000 -dname "CN=Android Debug,O=Android,C=US" >/dev/null
fi
apksigner sign --ks "$KS" --ks-pass pass:android --key-pass pass:android \
  --out "$WORK/ARMSX2-MaliSX2-G52-patched.apk" "$WORK/aligned.apk"
apksigner verify --print-certs "$WORK/ARMSX2-MaliSX2-G52-patched.apk" | head -3

echo
echo "done: $WORK/ARMSX2-MaliSX2-G52-patched.apk"
echo "install: adb uninstall com.armsx2.nightly; adb install <output>"
