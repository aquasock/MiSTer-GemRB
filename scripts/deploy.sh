#!/usr/bin/env bash
# Copies the bundle (and, once, the game data) to the MiSTer over ssh.
# Usage: MISTER_HOST=<ip> scripts/deploy.sh [code|data|all]   (default: code)
#   BG2_DATA: the extracted game (default work/bg2-extract/app)
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

HOST="${MISTER_HOST:?set MISTER_HOST to the IP address of the MiSTer}"
DEVICE_DIR="${DEVICE_DIR:-/media/fat/gemrb}"
BG2_DATA="${BG2_DATA:-$WORK/bg2-extract/app}"
BUNDLE="$WORK/bundle"

# The MiSTer's stock login is root/1; feed it to ssh without sshpass.
ASKPASS="$WORK/askpass.sh"
printf '#!/bin/sh\necho 1\n' > "$ASKPASS"; chmod +x "$ASKPASS"
export SSH_ASKPASS="$ASKPASS" SSH_ASKPASS_REQUIRE=force
SSH=(ssh -o StrictHostKeyChecking=accept-new -o PubkeyAuthentication=no "root@$HOST")

code() {
	# Replaces the program, libraries and scripts; keeps game/, save/, cache/ and any env.sh or edited GemRB.cfg.
	"${SSH[@]}" "mkdir -p $DEVICE_DIR && cd $DEVICE_DIR && rm -rf gemrb libgemrb_core.so* plugins GUIScripts unhardcoded override libs python run.sh"
	tar -C "$BUNDLE/gemrb" -cf - --exclude=./GemRB.cfg . | "${SSH[@]}" "tar --no-same-owner -C $DEVICE_DIR -xf -"
	"${SSH[@]}" "[ -e $DEVICE_DIR/GemRB.cfg ] || cat > $DEVICE_DIR/GemRB.cfg" < "$BUNDLE/gemrb/GemRB.cfg"
	for f in "$BUNDLE"/Scripts/*.sh; do
		"${SSH[@]}" "mkdir -p /media/fat/Scripts && cat > /media/fat/Scripts/$(basename "$f") && chmod +x /media/fat/Scripts/$(basename "$f")" < "$f"
	done
}

data() {
	# Only what the engine reads; the installer's programs, manuals and helpers are left out.
	tar -C "$BG2_DATA" -cf - --exclude='*.exe' --exclude='*.dll' --exclude='*.pdf' --exclude='*.ico' --exclude='*.doc' \
		--exclude='ddrawfix' --exclude='script compiler' --exclude='mplayer' --exclude='cache' --exclude='*.cmd' . \
		| "${SSH[@]}" "mkdir -p $DEVICE_DIR/game && tar --no-same-owner -C $DEVICE_DIR/game -xf -"
}

case "${1:-code}" in
	code) code ;;
	data) data ;;
	all) code; data ;;
esac
