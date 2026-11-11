#!/bin/bash
# build.sh — build and sign the andr36oid Panel APK without Gradle.
set -e
cd "$(dirname "$0")"

SDK=${ANDROID_SDK:-/opt/android-sdk}
BT="$SDK/build-tools/36.1.0"
PLAT="$SDK/platforms/android-34/android.jar"

[ -f "$PLAT" ] || { echo "missing $PLAT"; exit 1; }

rm -rf out
mkdir -p out/classes out/gen

echo "=== aapt2 link ==="
"$BT/aapt2" link -o out/base.apk --manifest AndroidManifest.xml -I "$PLAT" --java out/gen

echo "=== javac ==="
find src out/gen -name '*.java' > out/sources.txt
javac -source 11 -target 11 -classpath "$PLAT" -d out/classes @out/sources.txt 2>&1 | grep -v "bootstrap class path\|obsolete\|warning" || true

echo "=== d8 ==="
find out/classes -name '*.class' > out/classes.txt
"$BT/d8" --lib "$PLAT" --output out @out/classes.txt

echo "=== package ==="
( cd out && zip -q base.apk classes.dex )

echo "=== align ==="
"$BT/zipalign" -f 4 out/base.apk out/aligned.apk

if [ ! -f debug.keystore ]; then
  echo "=== keystore ==="
  keytool -genkeypair -keystore debug.keystore -alias androiddebugkey \
    -storepass android -keypass android -dname "CN=Android Debug,O=Android,C=US" \
    -keyalg RSA -keysize 2048 -validity 10000 >/dev/null 2>&1
fi

echo "=== sign ==="
"$BT/apksigner" sign --ks debug.keystore --ks-pass pass:android --key-pass pass:android \
  --out out/panel.apk out/aligned.apk

"$BT/apksigner" verify out/panel.apk && echo "SIGNED OK"
ls -lh out/panel.apk
