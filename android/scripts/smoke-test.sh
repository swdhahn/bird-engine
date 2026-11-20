#!/usr/bin/env bash
# Installs the APK on a running device or emulator, launches it, and fails
# unless the engine reaches its frame loop without crashing. Writes
# logcat.txt and screenshot.png to the working directory.
set -uo pipefail

APK="$1"
PACKAGE="dev.birdengine.sample"
TIMEOUT_SECONDS="${2:-180}"

adb install -r "$APK" || exit 1
adb logcat -c
adb shell am start -n "$PACKAGE/android.app.NativeActivity" || exit 1

# The first "FPS:" line means start() finished and frames are presenting.
elapsed=0
while [ "$elapsed" -lt "$TIMEOUT_SECONDS" ]; do
  if adb logcat -d -s Bird:I | grep -q "FPS:"; then break; fi
  if ! adb shell pidof "$PACKAGE" > /dev/null; then break; fi
  sleep 5
  elapsed=$((elapsed + 5))
done
sleep 10  # a few more seconds of frames

adb logcat -d -s Bird:V AndroidRuntime:E DEBUG:V libc:F > logcat.txt
adb exec-out screencap -p > screenshot.png || true
cat logcat.txt
# Inline copy of the screenshot, for reviewers who cannot fetch artifacts.
if [ -s screenshot.png ]; then
  echo "SCREENSHOT_PNG_BASE64 $(base64 -w 0 screenshot.png)"
fi

status=0
grep -q "GL context: OpenGL ES" logcat.txt || { echo "::error::No GL context line"; status=1; }
grep -q "FPS:" logcat.txt || { echo "::error::Engine never reached its frame loop"; status=1; }
if grep -qE "Fatal:|FATAL EXCEPTION|Fatal signal|SIGSEGV|SIGABRT" logcat.txt; then
  echo "::error::The app crashed"; status=1
fi
adb shell pidof "$PACKAGE" > /dev/null || { echo "::error::The app is not running"; status=1; }
exit "$status"
