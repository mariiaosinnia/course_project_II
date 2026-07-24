#!/bin/bash
set -euo pipefail

# ──────────────────────────────────────────────
#  StudyRoom — install script (macOS & Linux)
# ──────────────────────────────────────────────

BOLD="\033[1m"
DIM="\033[2m"
CYAN="\033[36m"
GREEN="\033[32m"
YELLOW="\033[33m"
RED="\033[31m"
RESET="\033[0m"

STEP=0
total_steps=4

step() {
    STEP=$((STEP + 1))
    echo -e "\n${CYAN}${BOLD}[$STEP/$total_steps]${RESET} $1"
}

ok()   { echo -e "  ${GREEN}✓${RESET}  $1"; }
info() { echo -e "  ${DIM}→${RESET}  $1"; }
warn() { echo -e "  ${YELLOW}⚠${RESET}  $1"; }
fail() { echo -e "\n  ${RED}✗  $1${RESET}\n"; exit 1; }

# ── Header ──────────────────────────────────────
clear
echo ""
echo -e "${CYAN}${BOLD}"
echo "   ╔══════════════════════════════════╗"
echo "   ║        S T U D Y R O O M        ║"
echo "   ║   synchronized listening space  ║"
echo "   ╚══════════════════════════════════╝"
echo -e "${RESET}"
echo -e "  ${DIM}Setting up your client...${RESET}"
echo ""

# ── Detect OS ───────────────────────────────────
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    info "Detected macOS"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    info "Detected Linux"
else
    fail "Unsupported OS: $OSTYPE"
fi

# ── Step 1: Check build tools ───────────────────
step "Checking build tools"

if ! command -v cmake &>/dev/null; then
    warn "cmake not found — installing..."
    if [[ "$OS" == "macos" ]]; then
        command -v brew &>/dev/null || fail "Homebrew not found. Install it from https://brew.sh and re-run."
        brew install cmake
    else
        sudo apt-get update -qq && sudo apt-get install -y cmake
    fi
fi
ok "cmake $(cmake --version | head -1 | awk '{print $3}')"

if ! command -v git &>/dev/null; then
    warn "git not found — installing..."
    if [[ "$OS" == "macos" ]]; then
        brew install git
    else
        sudo apt-get install -y git
    fi
fi
ok "git $(git --version | awk '{print $3}')"

# ── Step 2: Install audio & network deps ────────
step "Installing dependencies"

if [[ "$OS" == "macos" ]]; then
    for pkg in boost opus portaudio; do
        if brew list "$pkg" &>/dev/null; then
            ok "$pkg (already installed)"
        else
            info "Installing $pkg..."
            brew install "$pkg"
            ok "$pkg"
        fi
    done
else
    info "Updating package list..."
    sudo apt-get update -qq
    for pkg in libboost-all-dev libopus-dev portaudio19-dev ninja-build; do
        if dpkg -s "$pkg" &>/dev/null 2>&1; then
            ok "$pkg (already installed)"
        else
            info "Installing $pkg..."
            sudo apt-get install -y -qq "$pkg"
            ok "$pkg"
        fi
    done
fi

# ── Step 3: Build ────────────────────────────────
step "Building StudyRoom client"

PRESET="macos-clang"
if [[ "$OS" == "linux" ]]; then
    PRESET="linux-gcc"
fi

info "Configuring with CMake preset: $PRESET"
cmake --preset "$PRESET" -DBUILD_SERVER=OFF -DBUILD_TESTING=OFF -Wno-dev > /dev/null 2>&1 \
    || fail "CMake configuration failed. Check your CMakeLists.txt."

CPU_COUNT=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
info "Compiling (using $CPU_COUNT cores)..."
cmake --build "build/$PRESET" --target client -j"$CPU_COUNT" 2>&1 | tail -5 \
    || fail "Build failed. Run 'cmake --build build/$PRESET --target client' for details."

BINARY_PATH="build/$PRESET/client/client"
[[ -f "$BINARY_PATH" ]] || fail "Build succeeded but binary not found at $BINARY_PATH"
ok "Build complete"

# ── Step 4: Create launcher ──────────────────────
step "Creating launcher"

cat > studyroom << EOF
#!/bin/bash
# StudyRoom launcher
SCRIPT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
exec "\$SCRIPT_DIR/$BINARY_PATH" "\$@"
EOF
chmod +x studyroom
ok "Created ./studyroom"

# Add alias to shell config if not already there
ALIAS_LINE='alias studyroom="$(pwd)/studyroom"'
SHELL_RC=""
if [[ -f "$HOME/.zshrc" ]]; then
    SHELL_RC="$HOME/.zshrc"
elif [[ -f "$HOME/.bashrc" ]]; then
    SHELL_RC="$HOME/.bashrc"
fi

