#pragma once
#include "console/base.hpp"
#include "console/event.hpp"
#include "console/terminal.hpp"

#include <memory>
#include <utility>
#include <vector>

class Container : public Base {
  protected:
	std::vector<std::unique_ptr<Base>> _children;
	int _focusedChild = -1;

  public:
	Container(Position pos = {0, 0}, Size sz = {1, 1}) : Base(pos, sz) {}

	void addChild(std::unique_ptr<Base> child) {
		_children.push_back(std::move(child));
		if (_focusedChild == -1 && _children.back()->isFocusable()) {
			_focusedChild = static_cast<int>(_children.size() - 1);
			_children[_focusedChild]->setFocus(true);
		}
	}

	void draw(Term& term) override {
		for (auto& child : _children) {
			if (child->isVisible()) {
				child->draw(term);
			}
		}
	}

	bool handleEvent(const Event& event) override {
		if (event.type == EventType::KEY_PRESS && event.key == '\t') {
			focusNext();
			return true;
		}

		if (_focusedChild >= 0 && _focusedChild < static_cast<int>(_children.size())) {
			return _children[_focusedChild]->handleEvent(event);
		}

		return false;
	}

  private:
	void focusNext() {
		if (_children.empty())
			return;

		if (_focusedChild >= 0) {
			_children[_focusedChild]->setFocus(false);
			_children[_focusedChild]->onFocusLost();
		}

		int start = (_focusedChild + 1) % static_cast<int>(_children.size());
		for (int i = 0; i < static_cast<int>(_children.size()); i++) {
			int idx = (start + i) % static_cast<int>(_children.size());
			if (_children[idx]->isFocusable()) {
				_focusedChild = idx;
				_children[_focusedChild]->setFocus(true);
				_children[_focusedChild]->onFocusGained();
				break;
			}
		}
	}
};
