#!/usr/bin/env bash
# Copyright 2026 the openage authors. See copying.md for legal info.
#
# Build a relocatable macOS release tree from an in-tree `bin/` build.
#
# Copies the build output, bundles non-system dylibs next to the binaries,
# rewrites install names to @loader_path / @rpath, ad-hoc codesigns, and
# emits a tar.gz (and optional DMG).
#
# Usage:
#   packaging/macos/package_portable.sh \
#     --source-dir bin \
#     --artifact-name openage-0.6.0-macos-arm64 \
#     --arch arm64 \
#     [--dmg]
#
set -euo pipefail

SOURCE_DIR=""
ARTIFACT_NAME=""
ARCH=""
MAKE_DMG=0
OUT_DIR="dist"

usage() {
	sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
	exit 1
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--source-dir) SOURCE_DIR="$2"; shift 2 ;;
		--artifact-name) ARTIFACT_NAME="$2"; shift 2 ;;
		--arch) ARCH="$2"; shift 2 ;;
		--out-dir) OUT_DIR="$2"; shift 2 ;;
		--dmg) MAKE_DMG=1; shift ;;
		-h|--help) usage ;;
		*) echo "unknown argument: $1" >&2; usage ;;
	esac
done

if [[ -z "${SOURCE_DIR}" || -z "${ARTIFACT_NAME}" || -z "${ARCH}" ]]; then
	echo "error: --source-dir, --artifact-name, and --arch are required" >&2
	usage
fi

if [[ ! -d "${SOURCE_DIR}" ]]; then
	echo "error: source directory not found: ${SOURCE_DIR}" >&2
	exit 1
fi

if [[ ! -x "${SOURCE_DIR}/run" ]]; then
	echo "error: expected executable ${SOURCE_DIR}/run" >&2
	exit 1
fi

STAGE="${OUT_DIR}/${ARTIFACT_NAME}"
rm -rf "${STAGE}"
mkdir -p "${STAGE}/lib"

echo "==> Staging build tree into ${STAGE}"
# Follow the bin symlink and copy the concrete build contents.
cp -aR "${SOURCE_DIR}/." "${STAGE}/"

