#!/bin/sh
set -u
. "$(dirname -- "$0")/session.sh"
# Start logging before anything can fail: a launcher that dies silently is the
# hardest failure to diagnose on this device.
mkdir -p "$APP/logs" "$STATE" "$APP/cache" "$APP/userdata" "$APP/storage/books" "$APP/storage/documents" "$APP/storage/screenshots" "$APP/userdata/.config" "$APP/userdata/.local/share" "$APP/userdata/.cache" 2>/dev/null
exec >> "$APP/logs/launcher.log" 2>&1
echo "[launcher] $(date)"
product=$(awk -F, '{print substr($NF,length($NF)-2)}' /mnt/onboard/.kobo/version 2>/dev/null)
echo "[launcher] product=$product"
# Snapshot the input tree once per launch. This is what tells us which node
# carries the grip keys, the power button and the gyro on this exact firmware.
{
    echo "[diag] $(date)"
    echo "[diag] fb0 rotate=$(cat /sys/class/graphics/fb0/rotate 2>/dev/null)"
    cat /proc/bus/input/devices 2>/dev/null
} > "$APP/logs/input-devices.log" 2>&1
case "$product" in 390|393) ;; *) echo 'Only Kobo Libra Colour (390) / Clara Colour (393) are supported; launch cancelled.'; exit 12;; esac
if [ -d "$STATE/active" ]; then
    if [ -f "$STATE/owner" ]; then read -r pid start < "$STATE/owner"; if same_process "$pid" "$start"; then echo "Already running"; exit 0; fi; fi
    "$APP/recover.sh"
fi
# Never take display ownership from KOReader. Return to Nickel first.
for pid in $(pidof luajit 2>/dev/null); do
    if tr '\000' ' ' < "/proc/$pid/cmdline" | grep -q reader.lua; then echo "Exit KOReader and launch from NickelMenu"; exit 10; fi
