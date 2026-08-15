#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
#
# One-command Linux product builder/launcher/packager for FV-1 Lab.
#
# Examples:
#   ./linux.sh deps
#   ./linux.sh run
#   ./linux.sh test
#   ./linux.sh package all
#   ./linux.sh package deb
#   ./linux.sh package appimage
#   ./linux.sh package flatpak
#
# Generated packages are written to ./dist/.

set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${FV1_LINUX_BUILD_DIR:-${ROOT_DIR}/build-linux-product}"
PACKAGE_WORK_DIR="${FV1_LINUX_PACKAGE_WORK_DIR:-${ROOT_DIR}/build-linux-packaging}"
DIST_DIR="${FV1_DIST_DIR:-${ROOT_DIR}/dist}"
APP_ID="com.rothamplification.FV1Lab"
FLATPAK_RUNTIME_VERSION="6.10"
LINUXDEPLOY_VERSION="1-alpha-20251107-1"
LINUXDEPLOY_QT_VERSION="1-alpha-20250213-1"

VERSION_BASE="$(sed -n 's/^project(SpinFV1Emulator VERSION \([^ ]*\).*/\1/p' "${ROOT_DIR}/CMakeLists.txt" | head -n1)"
RELEASE_CHANNEL="$(sed -n 's/^set(FV1_RELEASE_CHANNEL "\([^"]*\)".*/\1/p' "${ROOT_DIR}/CMakeLists.txt" | head -n1)"
if [[ -z "${VERSION_BASE}" ]]; then
    echo "error: unable to determine project version from CMakeLists.txt" >&2
    exit 1
fi
if [[ -n "${RELEASE_CHANNEL}" ]]; then
    VERSION="${VERSION_BASE}-${RELEASE_CHANNEL}"
else
    VERSION="${VERSION_BASE}"
fi

HOST_ARCH="$(uname -m)"
case "${HOST_ARCH}" in
    x86_64)
        APPIMAGE_ARCH="x86_64"
        DEB_ARCH="amd64"
        ;;
    aarch64|arm64)
        APPIMAGE_ARCH="aarch64"
        DEB_ARCH="arm64"
        ;;
    i386|i486|i586|i686)
        APPIMAGE_ARCH="i386"
        DEB_ARCH="i386"
        ;;
    armv7l|armhf)
        APPIMAGE_ARCH="armhf"
        DEB_ARCH="armhf"
        ;;
    *)
        echo "error: unsupported Linux architecture: ${HOST_ARCH}" >&2
        exit 1
        ;;
esac

# Debian sorts '~rc1' before the final release. Keep the human-facing project
# version unchanged everywhere else.
DEB_VERSION="${VERSION/-rc/~rc}"

log() {
    printf '\n=== %s ===\n' "$*"
}

have() {
    command -v "$1" >/dev/null 2>&1
}

usage() {
    cat <<'USAGE'
FV-1 Lab Linux product helper

Usage:
  ./linux.sh deps
  ./linux.sh build
  ./linux.sh run [application arguments...]
  ./linux.sh test
  ./linux.sh install
  ./linux.sh package {all|deb|appimage|flatpak}
  ./linux.sh flatpak-run [application arguments...]
  ./linux.sh clean
  ./linux.sh status

Commands:
  deps          Install/verify the Linux build and packaging prerequisites.
  build         Configure and build the native Linux FV-1 Lab product.
  run           Build if needed, then launch the native fv1-lab executable.
  test          Build and run the complete CTest suite.
  install       Install the native product under ~/.local and refresh desktop data.
  package all   Build .deb, AppImage and Flatpak bundles into ./dist/.
  package deb   Build the Debian/Ubuntu package.
  package appimage
                Build a self-contained Qt AppImage with linuxdeploy.
  package flatpak
                Build a local single-file Flatpak bundle.
  flatpak-run   Build/install the Flatpak for the current user and launch it.
  clean         Remove Linux product/package build directories and ./dist/.
  status        Show resolved version, architecture and output directories.

Environment overrides:
  FV1_LINUX_BUILD_DIR
  FV1_LINUX_PACKAGE_WORK_DIR
  FV1_DIST_DIR
USAGE
}

