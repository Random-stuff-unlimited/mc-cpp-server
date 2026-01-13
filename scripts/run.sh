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

# Get script directory (using realpath for robust path resolution)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Fallback to realpath if available
if command -v realpath &> /dev/null; then
    PROJECT_ROOT="$(realpath "$SCRIPT_DIR/..")"
fi

# Configuration
TARGET_NAME="mc-server"
BUILD_TYPE="${1:-release}"

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

# Copy executable to project root
echo -e "${BLUE}Copying executable to project root...${NC}"
cp "$EXECUTABLE" "$PROJECT_ROOT/$TARGET_NAME"

echo -e "${BOLD}${CYAN}========================================${NC}"
echo -e "${BOLD}${CYAN}  Running $TARGET_NAME ($BUILD_TYPE)${NC}"
echo -e "${BOLD}${CYAN}========================================${NC}"
echo ""

# Run the server from project root
cd "$PROJECT_ROOT"
./"$TARGET_NAME"