done
NICKEL=$(pidof nickel 2>/dev/null) || { echo "Nickel is not running; refusing to take ownership"; exit 11; }
chmod 755 "$APP"/*.sh "$APP"/bin/* "$APP"/runtime/* "$APP"/checks/* 2>/dev/null || true
if [ -f "$APP/SHA256SUMS" ]; then
    # Recheck on every launch: resources can disappear after first installation.
    if [ -f "$APP/integrity.sh" ]; then
        expected=$(awk '$2 == "integrity.sh" {print $1; n++} END {if (n != 1) exit 1}' "$APP/SHA256SUMS") || exit 12
        actual=$(sha256sum "$APP/integrity.sh") || exit 12
        actual=${actual%% *}
        [ "$actual" = "$expected" ] || { echo 'Integrity helper checksum failed; reinstall the package'; exit 12; }
        sh "$APP/integrity.sh" repair > "$APP/install-verification.log" 2>&1 || {
            echo 'Package checksum verification failed; see install-verification.log'
            grep -E 'FAILED|INTEGRITY|No such file|WARNING' "$APP/install-verification.log" | head -n 12
            exit 12
        }
    else
        (cd "$APP" && sha256sum -c SHA256SUMS > "$APP/install-verification.log" 2>&1) || { echo "Package checksum verification failed; see install-verification.log"; exit 12; }
    fi
    : > "$STATE/install-verified"
fi
# Libra Colour (390): upstream's display and touch calibration targets fb
# rotation 3 -- the value used by Clara Colour and by the first working Libra
# session. Nickel sometimes hands the framebuffer over at rotation 1, which is
# exactly 180 degrees off and mirrors both the picture and the taps.
if [ "$product" = 390 ]; then
    if ! "$APP/bin/fb-guard" --rotate 3; then
        echo 3 > /sys/class/graphics/fb0/rotate 2>/dev/null || true
        if ! "$APP/bin/fb-guard" --rotate 3; then
            # Last resort: at least keep taps aligned with the visible UI.
            export REEDEN_TOUCH_INVERT_X=1 REEDEN_TOUCH_INVERT_Y=1
            echo "[launcher] WARN: could not restore rotation 3; touch inverted 180 degrees as fallback"
        fi
    fi
fi
"$APP/bin/fb-guard" --inspect || { echo "Unsupported framebuffer configuration"; exit 12; }
"$APP/runtime/reeden-kindle" --verify "$APP/bin/reeden-kindle" || exit 13
mkdir "$STATE/active" || exit 14
echo "$$ $(token $$)" > "$STATE/owner"
rm -f "$STATE/host" "$STATE/heartbeat" "$STATE/ready" "$STATE/handoff" "$STATE/asleep" "$STATE/power-cancel" "$STATE/power-phase" "$STATE/power-phase.new"
: > "$STATE/paused"
if [ -f "$APP/logs/current.log" ]; then mv -f "$APP/logs/current.log" "$APP/logs/previous.log"; fi
: > "$APP/logs/current.log"
trap '"$APP/recover.sh"' EXIT
trap 'exit 143' TERM HUP INT
nohup "$APP/guardian.sh" > "$APP/logs/guardian.log" 2>&1 < /dev/null &
read -r light < /sys/class/backlight/mxc_msp430.0/brightness
read -r warm < /sys/class/backlight/lm3630a_led/color
echo "$light $warm" > "$STATE/light"
export REEDEN_KOBO_DIR="$APP"
export REEDEN_BUNDLE_DIR="$APP/core"
export REEDEN_ASSETS_DIR="$APP/ui-assets"
export HOME="$APP/userdata" XDG_CONFIG_HOME="$APP/userdata/.config" XDG_DATA_HOME="$APP/userdata/.local/share" XDG_CACHE_HOME="$APP/userdata/.cache"
export TMPDIR="$APP/cache" LANG=C.UTF-8 LC_ALL=C.UTF-8
export PATH="$APP/bin:/usr/local/Kobo:/usr/sbin:/usr/bin:/sbin:/bin"
export REEDEN_LOWMEM_SUPPORTED=0 REEDEN_LOWMEM_DEFAULT=0 REEDEN_LOWMEM_FILE="$APP/userdata/lowmem"
export FLUTTER_FONT_DIRECTORY="$APP/fonts/"
# A Kindle self-update must never replace the Kobo host. No auth checks altered.
export REEDEN_UPDATE_FEED=http://127.0.0.1:9/kobo-updates-disabled.json
export GIO_USE_VFS=local GSETTINGS_BACKEND=memory
export SSL_CERT_FILE="$APP/certs/ca-certificates.crt" SSL_CERT_DIR="$APP/certs"
# Optional features added for the Libra Colour. Both default to on and can be
# disabled here without reinstalling: set REEDEN_BUTTONS=0 for the grip
# page-turn keys, REEDEN_ROTATE_AUTO=0 for portrait auto-rotation.
: "${REEDEN_BUTTONS:=1}"
: "${REEDEN_ROTATE_AUTO:=1}"
export REEDEN_BUTTONS REEDEN_ROTATE_AUTO
# On the Libra Colour the gyro chip is mounted 180 degrees off relative to the
# panel: while the device is held upright the driver reports 0x17 (portrait
# down), so the unpatched mapping flips an already-upright picture. Invert the
# two portrait codes here. Delete this block on hardware where the sensor and
# the panel agree (the flag is presence-based: any value enables it).
if [ "$product" = 390 ]; then
    export REEDEN_ROTATE_FLIP=1
fi
unset LD_LIBRARY_PATH LD_PRELOAD LD_AUDIT
cd "$APP/core" || exit 18
"$APP/runtime/reeden-kindle" --library-path "$APP/runtime:$APP/lib:$APP/core/lib" --preload "$APP/lib/libreeden_kobo_compat.so" "$APP/bin/reeden-kindle" >> "$APP/logs/current.log" 2>&1 &
HOST=$!
echo "$HOST $(token "$HOST")" > "$STATE/host"
# Initialize the app while Nickel remains responsive. No framebuffer writes or
# input grabs occur until the engine has delivered an actual frame in memory.
attempt=0
while [ ! -f "$STATE/ready" ]; do
    owned_host || break
    [ -d "$STATE/active" ] && [ ! -d "$STATE/recovering" ] || exit 19
    if [ "$attempt" -ge 20 ]; then echo '[launcher] no first frame in 20s; Nickel was never paused'; exit 20; fi
    sleep 1; attempt=$((attempt+1))
done
if [ -f "$STATE/ready" ]; then
    [ -d "$STATE/active" ] && [ ! -d "$STATE/recovering" ] && [ "$(cat "$STATE/owner" 2>/dev/null)" = "$$ $(token $$)" ] || exit 19
    # KOReader or USB mode may have been entered during warmup. Recheck.
    for pid in $(pidof luajit 2>/dev/null); do
        if tr '\000' ' ' < "/proc/$pid/cmdline" | grep -q reader.lua; then echo 'KOReader entered during warmup; cancel'; exit 10; fi
    done
    pidof nickel >/dev/null 2>&1 || exit 11
    "$APP/native-session.sh" capture || exit 16
    "$APP/bin/fb-guard" --snapshot "$STATE/framebuffer" || exit 15
    "$APP/native-session.sh" stop || exit 17
    [ -d "$STATE/active" ] && [ ! -d "$STATE/recovering" ] && [ "$(cat "$STATE/owner" 2>/dev/null)" = "$$ $(token $$)" ] || exit 19
    : > "$STATE/handoff"
    echo '[launcher] first frame ready; hardware handoff authorized'
fi
wait "$HOST"
code=$?
echo "[launcher] host exit=$code"
exit "$code"
