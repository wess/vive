#!/usr/bin/env sh
# Vive installer for macOS and Linux.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/wess/vive/main/install.sh | sh
#   curl -fsSL https://raw.githubusercontent.com/wess/vive/main/install.sh | sh -s -- --version v0.1.0
#   curl -fsSL https://raw.githubusercontent.com/wess/vive/main/install.sh | sh -s -- --setup
#
# Flags:
#   --version X     Install a specific release tag (default: latest)
#   --prefix DIR    Install prefix (default: /usr/local, falls back to $HOME/.local)
#   --setup         Run `vive init --global` after install
#   --no-deps       Skip system dependency install
#   --help          Show this help

set -eu

REPO="wess/vive"
VERSION=""
PREFIX=""
RUN_SETUP=0
INSTALL_DEPS=1

log()  { printf "\033[1;34m==>\033[0m %s\n" "$*"; }
warn() { printf "\033[1;33m!!!\033[0m %s\n" "$*" >&2; }
die()  { printf "\033[1;31mxxx\033[0m %s\n" "$*" >&2; exit 1; }

while [ $# -gt 0 ]; do
  case "$1" in
    --version)   VERSION="$2"; shift 2 ;;
    --version=*) VERSION="${1#*=}"; shift ;;
    --prefix)    PREFIX="$2"; shift 2 ;;
    --prefix=*)  PREFIX="${1#*=}"; shift ;;
    --setup)     RUN_SETUP=1; shift ;;
    --no-deps)   INSTALL_DEPS=0; shift ;;
    --help|-h)
      sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *) die "Unknown flag: $1" ;;
  esac
done

need() { command -v "$1" >/dev/null 2>&1 || die "Missing required tool: $1"; }
need curl
need tar
need uname

OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
  Darwin) OS_TAG="darwin" ;;
  Linux)  OS_TAG="linux" ;;
  *) die "Unsupported OS: $OS (vive supports macOS and Linux)" ;;
esac

case "$ARCH" in
  arm64|aarch64) ARCH_TAG="arm64" ;;
  x86_64|amd64)  ARCH_TAG="x86_64" ;;
  *) die "Unsupported arch: $ARCH" ;;
esac

TARGET="${OS_TAG}-${ARCH_TAG}"

install_deps() {
  [ "$INSTALL_DEPS" -eq 1 ] || return 0
  log "Installing system dependencies"
  if [ "$OS_TAG" = "darwin" ]; then
    if ! command -v brew >/dev/null 2>&1; then
      warn "Homebrew not found. Install it from https://brew.sh then rerun, or pass --no-deps."
      return 0
    fi
    brew list cjson   >/dev/null 2>&1 || brew install cjson
    brew list sqlite  >/dev/null 2>&1 || brew install sqlite
    brew list ncurses >/dev/null 2>&1 || brew install ncurses
  elif [ "$OS_TAG" = "linux" ]; then
    if command -v apt-get >/dev/null 2>&1; then
      SUDO=""
      [ "$(id -u)" -ne 0 ] && SUDO="sudo"
      $SUDO apt-get update -y
      $SUDO apt-get install -y libsqlite3-0 libcjson1 libncurses6 || \
        $SUDO apt-get install -y libsqlite3-0 libcjson1 libncurses5
    elif command -v dnf >/dev/null 2>&1; then
      sudo dnf install -y sqlite-libs libcjson ncurses-libs
    elif command -v pacman >/dev/null 2>&1; then
      sudo pacman -S --needed --noconfirm sqlite cjson ncurses
    elif command -v apk >/dev/null 2>&1; then
      sudo apk add --no-cache sqlite-libs cjson ncurses-libs
    else
      warn "Could not detect a supported package manager. Install sqlite3, cjson, and ncurses manually."
    fi
  fi
}

resolve_version() {
  if [ -z "$VERSION" ]; then
    log "Resolving latest release"
    VERSION="$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" \
      | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -n1)"
    [ -n "$VERSION" ] || die "Could not determine latest version"
  fi
  VERSION="${VERSION#v}"
}

pick_prefix() {
  if [ -n "$PREFIX" ]; then
    return
  fi
  if [ -w /usr/local/bin ] 2>/dev/null || [ "$(id -u)" -eq 0 ]; then
    PREFIX="/usr/local"
  else
    PREFIX="$HOME/.local"
  fi
}

download_and_install() {
  NAME="vive-${VERSION}-${TARGET}"
  URL="https://github.com/${REPO}/releases/download/v${VERSION}/${NAME}.tar.gz"
  SHA_URL="${URL}.sha256"
  TMP="$(mktemp -d)"
  trap 'rm -rf "$TMP"' EXIT

  log "Downloading ${URL}"
  curl -fsSL "$URL"     -o "${TMP}/${NAME}.tar.gz" || die "Download failed: ${URL}"
  curl -fsSL "$SHA_URL" -o "${TMP}/${NAME}.tar.gz.sha256" || warn "No checksum available, skipping verification"

  if [ -f "${TMP}/${NAME}.tar.gz.sha256" ]; then
    log "Verifying checksum"
    (cd "$TMP" && shasum -a 256 -c "${NAME}.tar.gz.sha256" >/dev/null) \
      || die "Checksum verification failed"
  fi

  tar -C "$TMP" -xzf "${TMP}/${NAME}.tar.gz"
  BIN_DIR="${PREFIX}/bin"
  mkdir -p "$BIN_DIR"
  install -m 0755 "${TMP}/${NAME}/vive" "${BIN_DIR}/vive"
  log "Installed vive to ${BIN_DIR}/vive"

  case ":$PATH:" in
    *":${BIN_DIR}:"*) ;;
    *) warn "${BIN_DIR} is not in PATH. Add it to your shell profile:"; printf '    export PATH="%s:$PATH"\n' "$BIN_DIR" ;;
  esac
}

run_setup() {
  [ "$RUN_SETUP" -eq 1 ] || return 0
  log "Running 'vive init --global'"
  "${PREFIX}/bin/vive" init --global || warn "Setup failed; run 'vive init' manually"
}

install_deps
resolve_version
pick_prefix
download_and_install
run_setup

log "Done. Run 'vive --help' to get started."
