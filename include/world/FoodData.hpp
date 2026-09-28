#ifndef FOOD_DATA_HPP
#define FOOD_DATA_HPP

#include <algorithm>

// A player's hunger (vanilla's FoodData). Its tick, which needs the player, is Survival::tick.
// Saved by vanilla as foodLevel, foodSaturationLevel, foodExhaustionLevel and foodTickTimer
class FoodData {
  public:
	// FoodConstants
	static constexpr int   MAX_FOOD		   = 20;
	static constexpr float MAX_SATURATION  = 20.0F;
	static constexpr float EXHAUSTION_DROP = 4.0F;
	static constexpr int   HEAL_LEVEL	   = 18;
	static constexpr float EXHAUSTION_HEAL = 6.0F;
	static constexpr float EXHAUSTION_JUMP = 0.05F, EXHAUSTION_SPRINT_JUMP = 0.2F, EXHAUSTION_MINE = 0.005F, EXHAUSTION_ATTACK = 0.1F,
						   EXHAUSTION_SPRINT = 0.1F, EXHAUSTION_SWIM = 0.01F;

	// FoodData.eat(FoodProperties): nutrition and saturation (already multiplied, see FoodConstants.saturationByModifier)
	void eat(int nutrition, float saturation) {
		_foodLevel		 = std::clamp(nutrition + _foodLevel, 0, MAX_FOOD);
		_saturationLevel = std::clamp(saturation + _saturationLevel, 0.0F, static_cast<float>(_foodLevel));
	}
	void addExhaustion(float exhaustion) { _exhaustionLevel = std::min(_exhaustionLevel + exhaustion, 40.0F); }
	bool needsFood() const { return _foodLevel < MAX_FOOD; }

	int	  getFoodLevel() const { return _foodLevel; }
	void  setFoodLevel(int food) { _foodLevel = food; }
	float getSaturationLevel() const { return _saturationLevel; }
	void  setSaturation(float saturation) { _saturationLevel = saturation; }
	float getExhaustionLevel() const { return _exhaustionLevel; }
	void  setExhaustion(float exhaustion) { _exhaustionLevel = exhaustion; }
	int	  getTickTimer() const { return _tickTimer; }
	void  setTickTimer(int ticks) { _tickTimer = ticks; }

  private:
	int	  _foodLevel	   = 20;   // foodLevel
	float _saturationLevel = 5.0F; // foodSaturationLevel
	float _exhaustionLevel = 0;	   // foodExhaustionLevel
	int	  _tickTimer	   = 0;	   // foodTickTimer
};

#endif
