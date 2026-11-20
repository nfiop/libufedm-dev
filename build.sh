
#!/usr/bin/env bash
set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"

ZIG_VERSION="0.17.1"
TARGET="${1:-}"
ARCH="${ARCH:-x86_64}"

BUILD_DIR="$PROJECT_DIR/build/$TARGET"
TOOLCHAIN_DIR="$BUILD_DIR/toolchain"

# Validate target
case "$TARGET" in
    linux)
        CMAKE_SYSTEM="Linux"
        ZIG_TARGET="$ARCH-linux-gnu"
        ;;
    win32)
        CMAKE_SYSTEM="Windows"
        ZIG_TARGET="$ARCH-windows-gnu"
        ;;
    macos)
        CMAKE_SYSTEM="Darwin"
        ZIG_TARGET="$ARCH-macos"
        ;;
    *)
        echo "Usage: $0 <linux|win32|macos>"
        exit 1
        ;;
esac

# Detect host platform (the platform running this script)
HOST_OS="$(uname -s)"
HOST_ARCH="$(uname -m)"

case "$HOST_OS" in
    Linux)
        ZIG_HOST_OS="linux"
        ;;
    Darwin)
        ZIG_HOST_OS="macos"
        ;;
    *)
        echo "Unsupported build host: $HOST_OS"
        exit 1
        ;;
esac

case "$HOST_ARCH" in
    x86_64|amd64)
        ZIG_HOST_ARCH="x86_64"
        ;;
    aarch64|arm64)
        ZIG_HOST_ARCH="aarch64"
        ;;
    *)
        echo "Unsupported host architecture: $HOST_ARCH"
        exit 1
        ;;
esac

# Download Zig
ZIG_ARCHIVE="zig-${ZIG_HOST_ARCH}-${ZIG_HOST_OS}-${ZIG_VERSION}.tar.xz"
ZIG_URL="https://ziglang.org/download/${ZIG_VERSION}/${ZIG_ARCHIVE}"

mkdir -p "$TOOLCHAIN_DIR"

ZIG="$TOOLCHAIN_DIR/zig"

if [[ ! -x "$ZIG" ]]; then
    echo "Downloading Zig $ZIG_VERSION..."

    TMP_DIR="$(mktemp -d)"
    trap 'rm -rf "$TMP_DIR"' EXIT

    curl -fL "$ZIG_URL" -o "$TMP_DIR/$ZIG_ARCHIVE"

    mkdir -p "$TOOLCHAIN_DIR"
    tar -xf "$TMP_DIR/$ZIG_ARCHIVE" \
        -C "$TMP_DIR" \
        --strip-components=1

    cp "$TMP_DIR/zig" "$ZIG"
    cp -r "$TMP_DIR/lib" "$TOOLCHAIN_DIR/"
    cp -r "$TMP_DIR/doc" "$TOOLCHAIN_DIR/" 2>/dev/null || true
fi

echo "Using Zig:"
"$ZIG" version

# Create compiler wrappers
CC="$TOOLCHAIN_DIR/zig-cc"
CXX="$TOOLCHAIN_DIR/zig-cxx"

cat > "$CC" <<EOF
#!/usr/bin/env bash
exec "$ZIG" cc -target "$ZIG_TARGET" "\$@"
EOF

cat > "$CXX" <<EOF
#!/usr/bin/env bash
exec "$ZIG" c++ -target "$ZIG_TARGET" "\$@"
EOF

chmod +x "$CC" "$CXX"

# Configure
echo "================================"
echo "Target:    $TARGET"
echo "Triple:    $ZIG_TARGET"
echo "Compiler:  $ZIG"
echo "Build dir: $BUILD_DIR"
echo "================================"

cmake \
    -S "$PROJECT_DIR" \
    -B "$BUILD_DIR" \
    -DCMAKE_SYSTEM_NAME="$CMAKE_SYSTEM" \
    -DCMAKE_SYSTEM_PROCESSOR="$ARCH" \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_CXX_COMPILER="$CXX" \
    -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build "$BUILD_DIR" \
    --config Release \
    --parallel

echo "================================"
echo "Build completed successfully!"
echo "Output directory: $BUILD_DIR"
echo "================================"
