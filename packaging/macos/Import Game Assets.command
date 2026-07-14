#!/bin/bash
# Copyright 2026 the openage authors. See copying.md for legal info.
#
# Double-clickable macOS helper: Import Game Assets.command
#
# Opens Terminal, then either:
#   1. Converts from a folder dropped onto this script / passed as $1, or
#   2. Runs `./run convert --force --browse` so Finder can pick the install.
#
# Place this next to the `run` launcher (portable release root) or invoke
# from the repo root after `./configure && make`.

set -euo pipefail

cd "$(dirname "$0")"

# Portable release layout: script lives beside ./run
# Dev layout: script may live in packaging/macos/ — walk up to find ./run
find_run() {
	local dir="$1"
	local i
	for i in 1 2 3 4 5; do
		if [[ -x "${dir}/run" ]]; then
			echo "${dir}/run"
			return 0
		fi
		dir="$(cd "${dir}/.." && pwd)"
	done
	return 1
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RUN_BIN="$(find_run "${SCRIPT_DIR}" || true)"

if [[ -z "${RUN_BIN}" ]]; then
	osascript -e 'display alert "openage" message "Could not find the openage ./run launcher next to this script." as critical' || true
	echo "error: ./run not found relative to ${SCRIPT_DIR}" >&2
	exit 1
fi

ROOT="$(cd "$(dirname "${RUN_BIN}")" && pwd)"
cd "${ROOT}"

SOURCE_DIR="${1:-}"

echo "openage — Import Game Assets"
echo "Working directory: ${ROOT}"
echo

if [[ -n "${SOURCE_DIR}" ]]; then
	# Finder "Open With" / drop-on-script passes a folder path as $1.
	if [[ ! -d "${SOURCE_DIR}" ]]; then
		echo "error: not a directory: ${SOURCE_DIR}" >&2
		exit 1
	fi
	echo "Converting from: ${SOURCE_DIR}"
	exec ./run convert --force --no-prompts --source-dir "${SOURCE_DIR}"
fi

echo "No folder supplied — opening Finder to choose your game install…"
echo "(Steam DE is usually under ~/Library/Application Support/Steam/steamapps/common/AoE2DE)"
echo
exec ./run convert --force --browse
