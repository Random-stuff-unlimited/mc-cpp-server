# Console Framework Quick Reference

## Compilation
```bash
g++ -std=c++17 -Wall -Wextra -Iinclude src/your_app.cpp -o build/your_app
```

## Basic Setup
```cpp
#include "include/console/console.hpp"

int main() {
    Console app;
    
    // Add widgets
    app.addBase(make_label("Text", x, y));
    app.addBase(make_button("Button", x, y, callback));
    
    app.run(); // Start event loop
    return 0;
}
```

## Widget Creation

### Label
```cpp
auto label = make_label("Hello World!", 10, 5);
label->setText("New text");
label->setColors(Color::GREEN, BGColor::BLACK);
```

### Button
```cpp
auto button = make_button("Click Me", 10, 7, []() {
    // Button action
});
button->setClickHandler(new_callback);
```

### Text Input
```cpp
auto input = make_text_input(10, 9, 30); // x, y, width
std::string text = input->getText();
input->setText("default");
input->setMaskChar('*'); // Password field
```

### Border
```cpp
auto border = make_border(0, 0, 80, 24, "Title");
```

## Custom Widget Template
```cpp
class MyWidget : public Base {
public:
    MyWidget(Position pos) : Base(pos, {width, height}) {
        _focusable = true; // If needs focus
    }
    
    void draw(Term& term) override {
        if (!_visible) return;
        
        term.moveCursor(_position.x, _position.y);
        term.setColor(fgColor, bgColor);
        std::cout << "Content";
        term.resetColor();
    }
    
    bool handleEvent(const Event& event) override {
        if (event.type == EventType::KEY_PRESS) {
            // Handle key: event.key
            return true; // If handled
        }
        return false;
    }
};
```

## Colors
```cpp
// Foreground colors
Color::BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE
Color::BRIGHT_RED, BRIGHT_GREEN, etc.

// Background colors  
BGColor::BLACK, RED, GREEN, etc.

// Usage
widget->setColors(Color::BRIGHT_GREEN, BGColor::BLACK);
```

## Event Handling
```cpp
bool handleEvent(const Event& event) override {
    if (event.type == EventType::KEY_PRESS) {
        switch (event.key) {
            case 'q': /* quit */ return true;
            case '\n': /* enter */ return true;
            case '\t': /* tab */ return true;
            case 27: /* escape */ return true;
            case 3: /* ctrl+c */ return true;
        }
    }
    return false;
}
```

## Terminal Control
```cpp
Term terminal;
terminal.enableRawMode();
terminal.clearScreen();
terminal.moveCursor(x, y);
terminal.setColor(Color::RED, BGColor::WHITE);
terminal.hideCursor();
terminal.showCursor();
terminal.resetColor();
char key = terminal.readKey();
Size size = terminal.getTerminalSize();
```

## Widget Properties
```cpp
// Position and size
widget->setPosition(Position{x, y});
widget->setSize(Size{width, height});

// Visibility and focus
widget->setVisible(false);
widget->setFocus(true);

// Check state
bool visible = widget->isVisible();
bool focused = widget->isFocused();
bool can_focus = widget->isFocusable();
```

## Container Management
```cpp
Container container;
container.addChild(std::make_unique<Label>("Child", Position{0, 0}));

// Container automatically handles:
// - TAB navigation between focusable children
// - Event routing to focused child
// - Drawing all visible children
```

## Controls Reference
- **TAB**: Navigate between focusable widgets
- **ENTER/SPACE**: Activate buttons
- **BACKSPACE**: Delete in text inputs
- **ESC/Ctrl+C**: Default quit (handled by Console)

## Common Patterns

### Button with Widget Reference
```cpp
auto input = make_text_input(10, 5, 20);
auto input_ptr = input.get(); // Keep reference
app.addBase(std::move(input));

app.addBase(make_button("Clear", 10, 7, [input_ptr]() {
    input_ptr->setText("");
}));
```

### Quit Button
```cpp
app.addBase(make_button("Quit", 10, 9, [&app]() {
    app.stop();
}));
```

### Dialog Pattern
```cpp
void showDialog() {
    Console dialog;
    
    bool result = false;
    dialog.addBase(make_button("OK", 10, 5, [&]() {
        result = true;
        dialog.stop();
    }));
    
    dialog.run();
    // Use result after dialog closes
}
```

## Unicode Box Drawing
```
Top-left: ┌    Top-right: ┐
Bottom-left: └ Bottom-right: ┘
Horizontal: ─  Vertical: │

Example:
┌──────────┐
│ Content  │
└──────────┘
```

## Debugging Tips
```cpp
// Debug positions with temporary labels
app.addBase(make_label("DEBUG: X=10, Y=5", 10, 5));

// Check terminal size
Size size = terminal.getTerminalSize();
app.addBase(make_label("Terminal: " + std::to_string(size.width) + "x" + 
                      std::to_string(size.height), 0, 0));
```

## Build Script Example
```bash
#!/bin/bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Iinclude src/console_app.cpp -o build/console_app
if [ $? -eq 0 ]; then
    echo "✅ Build successful"
    ./build/console_app
fi
```