# ---------------------------------------------------------------------------
# Collect non-system shared libraries referenced by Mach-O binaries.
# ---------------------------------------------------------------------------
is_system_lib() {
	local path="$1"
	case "${path}" in
		/System/*|/usr/lib/*|/usr/lib/system/*) return 0 ;;
		*) return 1 ;;
	esac
}

collect_deps() {
	local binary="$1"
	local deps
	# otool -L prints the binary itself on the first line; skip it.
	deps="$(otool -L "${binary}" 2>/dev/null | awk 'NR>1 {print $1}' || true)"
	local dep
	for dep in ${deps}; do
		if is_system_lib "${dep}"; then
			continue
		fi
		if [[ "${dep}" == @* ]]; then
			continue
		fi
		if [[ ! -f "${dep}" ]]; then
			continue
		fi
		local base
		base="$(basename "${dep}")"
		if [[ ! -f "${STAGE}/lib/${base}" ]]; then
			echo "    bundling ${dep}"
			cp -f "${dep}" "${STAGE}/lib/${base}"
			# Resolve symlinks so install_name_tool can rewrite the real file.
			if [[ -L "${STAGE}/lib/${base}" ]]; then
				local real
				real="$(cd "${STAGE}/lib" && readlink "${base}" || true)"
				if [[ -n "${real}" && -f "${STAGE}/lib/${real}" ]]; then
					:
				else
					cp -fL "${dep}" "${STAGE}/lib/${base}"
				fi
			fi
			collect_deps "${STAGE}/lib/${base}"
		fi
	done
}

echo "==> Bundling shared libraries"
# Prefer Mach-O files under the staged tree (dylibs, the run launcher, cython .so).
while IFS= read -r -d '' macho; do
	collect_deps "${macho}"
done < <(find "${STAGE}" -type f \( -name '*.dylib' -o -name '*.so' -o -name 'run' \) -print0)

# ---------------------------------------------------------------------------
# Rewrite install names so binaries load bundled libs via @loader_path.
# ---------------------------------------------------------------------------
echo "==> Rewriting install names"
rewrite_binary() {
	local binary="$1"
	local deps
	deps="$(otool -L "${binary}" 2>/dev/null | awk 'NR>1 {print $1}' || true)"
	local dep
	for dep in ${deps}; do
		local base
		base="$(basename "${dep}")"
		if [[ -f "${STAGE}/lib/${base}" ]]; then
			# Binaries in subdirs need a relative @loader_path depth to lib/.
			local rel
			rel="$(python3 - "${binary}" "${STAGE}" <<'PY'
import os, sys
binary = sys.argv[1]
stage = sys.argv[2]
rel_dir = os.path.relpath(os.path.join(stage, "lib"), os.path.dirname(binary))
print(rel_dir)
PY
)"
			install_name_tool -change "${dep}" "@loader_path/${rel}/${base}" "${binary}" 2>/dev/null || true
		fi
	done
	# Give bundled dylibs an identity matching their file name.
	if [[ "${binary}" == *.dylib ]]; then
		install_name_tool -id "@rpath/$(basename "${binary}")" "${binary}" 2>/dev/null || true
	fi
}

while IFS= read -r -d '' macho; do
	rewrite_binary "${macho}"
done < <(find "${STAGE}" -type f \( -name '*.dylib' -o -name '*.so' -o -name 'run' \) -print0)

# ---------------------------------------------------------------------------
# Optional Qt plugin / framework deployment via macdeployqt.
# ---------------------------------------------------------------------------
if command -v macdeployqt >/dev/null 2>&1; then
	echo "==> Running macdeployqt for Qt plugins"
	# Point macdeployqt at libopenage when present; ignore failures for
	# non-app-bundle layouts (openage uses a plain `run` launcher).
	LIBOPENAGE="$(find "${STAGE}" -name 'libopenage*.dylib' | head -n1 || true)"
	if [[ -n "${LIBOPENAGE}" ]]; then
		macdeployqt "${LIBOPENAGE}" -libpath="${STAGE}/lib" -always-overwrite || true
	fi
elif [[ -x "$(brew --prefix qt6 2>/dev/null)/bin/macdeployqt" ]]; then
	echo "==> Running Homebrew macdeployqt for Qt plugins"
	LIBOPENAGE="$(find "${STAGE}" -name 'libopenage*.dylib' | head -n1 || true)"
	if [[ -n "${LIBOPENAGE}" ]]; then
		"$(brew --prefix qt6)/bin/macdeployqt" "${LIBOPENAGE}" \
			-libpath="${STAGE}/lib" -always-overwrite || true
	fi
else
	echo "==> macdeployqt not found; Qt plugins stay linked to Homebrew"
fi

# ---------------------------------------------------------------------------
# Architecture smoke check
# ---------------------------------------------------------------------------
echo "==> Verifying architecture (${ARCH})"
CHECK_TARGET="$(find "${STAGE}" -name 'libopenage*.dylib' | head -n1 || true)"
if [[ -z "${CHECK_TARGET}" ]]; then
	CHECK_TARGET="${STAGE}/run"
fi
file "${CHECK_TARGET}"
ARCHS="$(lipo -archs "${CHECK_TARGET}" 2>/dev/null || true)"
if [[ -n "${ARCHS}" ]]; then
	echo "    lipo archs: ${ARCHS}"
	if [[ "${ARCH}" == "universal2" ]]; then
		echo "${ARCHS}" | grep -q 'arm64' && echo "${ARCHS}" | grep -q 'x86_64'
	else
		echo "${ARCHS}" | grep -qw "${ARCH}"
	fi
fi

# ---------------------------------------------------------------------------
# Ad-hoc codesign (required on Apple Silicon for unsigned relocated dylibs)
# ---------------------------------------------------------------------------
echo "==> Ad-hoc codesigning"
if command -v codesign >/dev/null 2>&1; then
	# Sign deepest first so nested libraries are valid before parents.
	while IFS= read -r -d '' macho; do
		codesign --force --sign - --timestamp=none "${macho}" 2>/dev/null || true
	done < <(find "${STAGE}" -type f \( -name '*.dylib' -o -name '*.so' \) -print0)
	codesign --force --sign - --timestamp=none "${STAGE}/run" 2>/dev/null || true
else
	echo "    codesign not available; skipping"
fi

# ---------------------------------------------------------------------------
# Archive outputs
# ---------------------------------------------------------------------------
mkdir -p "${OUT_DIR}"
TARBALL="${OUT_DIR}/${ARTIFACT_NAME}.tar.gz"
echo "==> Creating ${TARBALL}"
tar -czf "${TARBALL}" -C "${OUT_DIR}" "${ARTIFACT_NAME}"
test -s "${TARBALL}"

if [[ "${MAKE_DMG}" -eq 1 ]]; then
	DMG="${OUT_DIR}/${ARTIFACT_NAME}.dmg"
	echo "==> Creating ${DMG}"
	if command -v hdiutil >/dev/null 2>&1; then
		rm -f "${DMG}"
		hdiutil create -volname "${ARTIFACT_NAME}" -srcfolder "${STAGE}" \
			-ov -format UDZO "${DMG}"
		test -s "${DMG}"
	else
		echo "    hdiutil not available; skipping DMG"
	fi
fi

echo "ARTIFACT_FILE=${TARBALL}"
if [[ "${MAKE_DMG}" -eq 1 && -f "${OUT_DIR}/${ARTIFACT_NAME}.dmg" ]]; then
	echo "DMG_FILE=${OUT_DIR}/${ARTIFACT_NAME}.dmg"
fi
echo "==> Portable package ready: ${ARTIFACT_NAME}"
