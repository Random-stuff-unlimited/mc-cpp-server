#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "world/Level.hpp"
#include "world/World.hpp"

namespace Commands {
	// TimeArgument: a number of ticks, or with a unit: d (days of 24000 ticks), s (seconds of 20), t (ticks)
	bool parseDuration(const std::string& text, int64_t& ticks) {
		if (text.empty()) return false;
		double unit	  = 1.0;
		std::string number = text;
		char		last   = text.back();
		if (last == 'd' || last == 's' || last == 't') {
			unit   = last == 'd' ? 24000.0 : last == 's' ? 20.0 : 1.0;
			number = text.substr(0, text.size() - 1);
		}
		char*  end	 = nullptr;
		double value = std::strtod(number.c_str(), &end);
		if (number.empty() || end == number.c_str() || *end != '\0' || value < 0.0) return false;
		ticks = static_cast<int64_t>(std::lround(value * unit));
		return true;
	}

	void registerWeather() {
		const std::string syntax = "weather (clear|rain|thunder) [<duration>]";
		commandList().push_back({"weather", syntax, [syntax](Server& server, Player& player, const std::vector<std::string>& args) {
								  if (args.empty() || args.size() > 2) return usage(server, player, syntax);
								  int64_t duration = -1;
								  if (args.size() == 2 && (!parseDuration(args[1], duration) || duration < 1 || duration > 1000000)) {
									  return usage(server, player, syntax);
								  }
								  const std::string& mode = args[0];
								  if (mode == "clear") {
									  server.levelOf(player).world().setWeather(false, false, static_cast<int>(duration));
								  } else if (mode == "rain") {
									  server.levelOf(player).world().setWeather(true, false, static_cast<int>(duration));
								  } else if (mode == "thunder") {
									  server.levelOf(player).world().setWeather(true, true, static_cast<int>(duration));
								  } else {
									  return usage(server, player, syntax);
								  }
								  translated(server, player, "commands.weather.set." + mode);
							  }});
	}
} // namespace Commands
