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
fi

AMAZON_FREERTOS_DIR="$PWD/component/common/application/amazon-freertos"
AMAZON_FREERTOS_REPO_URL="https://github.com/Ameba-AIoT/ameba-amazon-freertos.git"
AMAZON_FREERTOS_REPO_BRANCH="FreeRTOS-LTS-202406.xx"

# --- 1: Cloning Amazon FreeRTOS submodule ---
if [ ! -d "$AMAZON_FREERTOS_DIR" ]; then
  echo "Cloning AWS repository..."
  git clone --recurse-submodules -b "$AMAZON_FREERTOS_REPO_BRANCH" "$AMAZON_FREERTOS_REPO_URL" "$AMAZON_FREERTOS_DIR"
fi

echo "Amazon FreeRTOS setup complete"
