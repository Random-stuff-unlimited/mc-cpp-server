# Console Framework Documentation

A lightweight Text User Interface (TUI) framework built in C++ using ANSI escape codes for creating interactive terminal applications.

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Getting Started](#getting-started)
- [Core Classes](#core-classes)
- [Widget Reference](#widget-reference)
- [Event System](#event-system)
- [Examples](#examples)
- [API Reference](#api-reference)
- [Best Practices](#best-practices)
- [Troubleshooting](#troubleshooting)

## Overview

The Console framework provides a simple yet powerful way to create terminal-based user interfaces. It features:

- **Widget-based architecture** with inheritance hierarchy
- **Event-driven programming** model
- **Focus management** with keyboard navigation
- **Cross-platform compatibility** (Linux, macOS, Windows Terminal)
- **Minimal dependencies** (standard C++17 + POSIX termios)
- **ASCII-based drawing** for maximum compatibility

## Architecture

The framework follows a hierarchical widget system:

```
Base (Abstract Widget Class)
├── Container (holds other widgets)
├── Label (text display)
├── Button (clickable element)
├── TextInput (text editing)
└── Border (decorative frame)
```

### Core Components

- **`Term`**: Low-level terminal control (colors, cursor, input)
- **`Base`**: Abstract base class for all widgets
- **`Container`**: Widget that can hold and manage other widgets
- **`Console`**: Main application class managing the event loop
- **Event System**: Handles keyboard input and widget interactions

## Getting Started

### Basic Usage

```cpp
#include "include/console/console.hpp"

int main() {
    Console app;
    
    // Add widgets
    app.addBase(make_label("Hello World!", 10, 5));
    app.addBase(make_button("Click Me", 10, 7, []() {
        // Button action
    }));
    
    // Run the application
    app.run();
    return 0;
}
```

### Compilation

```bash
g++ -std=c++17 -Wall -Wextra -Iinclude src/your_app.cpp -o build/your_app
```

### Controls

- **TAB**: Navigate between focusable widgets
- **ENTER/SPACE**: Activate buttons
- **BACKSPACE**: Delete characters in text inputs
- **ESC/Ctrl+C**: Quit application

## Core Classes

### Term (Terminal Control)

Provides low-level terminal operations:

```cpp
Term terminal;
terminal.enableRawMode();        // Enable raw input mode
terminal.clearScreen();          // Clear the screen
terminal.moveCursor(x, y);       // Move cursor to position
terminal.setColor(Color::RED, BGColor::WHITE);  // Set colors
terminal.hideCursor();           // Hide cursor
terminal.showCursor();           // Show cursor
char key = terminal.readKey();   // Read single keypress
Size size = terminal.getTerminalSize(); // Get terminal dimensions
```

### Base (Widget Base Class)

All widgets inherit from `Base`:

```cpp
class MyWidget : public Base {
public:
    MyWidget(Position pos, Size size) : Base(pos, size) {
        _focusable = true; // Make it focusable if needed
    }
    
    void draw(Term& term) override {
        // Implement drawing logic
        if (!_visible) return;
        
        term.moveCursor(_position.x, _position.y);
        term.setColor(fgColor, bgColor);
        std::cout << "My Widget";
        term.resetColor();
    }
    
    bool handleEvent(const Event& event) override {
        // Handle keyboard events
        if (event.type == EventType::KEY_PRESS) {
            // Process key press
            return true; // Event handled
        }
        return false; // Event not handled
    }
};
```

### Console (Application Manager)

Main application class:

```cpp
Console app;

// Add widgets
app.addBase(std::make_unique<Label>("Text", Position{0, 0}));

// Run event loop
app.run();

// Stop programmatically
app.stop();
```

## Widget Reference

### Label

Displays static or dynamic text:

```cpp
// Create label
auto label = make_label("Hello World!", 10, 5);

// Modify text
label->setText("Updated text");

// Set colors
label->setColors(Color::BRIGHT_GREEN, BGColor::BLACK);

// Position and visibility
label->setPosition(Position{20, 10});
label->setVisible(false);
```

### Button

Interactive clickable element:

```cpp
// Create button with callback
auto button = make_button("Save", 10, 8, []() {
    std::cout << "Save clicked!" << std::endl;
});

// Change click handler
button->setClickHandler([]() {
    // New action
});

// Styling
button->setColors(Color::WHITE, BGColor::BLUE);
```

**Button Appearance:**
```
┌----------┐
│ Save     │
└----------┘
```

### TextInput

Text editing field with cursor:

```cpp
// Create text input
auto input = make_text_input(10, 12, 30); // x, y, width

// Get/set text
std::string text = input->getText();
input->setText("Default text");

// Password field
input->setMaskChar('*');

// Limit length
input->setMaxLength(50);
```

### Border

Decorative frame around content:

```cpp
// Create border
auto border = make_border(5, 5, 50, 20, "Window Title");

// Without title
auto simple_border = make_border(0, 0, 80, 24);
```

**Border Appearance:**
```
┌─ Window Title ────────────────┐
│                              │
│                              │
└──────────────────────────────┘
```

### Container

Manages child widgets and focus:

```cpp
Container container(Position{0, 0}, Size{80, 24});

// Add children
container.addChild(std::make_unique<Label>("Child 1", Position{5, 5}));
container.addChild(std::make_unique<Button>("Child 2", Position{5, 7}));

// Container handles focus management automatically
```

## Event System

### Event Types

```cpp
enum class EventType { 
    KEY_PRESS,  // Keyboard input
    RESIZE,     // Terminal resize (future)
    QUIT        // Application quit (future)
};
```

### Event Structure

```cpp
struct Event {
    EventType type;
    char key = 0;                    // For KEY_PRESS events
    Position mouse_pos = {0, 0};     // For mouse events (future)
};
```

### Handling Events

```cpp
bool MyWidget::handleEvent(const Event& event) {
    if (event.type == EventType::KEY_PRESS) {
        switch (event.key) {
            case 'q':
                // Handle 'q' key
                return true;
            case '\n':
                // Handle Enter key
                return true;
            default:
                return false; // Not handled
        }
    }
    return false;
}
```

## Examples

### Simple Application

```cpp
#include "include/console/console.hpp"

int main() {
    Console app;
    
    // Title
    app.addBase(make_label("My Application", 2, 1));
    
    // Input field
    app.addBase(make_label("Name:", 2, 3));
    auto name_input = make_text_input(8, 3, 20);
    auto name_ptr = name_input.get(); // Keep reference
    app.addBase(std::move(name_input));
    
    // Buttons
    app.addBase(make_button("Submit", 2, 5, [name_ptr]() {
        std::cout << "Hello, " << name_ptr->getText() << "!" << std::endl;
    }));
    
    app.addBase(make_button("Clear", 12, 5, [name_ptr]() {
        name_ptr->setText("");
    }));
    
    app.addBase(make_button("Quit", 22, 5, [&app]() {
        app.stop();
    }));
    
    app.run();
    return 0;
}
```

### Dialog Box

```cpp
void showDialog() {
    Console dialog;
    
    // Border
    dialog.addBase(make_border(10, 5, 50, 10, "Confirmation"));
    
    // Message
    dialog.addBase(make_label("Are you sure you want to continue?", 12, 7));
    
    // Buttons
    bool confirmed = false;
    dialog.addBase(make_button("Yes", 15, 10, [&]() {
        confirmed = true;
        dialog.stop();
    }));
    
    dialog.addBase(make_button("No", 25, 10, [&dialog]() {
        dialog.stop();
    }));
    
    dialog.run();
    
    if (confirmed) {
        // User confirmed
    }
}
```

### Menu System

```cpp
class MenuApp {
    Console app;
    std::vector<std::string> options = {
        "Start Game", "Settings", "Help", "Exit"
    };
    
public:
    void run() {
        app.addBase(make_border(5, 2, 30, 15, "Main Menu"));
        
        for (size_t i = 0; i < options.size(); ++i) {
            app.addBase(make_button(options[i], 7, 4 + i * 2, [this, i]() {
                handleMenuChoice(i);
            }));
        }
        
        app.run();
    }
    
private:
    void handleMenuChoice(size_t choice) {
        switch (choice) {
            case 0: startGame(); break;
            case 1: showSettings(); break;
            case 2: showHelp(); break;
            case 3: app.stop(); break;
        }
    }
};
```

## API Reference

### Colors

```cpp
enum class Color {
    RESET, BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE,
    BRIGHT_BLACK, BRIGHT_RED, BRIGHT_GREEN, BRIGHT_YELLOW,
    BRIGHT_BLUE, BRIGHT_MAGENTA, BRIGHT_CYAN, BRIGHT_WHITE
};

enum class BGColor {
    // Same values as Color but for background
};
```

### Position and Size

```cpp
struct Position {
    int x, y;
    Position(int x = 0, int y = 0);
    Position operator+(const Position& other) const;
};

struct Size {
    int width, height;
    Size(int w = 0, int h = 0);
};
```

### Utility Functions

```cpp
// Widget creation helpers
std::unique_ptr<Label> make_label(const std::string& text, int x, int y);
std::unique_ptr<Button> make_button(const std::string& text, int x, int y, 
                                   std::function<void()> handler = nullptr);
std::unique_ptr<TextInput> make_text_input(int x, int y, int width = 20);
std::unique_ptr<Border> make_border(int x, int y, int w, int h, 
                                   const std::string& title = "");
```

## Best Practices

### Widget Management

1. **Use smart pointers**: Always use `std::unique_ptr` for widget ownership
2. **Keep references**: Store raw pointers when you need to access widgets later
3. **Avoid deep nesting**: Keep widget hierarchies shallow for better performance

### Event Handling

1. **Return true when handled**: Always return `true` from `handleEvent()` when you process an event
2. **Chain events**: Let unhandled events bubble up to parent containers
3. **Use lambda captures carefully**: Be mindful of capture lifetime in button callbacks

### Layout and Design

1. **Plan your coordinates**: Draw your UI on paper before coding
2. **Use consistent spacing**: Maintain regular spacing between elements
3. **Test different terminal sizes**: Ensure your UI works on various screen sizes
4. **Provide visual feedback**: Use colors and focus indicators effectively

### Performance

1. **Minimize draws**: Only redraw when necessary
2. **Batch operations**: Group multiple widget updates together
3. **Cache expensive operations**: Store computed values when possible

## Troubleshooting

### Common Issues

**Widgets not appearing:**
- Check widget positions are within terminal bounds
- Verify widgets are marked as visible
- Ensure proper color settings

**Focus navigation not working:**
- Make sure widgets are marked as focusable: `_focusable = true`
- Check widgets are added to containers properly
- Verify TAB key handling in containers

**Unicode characters not displaying:**
- Framework uses ASCII characters for maximum compatibility
- Ensure terminal supports UTF-8 if using custom Unicode

**Colors not appearing:**
- Verify terminal supports ANSI color codes
- Check color enum values are correct
- Reset colors after drawing: `term.resetColor()`

### Debug Tips

1. **Add debug labels**: Use temporary labels to verify positions
2. **Test minimal examples**: Start with simple widgets and build up
3. **Check terminal capabilities**: Test on different terminal emulators
4. **Use raw mode carefully**: Ensure raw mode is disabled on exit

### Platform-Specific Notes

**Linux**: Works with most terminal emulators
**macOS**: Compatible with Terminal.app and iTerm2  
**Windows**: Requires Windows Terminal or WSL (not old cmd.exe)

### Memory Management

The framework uses RAII and smart pointers extensively:
- Widgets are automatically cleaned up when the Console destructor runs
- Raw mode is automatically disabled on app termination
- No manual memory management required

## Future Enhancements

Potential improvements for future versions:
- Mouse event support
- Window/dialog management
- Layout managers (grid, flex)
- Themes and styling system
- Unicode box drawing characters option
- Terminal resize handling
- Multi-threading support
- Custom widget library