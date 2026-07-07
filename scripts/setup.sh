#!/usr/bin/env bash
# One-command setup and build for Nova Studio on Linux/macOS.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

SKIP_BUILD=0
RUN=0
SKIP_TESTS=0

usage() {
    cat <<'EOF'
Usage: ./scripts/setup.sh [options]

Options:
  --skip-build   Install dependencies and configure only
  --skip-tests   Skip ctest after building
  --run          Launch nova_studio when the build succeeds
  -h, --help     Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build) SKIP_BUILD=1 ;;
        --skip-tests) SKIP_TESTS=1 ;;
        --run) RUN=1 ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
    shift
done

step() {
    printf '\n==> %s\n' "$1"
}

install_linux_deps() {
    step "Installing Linux dependencies (apt)..."
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        pkg-config \
        qt6-base-dev \
        qt6-multimedia-dev \
        libgl1-mesa-dev \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
        libswresample-dev
}

install_macos_deps() {
    step "Installing macOS dependencies (Homebrew)..."
    if ! command -v brew >/dev/null 2>&1; then
        echo "Homebrew is required. Install it from https://brew.sh and re-run." >&2
        exit 1
    fi
    brew install cmake qt ffmpeg pkg-config
}

configure() {
    if [[ "$(uname -s)" == "Darwin" ]]; then
        step "Configuring CMake (macOS preset)..."
        cmake --preset macos
    else
        step "Configuring CMake (Linux preset)..."
        cmake --preset linux
    fi
}

build() {
    step "Building Nova Studio..."
    if [[ "$(uname -s)" == "Darwin" ]]; then
        cmake --build --preset macos -j"$(sysctl -n hw.ncpu)"
    else
        cmake --build --preset linux -j"$(nproc)"
    fi
}

test_build() {
    step "Running tests..."
    if [[ "$(uname -s)" == "Darwin" ]]; then
        ctest --preset macos
    else
        ctest --preset linux
    fi
}

run_app() {
    if [[ "$(uname -s)" == "Darwin" ]]; then
        open "$ROOT/build/nova_studio.app"
    else
        "$ROOT/build/nova_studio"
    fi
}

cat <<'EOF'

 Nova Studio — setup
 -------------------
 Installs dependencies, configures, builds, and optionally runs tests.

EOF

if [[ "$(uname -s)" == "Darwin" ]]; then
    install_macos_deps
elif [[ -f /etc/debian_version ]] || command -v apt-get >/dev/null 2>&1; then
    install_linux_deps
else
    echo "Unsupported Linux distro. Install the packages listed in docs/BUILDING.md, then re-run." >&2
    exit 1
fi

configure

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    build

    if [[ "$SKIP_TESTS" -eq 0 ]]; then
        test_build
    fi

    echo
    echo "Done! Run Nova Studio with:"
    if [[ "$(uname -s)" == "Darwin" ]]; then
        echo "  open build/nova_studio.app"
    else
        echo "  ./build/nova_studio"
    fi
    echo

    if [[ "$RUN" -eq 1 ]]; then
        run_app
    fi
fi
