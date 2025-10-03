# =============================================================================
# Beautiful C++ Makefile with Colors and Dependency Tracking
# =============================================================================

# ================================ CONFIGURATION =============================
# Easily modifiable output file names and paths
TARGET_NAME     := mc-server
BUILD_DIR       := build
SOURCE_DIR      := src
INCLUDE_DIR     := include
DEPS_DIR        := .deps

# Final executable path (easily modifiable)
TARGET          := $(BUILD_DIR)/$(TARGET_NAME)

# Compiler and flags
CXX             := g++
CXXFLAGS        := -std=c++17 -Wall -Wextra -Wpedantic -O2
DEBUG_FLAGS     := -g -DDEBUG -O0
RELEASE_FLAGS   := -DNDEBUG -O3
INCLUDE_FLAGS   := -I$(INCLUDE_DIR)

# Linker flags (add your libraries here)
LDFLAGS         :=
LIBS            :=

# ================================ COLOR SETUP ===============================
# ANSI color codes for beautiful output
RESET           := \033[0m
BOLD            := \033[1m
DIM             := \033[2m

# Text colors
BLACK           := \033[30m
RED             := \033[31m
GREEN           := \033[32m
YELLOW          := \033[33m
BLUE            := \033[34m
MAGENTA         := \033[35m
CYAN            := \033[36m
WHITE           := \033[37m

# Background colors
BG_BLACK        := \033[40m
BG_RED          := \033[41m
BG_GREEN        := \033[42m
BG_YELLOW       := \033[43m
BG_BLUE         := \033[44m
BG_MAGENTA      := \033[45m
BG_CYAN         := \033[46m
BG_WHITE        := \033[47m

# Bright colors
BRIGHT_RED      := \033[91m
BRIGHT_GREEN    := \033[92m
BRIGHT_YELLOW   := \033[93m
BRIGHT_BLUE     := \033[94m
BRIGHT_MAGENTA  := \033[95m
BRIGHT_CYAN     := \033[96m
BRIGHT_WHITE    := \033[97m

# ============================= FILE DISCOVERY ==============================
# Automatically find all source files recursively
SOURCES         := $(shell find $(SOURCE_DIR) -name "*.cpp" -type f)
HEADERS         := $(shell find $(INCLUDE_DIR) -name "*.hpp" -type f)
OBJECTS         := $(patsubst $(SOURCE_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS            := $(patsubst $(SOURCE_DIR)/%.cpp,$(DEPS_DIR)/%.d,$(SOURCES))

# ============================== BUILD MODES ===============================
# Default build mode
BUILD_MODE      := release

# Set flags based on build mode
ifeq ($(BUILD_MODE),debug)
    CXXFLAGS += $(DEBUG_FLAGS)
    MODE_COLOR := $(BRIGHT_YELLOW)
    MODE_NAME := DEBUG
else
    CXXFLAGS += $(RELEASE_FLAGS)
    MODE_COLOR := $(BRIGHT_GREEN)
    MODE_NAME := RELEASE
endif

# ================================= TARGETS ==================================
.PHONY: all clean debug release info help run install uninstall compile_commands test_logger debug_logger test_logger_tui

# Default target
all: info $(TARGET)

# Build in debug mode
debug:
	@$(MAKE) BUILD_MODE=debug all

# Build in release mode
release:
	@$(MAKE) BUILD_MODE=release all

# Create the main executable
$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	@printf "$(BOLD)$(BRIGHT_CYAN)🔗 Linking executable: $(BRIGHT_WHITE)$@$(RESET)\n"
	@$(CXX) $(OBJECTS) -o $@ $(LDFLAGS) $(LIBS)
	@printf "$(BOLD)$(BRIGHT_GREEN)✅ Build completed successfully!$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_BLUE)📁 Executable: $(BRIGHT_WHITE)$@$(RESET)\n"

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp $(DEPS_DIR)/%.d | $(BUILD_DIR) $(DEPS_DIR)
	@printf "$(BOLD)$(BRIGHT_BLUE)🔨 Compiling: $(BRIGHT_WHITE)$<$(RESET)\n"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDE_FLAGS) -c $< -o $@

# Generate dependency files
$(DEPS_DIR)/%.d: $(SOURCE_DIR)/%.cpp | $(DEPS_DIR)
	@printf "$(DIM)$(CYAN)📋 Generating dependencies: $<$(RESET)\n"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDE_FLAGS) -MM -MT $(BUILD_DIR)/$*.o $< > $@

# Create build directories
$(BUILD_DIR):
	@printf "$(BOLD)$(YELLOW)📁 Creating build directory: $(BRIGHT_WHITE)$@$(RESET)\n"
	@mkdir -p $@

$(DEPS_DIR):
	@printf "$(DIM)$(YELLOW)📁 Creating deps directory: $(BRIGHT_WHITE)$@$(RESET)\n"
	@mkdir -p $@

# Include dependency files (only if they exist)
-include $(DEPS)

# Clean build artifacts
clean:
	@printf "$(BOLD)$(BRIGHT_RED)🧹 Cleaning build artifacts...$(RESET)\n"
	@rm -rf $(BUILD_DIR) $(DEPS_DIR)
	@printf "$(BOLD)$(BRIGHT_GREEN)✨ Clean completed!$(RESET)\n"

# Run the executable
run: $(TARGET)
	@printf "$(BOLD)$(BRIGHT_MAGENTA)🚀 Running $(TARGET_NAME)...$(RESET)\n"
	@printf "$(DIM)$(WHITE)" && echo "================================================" && printf "$(RESET)"
	@./$(TARGET)
	@printf "$(DIM)$(WHITE)" && echo "================================================" && printf "$(RESET)"

# Display project information
info:
	@printf "$(BOLD)$(BG_BLUE)$(WHITE) 🏗️  C++ BUILD SYSTEM $(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)╔═══════════════════════════════════════════════╗$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BOLD)Project:$(RESET)                 $(BRIGHT_WHITE)%-20s$(RESET) $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n" "$(TARGET_NAME)"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BOLD)Mode:$(RESET)                    $(MODE_COLOR)%-20s$(RESET) $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n" "$(MODE_NAME)"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BOLD)Compiler:$(RESET)                $(BRIGHT_WHITE)%-20s$(RESET) $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n" "$(CXX)"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BOLD)Target:$(RESET)                  $(BRIGHT_WHITE)%-20s$(RESET) $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n" "$(TARGET)"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BOLD)Sources:$(RESET)                 $(BRIGHT_GREEN)%-20s$(RESET) $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n" "$(words $(SOURCES)) files"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BOLD)Headers:$(RESET)                 $(BRIGHT_GREEN)%-20s$(RESET) $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n" "$(words $(HEADERS)) files"
	@printf "$(BOLD)$(BRIGHT_CYAN)╚═══════════════════════════════════════════════╝$(RESET)\n"

# Install the executable (modify INSTALL_PREFIX as needed)
INSTALL_PREFIX := /usr/local
install: $(TARGET)
	@printf "$(BOLD)$(BRIGHT_BLUE)📦 Installing $(TARGET_NAME) to $(INSTALL_PREFIX)/bin...$(RESET)\n"
	@sudo cp $(TARGET) $(INSTALL_PREFIX)/bin/$(TARGET_NAME)
	@sudo chmod +x $(INSTALL_PREFIX)/bin/$(TARGET_NAME)
	@printf "$(BOLD)$(BRIGHT_GREEN)✅ Installation completed!$(RESET)\n"

# Uninstall the executable
uninstall:
	@printf "$(BOLD)$(BRIGHT_RED)🗑️  Uninstalling $(TARGET_NAME)...$(RESET)\n"
	@sudo rm -f $(INSTALL_PREFIX)/bin/$(TARGET_NAME)
	@printf "$(BOLD)$(BRIGHT_GREEN)✅ Uninstallation completed!$(RESET)\n"

# Generate compile_commands.json for LSP support
compile_commands:
	@printf "$(BOLD)$(BRIGHT_BLUE)📝 Generating compile_commands.json for LSP...$(RESET)\n"
	@echo '[' > compile_commands.json
	@first=true; for src in $(SOURCES); do \
		[ "$$first" = true ] && first=false || echo ',' >> compile_commands.json; \
		echo '  {' >> compile_commands.json; \
		echo '    "directory": "'$(shell pwd)'",' >> compile_commands.json; \
		obj_path=$$(echo "$$src" | sed 's|$(SOURCE_DIR)/|$(BUILD_DIR)/|' | sed 's|\.cpp$$|.o|'); \
		echo "    \"command\": \"$(CXX) $(CXXFLAGS) $(INCLUDE_FLAGS) -c $$src -o $$obj_path\"," >> compile_commands.json; \
		echo '    "file": "'$$src'"' >> compile_commands.json; \
		echo '  }' >> compile_commands.json; \
	done
	@echo ']' >> compile_commands.json
	@printf "$(BOLD)$(BRIGHT_GREEN)✅ compile_commands.json generated successfully!$(RESET)\n"

# Display help information
help:
	@printf "$(BOLD)$(BG_GREEN)$(WHITE) 📖 MAKEFILE HELP $(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)╔══════════════════════════════════════════════════════════╗$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BOLD)Available targets:$(RESET)                                       $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET)                                                          $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)    ║$(RESET)\n" "all" "Build the project (default: release mode)"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "debug" "Build in debug mode"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "release" "Build in release mode"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "clean" "Remove all build artifacts"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "run" "Build and run the executable"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "compile_commands" "Generate compile_commands.json for LSP"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "install" "Install the executable to system"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "uninstall" "Remove the executable from system"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "info" "Display project information"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "help" "Show this help message"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "test_logger" "Build and run logger example"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "debug_logger" "Build and run debug logger (no TUI)"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BRIGHT_GREEN)%-10s$(RESET) %-39s $(BOLD)$(BRIGHT_CYAN)      ║$(RESET)\n" "test_logger_tui" "Build and run TUI-safe logger test"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET)                                                          $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) $(BOLD)Customization:$(RESET)                                           $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) • Modify $(BRIGHT_YELLOW)TARGET_NAME$(RESET) to change executable name           $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) • Modify $(BRIGHT_YELLOW)BUILD_DIR$(RESET) to change build directory             $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) • Add libraries to $(BRIGHT_YELLOW)LIBS$(RESET) variable                         $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)║$(RESET) • Add compiler flags to $(BRIGHT_YELLOW)CXXFLAGS$(RESET)                         $(BOLD)$(BRIGHT_CYAN)║$(RESET)\n"
	@printf "$(BOLD)$(BRIGHT_CYAN)╚══════════════════════════════════════════════════════════╝$(RESET)\n"

# =============================================================================
# 🎨 Beautiful Makefile - Features:
# • Colorful output with emojis
# • Automatic dependency tracking (.deps directory)
# • Debug and release build modes
# • Easy customization of output names
# • Clean directory structure
# • Install/uninstall targets
# • Comprehensive help system
# • No hardcoded file names (all auto-discovered)
# =============================================================================