if [[ -n "$SHELL_RC" ]]; then
    FULL_PATH="$(pwd)/studyroom"
    ALIAS_ENTRY="alias studyroom='$FULL_PATH'"
    if ! grep -q "alias studyroom=" "$SHELL_RC" 2>/dev/null; then
        echo "" >> "$SHELL_RC"
        echo "# StudyRoom" >> "$SHELL_RC"
        echo "$ALIAS_ENTRY" >> "$SHELL_RC"
        ok "Added 'studyroom' alias to $SHELL_RC"
        info "Run 'source $SHELL_RC' or open a new terminal to use the alias"
    else
        ok "Alias already exists in $SHELL_RC"
    fi
fi

# ── Done ─────────────────────────────────────────
echo ""
echo -e "${GREEN}${BOLD}"
echo "   ╔══════════════════════════════════╗"
echo "   ║     ✓  Ready to listen!         ║"
echo "   ╚══════════════════════════════════╝"
echo -e "${RESET}"
echo -e "  Start the app:  ${CYAN}${BOLD}./studyroom${RESET}"
echo -e "  After restart:  ${CYAN}${BOLD}studyroom${RESET}  ${DIM}(alias added to your shell)${RESET}"
echo ""

# В install.sh після створення ./studyroom лаунчера — додати:

if [[ "$OS" == "macos" ]]; then
    APP_PATH="$HOME/Desktop/StudyRoom.app"
    mkdir -p "$APP_PATH/Contents/MacOS"
    mkdir -p "$APP_PATH/Contents/Resources"  # <-- додати це

    FULL_PATH="$(pwd)/$BINARY_PATH"
    cat > "$APP_PATH/Contents/MacOS/StudyRoom" << EOF
#!/bin/bash
cd "$(pwd)"
open -a Terminal "$FULL_PATH"
EOF
    chmod +x "$APP_PATH/Contents/MacOS/StudyRoom"

    cat > "$APP_PATH/Contents/Info.plist" << 'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>StudyRoom</string>
    <key>CFBundleExecutable</key><string>StudyRoom</string>
    <key>CFBundleIdentifier</key><string>com.studyroom.client</string>
    <key>CFBundleVersion</key><string>1.0</string>
    <key>CFBundleIconFile</key><string>AppIcon</string>
</dict>
</plist>
PLIST

    # Іконка — всередині того самого if
    if [[ -f "assets/icon.png" ]]; then
        mkdir -p icon.iconset
        sips -z 16 16   assets/icon.png --out icon.iconset/icon_16x16.png    2>/dev/null
        sips -z 32 32   assets/icon.png --out icon.iconset/icon_16x16@2x.png 2>/dev/null
        sips -z 128 128 assets/icon.png --out icon.iconset/icon_128x128.png  2>/dev/null
        sips -z 256 256 assets/icon.png --out icon.iconset/icon_128x128@2x.png 2>/dev/null
        sips -z 512 512 assets/icon.png --out icon.iconset/icon_512x512.png  2>/dev/null
        iconutil -c icns icon.iconset -o "$APP_PATH/Contents/Resources/AppIcon.icns"
        rm -rf icon.iconset
        ok "Applied custom icon"
    fi

    ok "Created StudyRoom.app on Desktop"

elif [[ "$OS" == "linux" ]]; then
    FULL_PATH="$(pwd)/$BINARY_PATH"
    WORK_DIR="$(pwd)"
    DESKTOP_FILE="$HOME/Desktop/StudyRoom.desktop"

    # Іконка — копіюємо png в системну папку
    ICON_PATH="audio-headphones"  # дефолт якщо нема кастомної
    if [[ -f "assets/icon.png" ]]; then
        ICON_DIR="$HOME/.local/share/icons/hicolor"

        # Копіюємо в стандартні розміри
        for size in 16 32 48 64 128 256 512; do
            mkdir -p "$ICON_DIR/${size}x${size}/apps"
            if command -v convert &>/dev/null; then
                # ImageMagick якщо є
                convert "assets/icon.png" -resize "${size}x${size}" \
                    "$ICON_DIR/${size}x${size}/apps/studyroom.png" 2>/dev/null || true
            else
                # Без конвертації — просто копіюємо оригінал
                cp "assets/icon.png" "$ICON_DIR/${size}x${size}/apps/studyroom.png" 2>/dev/null || true
            fi
        done

        # Оновлюємо кеш іконок
        command -v gtk-update-icon-cache &>/dev/null && \
            gtk-update-icon-cache -f "$ICON_DIR" 2>/dev/null || true

        ICON_PATH="studyroom"
        ok "Applied custom icon"
    fi

    cat > "$DESKTOP_FILE" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=StudyRoom
Comment=Synchronized music listening
Exec=bash -c 'cd $WORK_DIR && $FULL_PATH; exec bash'
Icon=$ICON_PATH
Terminal=true
Categories=AudioVideo;
EOF
    chmod +x "$DESKTOP_FILE"

    command -v gio &>/dev/null && \
        gio set "$DESKTOP_FILE" metadata::trusted true 2>/dev/null || true
    ok "Created StudyRoom shortcut on Desktop"
fi