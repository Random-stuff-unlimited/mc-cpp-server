#ifndef DIFFICULTY_INSTANCE_HPP
#define DIFFICULTY_INSTANCE_HPP

#include <algorithm>
#include <cstdint>

// DifficultyInstance: the local difficulty — the world's difficulty, raised by the days that went by, the time
// players spent in the chunk and the moon. Mobs spawn with more equipment, enchanted, on a harder local difficulty
class DifficultyInstance {
  public:
	// difficulty: 0 peaceful .. 3 hard
	DifficultyInstance(int difficulty, int64_t dayTime, int64_t inhabitedTime, float moonBrightness) : _base(difficulty) {
		if (difficulty == 0) {
			_effective = 0.0F;
			return;
		}
		bool  hard	   = difficulty == 3;
		float global   = std::clamp((static_cast<float>(dayTime) + -72000.0F) / 1440000.0F, 0.0F, 1.0F) * 0.25F;
		float value	   = 0.75F + global;
		float local	   = std::clamp(static_cast<float>(inhabitedTime) / 3600000.0F, 0.0F, 1.0F) * (hard ? 1.0F : 0.75F);
		local		  += std::clamp(moonBrightness * 0.25F, 0.0F, global);
		if (difficulty == 1) local *= 0.5F;
		value		  += local;
		_effective	   = difficulty * value;
	}

	int	  getDifficulty() const { return _base; }
	float getEffectiveDifficulty() const { return _effective; }
	bool  isHard() const { return _effective >= 3.0F; }
	bool  isHarderThan(float value) const { return _effective > value; }
	float getSpecialMultiplier() const {
		if (_effective < 2.0F) return 0.0F;
		return _effective > 4.0F ? 1.0F : (_effective - 2.0F) / 2.0F;
	}

  private:
	int	  _base;
	float _effective;
};

#endif
