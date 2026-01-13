#!/bin/bash
# =============================================================================
# mc-server Run Script
# Sets up test environment and runs the Minecraft server
# =============================================================================

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Configuration
TARGET_NAME="mc-server"
BUILD_TYPE="${1:-release}"
TEST_SERVER_DIR="$PROJECT_ROOT/test-server"

# Validate build type
if [[ "$BUILD_TYPE" != "debug" && "$BUILD_TYPE" != "release" && "$BUILD_TYPE" != "relwithdebinfo" ]]; then
    echo -e "${RED}Error: Invalid build type '$BUILD_TYPE'${NC}"
    echo -e "Usage: $0 [debug|release|relwithdebinfo]"
    exit 1
fi

BUILD_DIR="$PROJECT_ROOT/build/$BUILD_TYPE"
EXECUTABLE="$BUILD_DIR/$TARGET_NAME"

echo -e "${BOLD}${CYAN}========================================${NC}"
echo -e "${BOLD}${CYAN}  mc-server Run Script${NC}"
echo -e "${BOLD}${CYAN}========================================${NC}"
echo ""

# Check if executable exists
if [[ ! -f "$EXECUTABLE" ]]; then
    echo -e "${YELLOW}Executable not found. Building in $BUILD_TYPE mode...${NC}"
    cd "$PROJECT_ROOT"
    cmake --workflow --preset "$BUILD_TYPE"
fi

# Verify executable exists after build
if [[ ! -f "$EXECUTABLE" ]]; then
    echo -e "${RED}Error: Failed to build executable${NC}"
    exit 1
fi

echo -e "${BLUE}Setting up test-server environment...${NC}"

# Create test-server directory
mkdir -p "$TEST_SERVER_DIR"

# Copy executable
echo -e "${CYAN}  Copying executable...${NC}"
cp "$EXECUTABLE" "$TEST_SERVER_DIR/$TARGET_NAME"

# Copy config.json if it exists
if [[ -f "$PROJECT_ROOT/config.json" ]]; then
    echo -e "${CYAN}  Copying config.json...${NC}"
    cp "$PROJECT_ROOT/config.json" "$TEST_SERVER_DIR/"
else
    echo -e "${YELLOW}  Warning: config.json not found, skipping...${NC}"
fi

# Copy world folder if it exists
if [[ -d "$PROJECT_ROOT/world" ]]; then
    echo -e "${CYAN}  Copying world folder...${NC}"
    cp -r "$PROJECT_ROOT/world" "$TEST_SERVER_DIR/"
else
    echo -e "${YELLOW}  Warning: world folder not found, skipping...${NC}"
fi

echo ""
echo -e "${GREEN}Test environment ready!${NC}"
echo -e "${BOLD}${CYAN}========================================${NC}"
echo -e "${BOLD}${CYAN}  Running $TARGET_NAME ($BUILD_TYPE)${NC}"
echo -e "${BOLD}${CYAN}========================================${NC}"
echo ""

# Run the server
cd "$TEST_SERVER_DIR"
./"$TARGET_NAME"
