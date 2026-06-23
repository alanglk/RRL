#!/bin/bash
# 1. download all resources into assets/tmp
# 2. unzip if compressed into appropiate folders assets/*
# 3. remove tmp

# Exit immediately if a command exits with a non-zero status
set -e

echo "Fetching assets..."

# --- Assets -------------------------------------------------------------------
declare -A ASSETS=(
    ["models/rungholt"]="https://casual-effects.com/g3d/data10/research/model/rungholt/rungholt.zip"
)


# --- Tmp Dir ------------------------------------------------------------------
TMP_DIR="assets/tmp"
mkdir -p "$TMP_DIR"


# --- Download and Extract -----------------------------------------------------
for DEST_SUBDIR in "${!ASSETS[@]}"; do
    URL="${ASSETS[$DEST_SUBDIR]}"
    FILENAME=$(basename "$URL")
    TMP_FILE="$TMP_DIR/$FILENAME"
    TARGET_DIR="assets/$DEST_SUBDIR"

    echo "----------------------------------------"
    echo "Processing: $FILENAME"
    
    # Download into tmp
    echo " -> Downloading from $URL..."
    curl -L -# -o "$TMP_FILE" "$URL"

    # Create the actual target directory
    mkdir -p "$TARGET_DIR"
    echo " -> Extracting to $TARGET_DIR..."

    # Identify and extract based on file extension
    if [[ "$FILENAME" == *.zip ]]; then
        unzip -q -o "$TMP_FILE" -d "$TARGET_DIR"
    elif [[ "$FILENAME" == *.tar.gz || "$FILENAME" == *.tgz ]]; then
        tar -xzf "$TMP_FILE" -C "$TARGET_DIR"
    elif [[ "$FILENAME" == *.tar ]]; then
        tar -xf "$TMP_FILE" -C "$TARGET_DIR"
    elif [[ "$FILENAME" == *.tar.bz2 || "$FILENAME" == *.tbz2 ]]; then
        tar -xjf "$TMP_FILE" -C "$TARGET_DIR"
    else
        echo " -> Warning: Unrecognized archive format for $FILENAME."
        echo " -> Moving raw file directly to $TARGET_DIR"
        mv "$TMP_FILE" "$TARGET_DIR/"
    fi
    
    echo " -> Done processing $FILENAME."
done

# --- Cleanup ------------------------------------------------------------------
echo "----------------------------------------"
rm -rf "$TMP_DIR"
echo "Removed temporary directory: $TMP_DIR"
echo "Asset pipeline completed successfully!"