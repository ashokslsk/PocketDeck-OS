#!/bin/bash
# usage: simrun.sh <name> <input-script> <screenshot-schedule-with-NAME-placeholders>
SP=/private/tmp/claude-501/-Users-ashok-Library-Application-Support-Claude-scratch-workspaces-9fbb4d3d-2663-4dcd-b22b-654bf7582786-e533bd95-93c8-45a5-94c8-6fab1c9482d0-scratch-2026-09-26-a24ec2/edfb3f92-85f7-4810-8e22-67e88b4e7394/scratchpad
SRC="$(cd "$(dirname "$0")/pocketdeck-os-main" && pwd)"
RUN=$SP/simrun/$1
if [ -n "$REUSE_FS" ]; then
  # Continue from another run's SD card (library, stats, settings persist).
  rm -rf "$RUN"; mkdir -p "$RUN/shots"; cp -R "$SP/simrun/$REUSE_FS/fs_" "$RUN/fs_"
else
  rm -rf "$RUN"; mkdir -p "$RUN/fs_" "$RUN/shots"
  cp -R "$SRC/sd-sample/tools" "$RUN/fs_/tools"
fi
[ -n "$PREP" ] && (cd "$RUN/fs_" && eval "$PREP")
true
cd "$RUN" && SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software CROSSPOINT_SIM_INPUT_SCRIPT="$2" CROSSPOINT_SIM_SCREENSHOTS="${3//SHOTS/$RUN/shots}" CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_WAKE="700:QUIT" \
  perl -e "alarm 150; exec @ARGV" "$SP/build/ci/.pio/build/${SIM_ENV:-simulator}/program" > "$RUN/log.txt" 2>&1
echo "exit $?"; ls "$RUN/shots"
