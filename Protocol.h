#pragma once

#include <cstdint>

#define VRC_KNUCKLES_EMULATOR_SHM_NAME "Local\\VRCKnucklesEmulatorSHM"

enum Gesture
{
	Idle,
	Fist,
	OpenHand,
	Point,
	Peace,
	RockNRoll,
	FingerGun,
	ThumbsUp,
};

static const char *GestureToString(Gesture g)
{
	switch (g) {
	case Idle: return "Idle";
	case Fist: return "Fist";
	case OpenHand: return "OpenHand";
	case Point: return "Point";
	case Peace: return "Peace";
	case RockNRoll: return "RockNRoll";
	case FingerGun: return "FingerGun";
	case ThumbsUp: return "ThumbsUp";
	}
	return "???";
}

namespace protocol
{
	struct Context
	{
		volatile uint32_t leftID = ~0, rightID = ~0;
		volatile Gesture leftGesture = Idle, rightGesture = Idle;
	};

	class SharedMemory
	{
	public:
		void Create()
		{
			fileMapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof *mappedContext, VRC_KNUCKLES_EMULATOR_SHM_NAME);
			if (fileMapping == nullptr)
			{
#ifdef LOG
				LOG("Failed to create memory mapping. Error: %d", GetLastError());
#else
				throw std::runtime_error("Failed to create memory mapping. Error: " + std::to_string(GetLastError()));
#endif
				return;
			}

			mappedContext = (protocol::Context *)MapViewOfFile(fileMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof *mappedContext);
			if (mappedContext == nullptr)
			{
				CloseHandle(fileMapping);
				fileMapping = nullptr;
#ifdef LOG
				LOG("Failed to map memory. Error %d", GetLastError());
#else
				throw std::runtime_error("Failed to map memory. Error: " + std::to_string(GetLastError()));
#endif
				return;
			}
		}

		void Release()
		{
			if (mappedContext)
			{
				UnmapViewOfFile(mappedContext);
				mappedContext = nullptr;
			}

			if (fileMapping)
			{
				CloseHandle(fileMapping);
				fileMapping = nullptr;
			}
		}

		Context *mappedContext = nullptr;

	private:
		HANDLE fileMapping = nullptr;
	};
}
