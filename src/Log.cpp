#include "Log.h"

#define IM_VEC2_CLASS_EXTRA\
	operator glm::vec2() {return glm::vec2(x, y);}\
	ImVec2(glm::vec2& vec)\
		: x(vec.x), y(vec.y)\
	{}\
	const ImVec2(const glm::vec2& vec)\
		: x(vec.x), y(vec.y)\
	{}

#include <chrono>
#include <thread>

namespace logger {
	typedef std::chrono::nanoseconds RegionDuration;
	typedef std::chrono::time_point<std::chrono::steady_clock> RegionTimePoint;
	struct RegionTime {
		RegionTimePoint beginTime = RegionTimePoint(RegionDuration(-1));
		RegionTimePoint endTime = RegionTimePoint(RegionDuration(-1));
	};

	struct RegionTimeProfile {
		RegionTimeProfile* parentProfile = nullptr;
		std::vector<RegionTimeProfile> childProfiles = {};

		std::string region = "";
		RegionTime time = {};
	};

	std::vector<std::string> _regionStack;

	// time storage
	RegionTimePoint _programStartTime = std::chrono::high_resolution_clock::now();
	RegionTimeProfile _rootTimeProfile = {};
	RegionTimeProfile* _pCurrentTimeProfile = &_rootTimeProfile;

	// time gui
	RegionDuration _timelineValidDuration = std::chrono::seconds(10); // default to ten seconds to view

	void initLog() {
		_rootTimeProfile.region = "root";
		_rootTimeProfile.time.beginTime = std::chrono::high_resolution_clock::now();

		_regionStack = {};
	}

	void terminateLog() {}

	void beginRegion(std::string name) {
		_pCurrentTimeProfile->childProfiles.push_back({_pCurrentTimeProfile}); // make a new time profile in the tree
		_pCurrentTimeProfile = &_pCurrentTimeProfile->childProfiles.back();

		_pCurrentTimeProfile->region = name;
		_pCurrentTimeProfile->time.beginTime = std::chrono::high_resolution_clock::now(); // save the begin time
		_regionStack.push_back(name);
	}

	void endRegion() {
		_pCurrentTimeProfile->time.endTime = std::chrono::high_resolution_clock::now(); // save the end time
		_pCurrentTimeProfile = _pCurrentTimeProfile->parentProfile; // go back to parent

		_regionStack.pop_back();
	}

	void error(std::string msg) {
		for (std::string region : _regionStack)
			printf("%s -> ", region.c_str());
		fprintf(stderr, ":\nERROR: %s\n", msg.c_str());
	}
	void errorRaw(std::string msg) {
		fprintf(stderr, "%s\n", msg.c_str());
	}

	void warning(std::string msg) {
		for (std::string region : _regionStack)
			printf("%s -> ", region.c_str());
		printf(":\nWARNING: %s\n", msg.c_str());
	}
	void warningRaw(std::string msg) {
		printf("%s\n", msg.c_str());
	}

	void info(std::string msg) {
		for (std::string region : _regionStack)
			printf("%s -> ", region.c_str());
		printf(":\n%s\n", msg.c_str());
	}
	void infoRaw(std::string msg) {
		printf("%s\n", msg.c_str());
	}

	std::string getCurrentRegion() {
		return _regionStack.back();
	}

	const std::vector<std::string>& getRegionStack() {
		return _regionStack;
	}

	void setTimelineValidDuration(float duration) {
		_timelineValidDuration = std::chrono::seconds((int64_t)duration);
		printf("%lld\n", _timelineValidDuration.count());
	}

	void cleanProfile(RegionTimeProfile& profile, uint32_t index) {
		if (profile.time.endTime > RegionTimePoint(RegionDuration(0)) &&
			std::chrono::high_resolution_clock::now() - profile.time.endTime > _timelineValidDuration
		) {
			profile.parentProfile->childProfiles.erase(profile.parentProfile->childProfiles.begin() + index);
			return;
		}
		uint32_t i = 0;
		for (auto& childProfile : profile.childProfiles) {
			cleanProfile(childProfile, i);
			i++;
		}
	}

	void cleanTimeline() {
		uint32_t i = 0;
		for (auto& childProfile : _rootTimeProfile.childProfiles) {
			cleanProfile(childProfile, i);
			i++;
		}
	}
}