require_base_build_tools() {
    local missing=()
    for cmd in cmake ninja pkg-config qmake6; do
        have "${cmd}" || missing+=("${cmd}")
    done
    if ((${#missing[@]})); then
        echo "error: missing build tools: ${missing[*]}" >&2
        echo "Run: ./linux.sh deps" >&2
        exit 1
    fi
    if ! pkg-config --exists speexdsp 2>/dev/null; then
        echo "error: SpeexDSP development files are missing." >&2
        echo "Run: ./linux.sh deps" >&2
        exit 1
    fi
    if [[ ! -f /usr/include/miniaudio.h && ! -f "${ROOT_DIR}/third_party/miniaudio/miniaudio.h" ]]; then
        echo "error: miniaudio.h is missing." >&2
        echo "Run: ./linux.sh deps" >&2
        exit 1
    fi
}

configure_native() {
    require_base_build_tools
    mkdir -p "${BUILD_DIR}"
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DFV1_BUILD_GUI=ON \
        -DFV1_ENABLE_LIVE_AUDIO=ON \
        -DFV1_BUILD_TESTS=ON \
        -DFV1_BUILD_WINDOWS_FRONTEND=OFF \
        -DFV1_SDK_BUILD_SHARED=OFF
}

build_native() {
    log "Configuring Linux product"
    configure_native
    log "Building FV-1 Lab"
    cmake --build "${BUILD_DIR}" --parallel
    [[ -x "${BUILD_DIR}/fv1-lab" ]] || {
        echo "error: fv1-lab was not produced" >&2
        exit 1
    }
}

stage_runtime_tree() {
    local destination="$1"
    rm -rf "${destination}"
    mkdir -p "${destination}"
    DESTDIR="${destination}" cmake --install "${BUILD_DIR}" --prefix /usr

    # Product packages are runtime deliverables. The public SDK development
    # package remains available through the normal CMake install path, but it
    # does not need to inflate FV-1 Lab desktop bundles.
    rm -rf "${destination}/usr/include" "${destination}/usr/lib/cmake"
    find "${destination}/usr/lib" -maxdepth 1 -type f -name '*.a' -delete 2>/dev/null || true
}

install_deps() {
    log "Installing/verifying FV-1 Lab development prerequisites"
    "${ROOT_DIR}/bootstrap-dev.sh" --install-only

    if ! have apt-get; then
        echo "error: automatic packaging dependency installation currently targets apt-based Linux." >&2
        echo "Install dpkg-dev, flatpak, flatpak-builder, desktop-file-utils and curl manually." >&2
        exit 1
    fi

    local sudo_cmd=()
    if [[ ${EUID} -ne 0 ]]; then
        have sudo || { echo "error: sudo is required to install packages" >&2; exit 1; }
        sudo_cmd=(sudo)
    fi

    log "Installing Linux packaging tools"
    "${sudo_cmd[@]}" apt-get update
    "${sudo_cmd[@]}" apt-get install -y \
        dpkg-dev \
        flatpak \
        flatpak-builder \
        desktop-file-utils \
        libglib2.0-bin \
        curl \
        file

    if ! flatpak remote-list --user --columns=name 2>/dev/null | grep -qx flathub; then
        log "Adding Flathub user remote"
        flatpak remote-add --user --if-not-exists flathub \
            https://flathub.org/repo/flathub.flatpakrepo
    fi

    log "Installing Flatpak Qt ${FLATPAK_RUNTIME_VERSION} SDK/runtime"
    flatpak install --user -y flathub \
        "org.kde.Platform//${FLATPAK_RUNTIME_VERSION}" \
        "org.kde.Sdk//${FLATPAK_RUNTIME_VERSION}"

    log "Dependencies ready"
}

run_native() {
    build_native
    log "Launching native FV-1 Lab"
    exec "${BUILD_DIR}/fv1-lab" "$@"
}

run_tests() {
    build_native
    log "Running Linux regression suite"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
}

install_user() {
    build_native
    log "Installing FV-1 Lab under ${HOME}/.local"
    cmake --install "${BUILD_DIR}" --prefix "${HOME}/.local"
    if have update-desktop-database; then
        update-desktop-database "${HOME}/.local/share/applications" || true
    fi
    if have gtk-update-icon-cache && [[ -d "${HOME}/.local/share/icons/hicolor" ]]; then
        gtk-update-icon-cache -f -t "${HOME}/.local/share/icons/hicolor" || true
    fi
    cat <<EOF2
Installed:
  ${HOME}/.local/bin/fv1-lab

Launch with:
  ${HOME}/.local/bin/fv1-lab
EOF2
}

detect_deb_dependencies() {
    local pkgroot="$1"
    local scratch="${PACKAGE_WORK_DIR}/deb-shlibdeps"
    rm -rf "${scratch}"
    mkdir -p "${scratch}/debian"
    cat > "${scratch}/debian/control" <<'CONTROL'
Source: fv1-lab
Section: sound
Priority: optional
Maintainer: Roth Amplification LTD
Standards-Version: 4.6.2

Package: fv1-lab
Architecture: any
Description: FV-1 Lab
 Virtual Spin FV-1 DSP emulator and electronic testbench.
CONTROL

    local output
    if ! output="$(
        cd "${scratch}"
        dpkg-shlibdeps -O \
            "${pkgroot}/usr/bin/fv1-lab" \
            "${pkgroot}/usr/bin/fv1-cli" \
            "${pkgroot}/usr/bin/fv1-live"
    )"; then
        echo "error: dpkg-shlibdeps could not derive runtime dependencies." >&2
        echo "Build the .deb on an apt/dpkg-managed Ubuntu, Pop!_OS or Debian host with ./linux.sh deps." >&2
        exit 1
    fi

    local deps
    deps="$(sed -n 's/^shlibs:Depends=//p' <<<"${output}" | tail -n1)"
    if [[ -z "${deps}" ]]; then
        echo "error: dpkg-shlibdeps returned no runtime dependency set" >&2
        exit 1
    fi
    printf '%s' "${deps}"
}

package_deb() {
    have dpkg-deb || { echo "error: dpkg-deb missing; run ./linux.sh deps" >&2; exit 1; }
    have dpkg-shlibdeps || { echo "error: dpkg-shlibdeps missing; run ./linux.sh deps" >&2; exit 1; }
    build_native
    mkdir -p "${DIST_DIR}" "${PACKAGE_WORK_DIR}"

    local pkgroot="${PACKAGE_WORK_DIR}/deb-root"
    stage_runtime_tree "${pkgroot}"
    mkdir -p "${pkgroot}/DEBIAN"

    local deps
    deps="$(detect_deb_dependencies "${pkgroot}")"
    cat > "${pkgroot}/DEBIAN/control" <<EOF2
Package: fv1-lab
Version: ${DEB_VERSION}
Architecture: ${DEB_ARCH}
Maintainer: Roth Amplification LTD
Section: sound
Priority: optional
Depends: ${deps}
Recommends: qt6-wayland, pipewire-pulse | pulseaudio
Homepage: https://github.com/Roth-Amplification-Ltd/Spin-FV-1-Emulator
Description: Native Spin FV-1 DSP emulator and electronic testbench
 FV-1 Lab emulates the Spin Semiconductor FV-1 DSP and provides SpinASM
 loading/compilation, realtime audio processing, analysis, validation and
 virtual-chip inspection in a native Qt 6 Linux desktop application.
EOF2

    local outfile="${DIST_DIR}/fv1-lab_${DEB_VERSION}_${DEB_ARCH}.deb"
    rm -f "${outfile}"
    log "Building Debian package"
    dpkg-deb --root-owner-group --build "${pkgroot}" "${outfile}"
    log "Created ${outfile}"
    dpkg-deb --info "${outfile}" | sed -n '1,24p'
}

download_if_missing() {
    local url="$1"
    local output="$2"
    if [[ ! -f "${output}" ]]; then
        mkdir -p "$(dirname -- "${output}")"
        curl -fL --retry 3 --connect-timeout 20 "${url}" -o "${output}"
    fi
    chmod +x "${output}"
}

package_appimage() {
    have curl || { echo "error: curl missing; run ./linux.sh deps" >&2; exit 1; }
    build_native
    mkdir -p "${DIST_DIR}" "${PACKAGE_WORK_DIR}"

    local appdir="${PACKAGE_WORK_DIR}/FV1-Lab.AppDir"
    stage_runtime_tree "${appdir}"

    local tool_dir="${PACKAGE_WORK_DIR}/appimage-tools"
    local linuxdeploy="${tool_dir}/linuxdeploy-${APPIMAGE_ARCH}.AppImage"
    local qtplugin="${tool_dir}/linuxdeploy-plugin-qt-${APPIMAGE_ARCH}.AppImage"
    download_if_missing \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_VERSION}/linuxdeploy-${APPIMAGE_ARCH}.AppImage" \
        "${linuxdeploy}"
    download_if_missing \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${LINUXDEPLOY_QT_VERSION}/linuxdeploy-plugin-qt-${APPIMAGE_ARCH}.AppImage" \
        "${qtplugin}"

    local output_dir="${PACKAGE_WORK_DIR}/appimage-output"
    rm -rf "${output_dir}"
    mkdir -p "${output_dir}"

    local desktop_file="${appdir}/usr/share/applications/roth-fv1-emulator.desktop"
    local icon_file="${appdir}/usr/share/icons/hicolor/512x512/apps/roth-fv1-emulator.png"
    [[ -f "${desktop_file}" ]] || { echo "error: staged desktop file missing" >&2; exit 1; }
    [[ -f "${icon_file}" ]] || { echo "error: staged 512x512 icon missing" >&2; exit 1; }

    log "Bundling Qt and runtime libraries into AppImage"
    (
        cd "${output_dir}"
        APPIMAGE_EXTRACT_AND_RUN=1 \
        VERSION="${VERSION}" \
        QMAKE="$(command -v qmake6)" \
        "${linuxdeploy}" \
            --appdir "${appdir}" \
            --executable "${appdir}/usr/bin/fv1-lab" \
            --desktop-file "${desktop_file}" \
            --icon-file "${icon_file}" \
            --plugin qt \
            --output appimage
    )

    mapfile -t generated < <(find "${output_dir}" -maxdepth 1 -type f -name '*.AppImage' -print)
    if [[ ${#generated[@]} -ne 1 ]]; then
        echo "error: expected exactly one AppImage, found ${#generated[@]}" >&2
        printf '  %s\n' "${generated[@]:-}" >&2
        exit 1
    fi

    local outfile="${DIST_DIR}/FV1-Lab-${VERSION}-${APPIMAGE_ARCH}.AppImage"
    mv -f "${generated[0]}" "${outfile}"
    chmod +x "${outfile}"
    log "Created ${outfile}"
    file "${outfile}"
}

ensure_flatpak_prereqs() {
    have flatpak || { echo "error: flatpak missing; run ./linux.sh deps" >&2; exit 1; }
    have flatpak-builder || { echo "error: flatpak-builder missing; run ./linux.sh deps" >&2; exit 1; }
    if ! flatpak remote-list --user --columns=name 2>/dev/null | grep -qx flathub; then
        echo "error: Flathub user remote is missing; run ./linux.sh deps" >&2
        exit 1
    fi
}

prepare_flatpak_inputs() {
    # Never point a Flatpak type: dir source at the live repository while the
    # builder's own build/state/output directories are also inside that tree.
    # Create a clean snapshot containing the current tracked files plus any
    # untracked, non-ignored working-tree files. Modified tracked files are
    # copied from disk, so this packages exactly what the caller is testing.
    local source_stage="${PACKAGE_WORK_DIR}/flatpak-source"
    local manifest_template="${ROOT_DIR}/packaging/linux/${APP_ID}.yml"
    local generated_manifest="${PACKAGE_WORK_DIR}/${APP_ID}.generated.yml"

    rm -rf "${source_stage}"
    mkdir -p "${source_stage}"

    log "Staging clean Flatpak source snapshot"
    while IFS= read -r -d '' relpath; do
        [[ -e "${ROOT_DIR}/${relpath}" || -L "${ROOT_DIR}/${relpath}" ]] || continue
        mkdir -p "${source_stage}/$(dirname -- "${relpath}")"
        cp -a -- "${ROOT_DIR}/${relpath}" "${source_stage}/${relpath}"
    done < <(git -C "${ROOT_DIR}" ls-files -z --cached --others --exclude-standard)

    [[ -f "${source_stage}/CMakeLists.txt" ]] || {
        echo "error: Flatpak source snapshot is missing CMakeLists.txt" >&2
        exit 1
    }

    # The checked-in manifest is a template. Resolve the local source relative
    # to this generated manifest, which lives beside flatpak-source/.
    sed 's|path: __FV1_LOCAL_SOURCE__|path: flatpak-source|' \
        "${manifest_template}" > "${generated_manifest}"

    grep -q '^        path: flatpak-source$' "${generated_manifest}" || {
        echo "error: failed to resolve Flatpak local source placeholder" >&2
        exit 1
    }

}

flatpak_build_common() {
    local install_mode="$1"
    ensure_flatpak_prereqs
    mkdir -p "${DIST_DIR}" "${PACKAGE_WORK_DIR}"

    local builddir="${PACKAGE_WORK_DIR}/flatpak-build"
    local repo="${PACKAGE_WORK_DIR}/flatpak-repo"
    local manifest="${PACKAGE_WORK_DIR}/${APP_ID}.generated.yml"
    prepare_flatpak_inputs
    rm -rf "${builddir}" "${repo}"

    local args=(
        --force-clean
        --user
        --install-deps-from=flathub
        --default-branch=stable
        --state-dir="${PACKAGE_WORK_DIR}/flatpak-state"
        --repo="${repo}"
    )
    if [[ "${install_mode}" == "install" ]]; then
        args+=(--install)
    fi

    log "Building Flatpak (${APP_ID})"
    # Run directly, not through a pipeline/command substitution. With errexit
    # and pipefail enabled, a builder failure now stops here and preserves the
    # real diagnostic instead of cascading into a bogus invalid-repo error.
    flatpak-builder "${args[@]}" "${builddir}" "${manifest}"

    [[ -d "${repo}" ]] || {
        echo "error: flatpak-builder completed without creating ${repo}" >&2
        exit 1
    }
}

package_flatpak() {
    local repo="${PACKAGE_WORK_DIR}/flatpak-repo"
    flatpak_build_common bundle
    local outfile="${DIST_DIR}/FV1-Lab-${VERSION}-${HOST_ARCH}.flatpak"
    rm -f "${outfile}"
    log "Creating single-file Flatpak bundle"
    flatpak build-bundle \
        "${repo}" \
        "${outfile}" \
        "${APP_ID}" \
        stable \
        --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo
    log "Created ${outfile}"
}

run_flatpak() {
    flatpak_build_common install
    log "Launching Flatpak FV-1 Lab"
    exec flatpak run "${APP_ID}" "$@"
}

package_all() {
    package_deb
    package_appimage
    package_flatpak
    log "All Linux packages"
    find "${DIST_DIR}" -maxdepth 1 -type f \( -name '*.deb' -o -name '*.AppImage' -o -name '*.flatpak' \) \
        -printf '  %f\n' | sort
}

show_status() {
    cat <<EOF2
FV-1 Lab Linux product helper
  Version:          ${VERSION}
  Host architecture:${HOST_ARCH}
  Debian arch:      ${DEB_ARCH}
  AppImage arch:    ${APPIMAGE_ARCH}
  Build directory:  ${BUILD_DIR}
  Package work:     ${PACKAGE_WORK_DIR}
  Distribution dir: ${DIST_DIR}
  Flatpak runtime:  org.kde.Platform//${FLATPAK_RUNTIME_VERSION}
  Flatpak app ID:   ${APP_ID}
EOF2
}

clean_all() {
    log "Cleaning Linux product/package output"
    rm -rf "${BUILD_DIR}" "${PACKAGE_WORK_DIR}" "${DIST_DIR}"
}

command="${1:-}"
[[ -n "${command}" ]] || { usage; exit 2; }
shift || true

case "${command}" in
    deps)
        install_deps
        ;;
    build)
        build_native
        ;;
    run)
        run_native "$@"
        ;;
    test)
        run_tests
        ;;
    install)
        install_user
        ;;
    package)
        format="${1:-}"
        [[ -n "${format}" ]] || { echo "error: package requires all|deb|appimage|flatpak" >&2; exit 2; }
        case "${format}" in
            all) package_all ;;
            deb) package_deb ;;
            appimage) package_appimage ;;
            flatpak) package_flatpak ;;
            *) echo "error: unknown package format: ${format}" >&2; exit 2 ;;
        esac
        ;;
    flatpak-run)
        run_flatpak "$@"
        ;;
    clean)
        clean_all
        ;;
    status)
        show_status
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        echo "error: unknown command: ${command}" >&2
        usage >&2
        exit 2
        ;;
esac
