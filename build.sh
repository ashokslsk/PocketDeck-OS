#!/bin/bash
# Sync sources into a space-free path and build (ESP-IDF rejects spaces in paths).
SP=/private/tmp/claude-501/-Users-ashok-Library-Application-Support-Claude-scratch-workspaces-9fbb4d3d-2663-4dcd-b22b-654bf7582786-e533bd95-93c8-45a5-94c8-6fab1c9482d0-scratch-2026-09-26-a24ec2/edfb3f92-85f7-4810-8e22-67e88b4e7394/scratchpad
SRC="$(cd "$(dirname "$0")/pocketdeck-os-main" && pwd)"
rsync -a --delete --exclude .pio --exclude .git --exclude managed_components --exclude dependencies.lock --exclude sdkconfig.* ${EXTRA_EXCLUDE:+--exclude "$EXTRA_EXCLUDE"} "$SRC/" "$SP/build/ci/"
cd "$SP/build/ci" && "$(dirname "$SRC")/pio-venv/bin/pio" run -e "${1:-default}"
