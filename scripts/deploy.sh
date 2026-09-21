#!/usr/bin/env bash
# Copies the bundle (and, once per game, the game data) to the MiSTer over ssh.
# Usage: MISTER_HOST=<ip> scripts/deploy.sh code
#        MISTER_HOST=<ip> scripts/deploy.sh data <game>       game: bg2, bg1, pst, iwd, iwd2
#        MISTER_HOST=<ip> scripts/deploy.sh debug              unstripped files for MISTER_DEBUG=1 (run debug-symbols.sh first)
#   GAME_DATA: the extracted game (default work/<game>-extract/app)
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

HOST="${MISTER_HOST:?set MISTER_HOST to the IP address of the MiSTer}"
DEVICE_DIR="${DEVICE_DIR:-/media/fat/gemrb}"
BUNDLE="$WORK/bundle"

# The MiSTer's stock login is root/1; feed it to ssh without sshpass.
ASKPASS="$WORK/askpass.sh"
printf '#!/bin/sh\necho 1\n' > "$ASKPASS"; chmod +x "$ASKPASS"
export SSH_ASKPASS="$ASKPASS" SSH_ASKPASS_REQUIRE=force
SSH=(ssh -o StrictHostKeyChecking=accept-new -o PubkeyAuthentication=no "root@$HOST")

code() {
	# Replaces the program, libraries and scripts. Keeps games/, saves/, cache/, the swap file, env.sh and every
	# GemRB-<game>.cfg you may have edited (the bundle only carries GemRB-<game>.cfg.default).
	"${SSH[@]}" "mkdir -p $DEVICE_DIR && cd $DEVICE_DIR && rm -rf gemrb libgemrb_core.so* plugins GUIScripts unhardcoded override libs python run.sh GemRB-*.cfg.default GemRB.cfg.default"
	tar -C "$BUNDLE/gemrb" -cf - . | "${SSH[@]}" "tar --no-same-owner -C $DEVICE_DIR -xf -"
	"${SSH[@]}" "rm -f /media/fat/Scripts/gemrb.sh"     # the launcher's name before it was per game
	for f in "$BUNDLE"/Scripts/*.sh; do
		"${SSH[@]}" "mkdir -p /media/fat/Scripts && cat > /media/fat/Scripts/$(basename "$f") && chmod +x /media/fat/Scripts/$(basename "$f")" < "$f"
	done
}

data() {
	local game="${1:?usage: deploy.sh data <game>  (bg2, bg1, pst, iwd or iwd2)}"
	local src="${GAME_DATA:-$WORK/$game-extract/app}"
	# innoextract puts the game in app/ for most GOG installers, but directly in the folder for some (Icewind Dale 2, which
	# also has a small unrelated app/ next to it): use the folder that holds CHITIN.KEY.
	if [ -z "${GAME_DATA:-}" ] && [ -z "$(find "$src" -maxdepth 1 -iname chitin.key 2>/dev/null)" ] \
	   && [ -n "$(find "$WORK/$game-extract" -maxdepth 1 -iname chitin.key 2>/dev/null)" ]; then
		src="$WORK/$game-extract"
	fi
	[ -d "$src" ] || { echo "no game files in $src (set GAME_DATA)" >&2; exit 1; }
	# Only what the engine reads; the installer's programs, manuals and helpers, and any saves, are left out.
	tar -C "$src" -cf - --ignore-case --exclude='*.exe' --exclude='*.dll' --exclude='*.pdf' --exclude='*.ico' --exclude='*.doc' \
		--exclude='ddrawfix' --exclude='script compiler' --exclude='mplayer' --exclude='cache' --exclude='*.cmd' \
		--exclude='*.sdb' --exclude='__support' --exclude='__redist' --exclude='commonappdata' --exclude='./app' --exclude='temp' --exclude='Save' --exclude='MPSave' . \
		| "${SSH[@]}" "mkdir -p $DEVICE_DIR/games/$game && tar --no-same-owner -C $DEVICE_DIR/games/$game -xf -"
	# The GOG installer of Planescape: Torment keeps the game's settings and lists (Torment.ini, beast.ini, quests.ini, ...)
	# in __support/app and lets its own installer copy them next to the game; GemRB reads them from the game folder.
	if [ -d "$src/__support/app" ]; then
		(cd "$src/__support/app" && find . -maxdepth 1 -type f -iname '*.ini' -printf '%P\0' | tar --null -T - -cf -) \
			| "${SSH[@]}" "tar --no-same-owner -C $DEVICE_DIR/games/$game -xf -"
	fi
}

debug() {
	[ -d "$WORK/debugfiles/.build-id" ] || { echo "run scripts/debug-symbols.sh first" >&2; exit 1; }
	"${SSH[@]}" "rm -rf $DEVICE_DIR/debug && mkdir -p $DEVICE_DIR/debug"
	tar -C "$WORK/debugfiles" -cf - .build-id | "${SSH[@]}" "tar --no-same-owner -C $DEVICE_DIR/debug -xf -"
}

case "${1:-code}" in
	code) code ;;
	data) data "${2:-}" ;;
	debug) debug ;;
	*) echo "usage: deploy.sh code | data <game> | debug" >&2; exit 1 ;;
esac
