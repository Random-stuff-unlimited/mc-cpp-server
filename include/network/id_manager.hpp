#ifndef ID_MANAGER_HPP
#define ID_MANAGER_HPP

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <set>

class IdManager {
  private:
	uint32_t		   _nextId;
	std::set<uint32_t> _freedIds;
	mutable std::mutex _mutex;

  public:
	IdManager();
	~IdManager();

	uint32_t allocate();
	void	 release(uint32_t id);
	size_t	 getAllocatedCount() const;
	size_t	 getFreedCount() const;
	void	 reset();
};

#endif
