#ifndef JAVA_HASH_SET_HPP
#define JAVA_HASH_SET_HPP

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

// A set iterated in the order of Java's HashSet (java.util.HashMap), for the few places where vanilla's result
// depends on it (tree generation). T needs javaHashCode() and std::hash.
//
// HashMap puts an element in bucket (h ^ h >>> 16) & (capacity - 1), buckets keep their insertion order (resizing
// splits them without reordering) and iteration goes bucket by bucket. The capacity starts at 16 and doubles when
// the size goes over 3/4 of it, never shrinking. Only buckets turned into trees (8 collisions and more) aren't
// reproduced, which small sets of positions don't reach.
template <typename T> class JavaHashSet {
  public:
	bool add(const T& value) {
		if (_index.count(value)) return false;
		_index.emplace(value, _entries.size());
		_entries.push_back({value, spread(value.javaHashCode()), _sequence++, true});
		_size++;
		if (_size > _capacity * 3 / 4) _capacity *= 2;
		return true;
	}
	bool contains(const T& value) const { return _index.count(value) != 0; }
	bool remove(const T& value) {
		auto it = _index.find(value);
		if (it == _index.end()) return false;
		_entries[it->second].present = false;
		_index.erase(it);
		_size--;
		if (_size == 0) _entries.clear();
		return true;
	}
	bool   empty() const { return _size == 0; }
	size_t size() const { return _size; }

	// The first element of an iteration (iterator().next()); the set must not be empty
	T first() const {
		const Entry* best = nullptr;
		for (const Entry& entry : _entries) {
			if (entry.present && (!best || before(entry, *best))) best = &entry;
		}
		return best->value;
	}
	// Every element, in iteration order
	std::vector<T> values() const {
		std::vector<const Entry*> present;
		for (const Entry& entry : _entries) {
			if (entry.present) present.push_back(&entry);
		}
		std::sort(present.begin(), present.end(), [this](const Entry* a, const Entry* b) { return before(*a, *b); });
		std::vector<T> result;
		result.reserve(present.size());
		for (const Entry* entry : present) result.push_back(entry->value);
		return result;
	}

  private:
	struct Entry {
		T		 value;
		uint32_t hash;
		uint64_t sequence;
		bool	 present;
	};

	std::vector<Entry>			  _entries;
	std::unordered_map<T, size_t> _index;
	size_t						  _size		= 0;
	uint32_t					  _capacity = 16;
	uint64_t					  _sequence = 0;

	static uint32_t spread(int32_t hash) {
		uint32_t h = static_cast<uint32_t>(hash);
		return h ^ (h >> 16);
	}
	bool before(const Entry& a, const Entry& b) const {
		uint32_t bucketA = a.hash & (_capacity - 1), bucketB = b.hash & (_capacity - 1);
		return bucketA != bucketB ? bucketA < bucketB : a.sequence < b.sequence;
	}
};

#endif
