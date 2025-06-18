#!/bin/bash

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <IC type>"
  echo "IC type: amebad"
  exit 1
fi

AMEBA="$1"

if [ "$AMEBA" != "amebad" ]; then
  echo "Invalid IC type. Expected: amebad."
  exit 1
else
  AMEBA_EXAMPLE="amebaD"
fi

REPO_NAME=$(basename -s .git "$(git rev-parse --show-toplevel 2>/dev/null)")

CHIP_LINK="$PWD/third_party/connectedhomeip"
CHIP_SRC="../../connectedhomeip"

PROJECT_DIR="$PWD/project/realtek_${AMEBA_EXAMPLE}_v0_example"
MATTER_DIR="$PWD/component/common/application/matter"
MATTER_PROJECT_DIR="$MATTER_DIR/project/${AMEBA}"
MATTER_SCRIPT="$MATTER_DIR/tools/scripts/matter_version_selection.sh"

# --- 1: Setup third_party softlink ---
mkdir -p third_party
rm -rf "$CHIP_LINK"
ln -s "$CHIP_SRC" "$CHIP_LINK"

# --- 2: Clone Ameba Matter repo if not present ---
if [ ! -d "$MATTER_DIR" ]; then
  echo "Cloning Matter repository..."
  git clone https://github.com/Ameba-AIoT/ameba-rtos-matter.git "$MATTER_DIR"
fi

# --- 3: Run matter version selection script ---
cd "$MATTER_DIR" || exit 1

if [ -f "$MATTER_SCRIPT" ]; then
  bash "$MATTER_SCRIPT" "$AMEBA" "$REPO_NAME"
else
  echo "Error: $MATTER_SCRIPT not found."
  exit 1
fi

# --- 4: Ensure correct Matter branch ---
MATTER_BRANCH=$(git rev-parse --abbrev-ref HEAD)

if git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
  current_branch=$(git rev-parse --abbrev-ref HEAD)
  if [ "$current_branch" != "$MATTER_BRANCH" ]; then
    echo "Switching to branch '$MATTER_BRANCH'"
    git checkout "$MATTER_BRANCH" || exit 1
  fi
else
  echo "Matter repo seems broken. Re-cloning..."
  cd - > /dev/null || exit 1
  rm -rf "$MATTER_DIR"
  git clone -b "$MATTER_BRANCH" https://github.com/Ameba-AIoT/ameba-rtos-matter.git "$MATTER_DIR"
fi

cd - > /dev/null || exit 1

echo "Matter setup complete"