#include "Logging.h"
#include "Hooking.h"
#include <openvr.h>
#include <openvr_capi.h>
#include <cmath>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../Protocol.h"

static protocol::SharedMemory Shm;

static bool (__stdcall *OriginalGetControllerState)(
	TrackedDeviceIndex_t index, VRControllerState_t *state, uint32_t stateSize
);

static bool (__stdcall *OriginalGetControllerStateWithPose)(
	ETrackingUniverseOrigin origin, TrackedDeviceIndex_t index,
	VRControllerState_t *state, uint32_t stateSize,
	struct TrackedDevicePose_t *pose
);

static Hook<void *(*)(const char *, vr::EVRInitError *)> GetGenericInterfaceHook("VR_GetGenericInterface");
static Hook<uint32_t(*)()> GetInitTokenHook("VR_GetInitToken");

const uint64_t TrackpadBit = 0x100000000, GripBit = 0x4;

static void ApplyGesture(int hand, Gesture gesture, VRControllerState_t *state)
{
	static bool prevMoving[2] = { false, false };

	float tx = state->rAxis[0].x, ty = state->rAxis[0].y;
	if (abs(tx) > 0.15 || abs(ty) > 0.15)
	{
		// disable gestures during movement since they both need the trackpad axis
		prevMoving[hand] = true;
		return;
	}

	// the default bindings have the thumbstick touched state mapped to the
	// trackpad pressed state, remove this so the thumbstick can be touched
	// during gestures
	state->ulButtonPressed &= ~TrackpadBit;

	if (gesture != Idle && gesture != OpenHand && gesture != Fist)
	{
		// touching the trackpad triggers gesture input
		state->ulButtonTouched |= TrackpadBit;
	}
	else
	{
		// remove touched state, otherwise touching the thumbstick causes the
		// fist gesture instead of idle
		state->ulButtonTouched &= ~TrackpadBit;
	}

	switch (gesture)
	{
	case OpenHand:
		state->ulButtonPressed |= GripBit;
		break;
	case Point:
		tx = 0.0f;
		ty = 1.0f;
		break;
	case Peace:
		tx = 0.7f;
		ty = 0.3f;
		break;
	case RockNRoll:
		tx = 0.5f;
		ty = -0.5f;
		break;
	case FingerGun:
		tx = -0.5f;
		ty = -0.5f;
		break;
	case ThumbsUp:
		tx = -0.7f;
		ty = 0.3f;
		break;
	}

	state->rAxis[0].x = hand ? tx : -tx;
	state->rAxis[0].y = ty;

	if (prevMoving[hand])
	{
		// when movement stops release trackpad for a frame, otherwise
		// trackpad input for gestures will trigger more movement
		state->ulButtonTouched &= ~TrackpadBit;
		prevMoving[hand] = false;
	}
}

static void HandleStateUpdate(TrackedDeviceIndex_t index, VRControllerState_t *state)
{
	protocol::Context context = *Shm.mappedContext;
	/*if (index == context.leftID)
		LOG("state: %llx %llx", state->ulButtonTouched, state->ulButtonPressed);*/

	if (index == context.leftID)
		ApplyGesture(0, context.leftGesture, state);
	else if (index == context.rightID)
		ApplyGesture(1, context.rightGesture, state);
}

static bool __stdcall DetourGetControllerState(TrackedDeviceIndex_t index, VRControllerState_t *state, uint32_t stateSize) 
{
	bool ok = OriginalGetControllerState(index, state, stateSize);
	if (ok && Shm.mappedContext)
	{
		HandleStateUpdate(index, state);
	}
	return ok;
}

static bool __stdcall DetourGetControllerStateWithPose(
	ETrackingUniverseOrigin origin, TrackedDeviceIndex_t index,
	VRControllerState_t *state, uint32_t stateSize,
	struct TrackedDevicePose_t *pose)
{
	bool ok = OriginalGetControllerStateWithPose(origin, index, state, stateSize, pose);
	if (ok && Shm.mappedContext)
	{
		HandleStateUpdate(index, state);
	}
	return ok;
}

static void *DetourGetGenericInterface(const char *name, vr::EVRInitError *err)
{
	void *result = GetGenericInterfaceHook.originalFunc(name, err);
	LOG("GetGenericInterface %s", name);
	if (strcmp(name, "FnTable:IVRSystem_019") == 0)
	{
		LOG("Hooking IVRSystem");
		auto vrSystem = (VR_IVRSystem_FnTable *)result;
		OriginalGetControllerState = vrSystem->GetControllerState;
		vrSystem->GetControllerState = &DetourGetControllerState;
		OriginalGetControllerStateWithPose = vrSystem->GetControllerStateWithPose;
		vrSystem->GetControllerStateWithPose = &DetourGetControllerStateWithPose;
	}
	return result;
}

static uint32_t DetourGetInitToken()
{
	uint32_t token = GetInitTokenHook.originalFunc();
	return token + 1; // force all interfaces to reload once
}

static void InjectHooks()
{
	auto err = MH_Initialize();
	if (err == MH_OK)
	{
		GetGenericInterfaceHook.CreateHook(&vr::VR_GetGenericInterface, &DetourGetGenericInterface);
		GetInitTokenHook.CreateHook(&vr::VR_GetInitToken, &DetourGetInitToken);
	}
	else
	{
		fprintf(stderr, "MH_Initialize error: %s", MH_StatusToString(err));
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		OpenLogFile();
		LOG("VRC-Knuckles-EmulatorDriver loaded");
		Shm.Create();
		InjectHooks();
		break;
	case DLL_PROCESS_DETACH:
		GetGenericInterfaceHook.Destroy();
		GetInitTokenHook.Destroy();
		Shm.Release();
		MH_Uninitialize();
		LOG("VRC-Knuckles-EmulatorDriver unloaded");
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

