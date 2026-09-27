#!/bin/bash
D="$(cd "$(dirname "$0")" && pwd)"
SP=/private/tmp/claude-501/-Users-ashok-Library-Application-Support-Claude-scratch-workspaces-9fbb4d3d-2663-4dcd-b22b-654bf7582786-e533bd95-93c8-45a5-94c8-6fab1c9482d0-scratch-2026-09-26-a24ec2/edfb3f92-85f7-4810-8e22-67e88b4e7394/scratchpad
B=$SP/build/ci/.pio/build
: > "$D/final.status"; mkdir -p "$D/pocketdeck-os-main/dist"
"$D/build.sh" default > "$D/final-default.log" 2>&1; echo "build default EXIT $?" >> "$D/final.status"; cp $B/default/firmware-x3-x4.bin "$D/pocketdeck-os-main/dist/pocketdeck-os-x3.bin"
"$D/build.sh" sticky > "$D/final-sticky.log" 2>&1; echo "build sticky EXIT $?" >> "$D/final.status"; cp $B/sticky/firmware*.bin "$D/sticky-out/" 2>/dev/null; ls $B/sticky/*.bin >> "$D/final.status"
cp "$(ls -t $B/sticky/firmware-*.bin | head -1)" "$D/pocketdeck-os-main/dist/pocketdeck-os-sticky.bin"
"$D/build.sh" x4-pro > "$D/final-x4-pro.log" 2>&1; echo "build x4-pro EXIT $?" >> "$D/final.status"; ls $B/x4-pro/*.bin >> "$D/final.status"
cp "$(ls -t $B/x4-pro/firmware-*.bin | head -1)" "$D/pocketdeck-os-main/dist/pocketdeck-os-x4-pro.bin"
"$D/build.sh" simulator > "$D/final-simulator.log" 2>&1; echo "build simulator EXIT $?" >> "$D/final.status"
CM=~/.platformio/packages/tool-cmake/bin
$CM/cmake --build $SP/tests --target ToolsCoreTest -j 8 > "$D/final-tests-build.log" 2>&1 && $SP/tests/tools_core/ToolsCoreTest > "$D/final-tests.log" 2>&1; echo "unit tests EXIT $?" >> "$D/final.status"
for theme in classic lyra lyra-extended roundedraff lyra-carousel dashboard pocketdeck; do
  (cd $SP/build/ci && SDL_RENDER_DRIVER=software python3 scripts/run_simulator_smoke_test.py --no-build --theme $theme > "$D/final-smoke-$theme.log" 2>&1); echo "smoke $theme EXIT $? tools=$(grep -c 'Rendering Tools' "$D/final-smoke-$theme.log")" >> "$D/final.status"
done
echo DONE >> "$D/final.status"
