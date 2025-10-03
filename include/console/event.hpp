#pragma once
#include "console/terminal.hpp"

enum class EventType { KEY_PRESS, RESIZE, QUIT };

struct Event {
	EventType type;
	char key           = 0;
	Position mouse_pos = {0, 0};

	Event(EventType t) : type(t) {}
	Event(EventType t, char k) : type(t), key(k) {}
};
