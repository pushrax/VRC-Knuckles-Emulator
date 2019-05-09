#include "stdafx.h"
#include "EmbeddedFiles.h"

#include <string>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <openvr.h>
#include <direct.h>

#include "../Protocol.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static protocol::SharedMemory Shm;
static std::string WorkingDir;
static DWORD InjectedPID = 0;
static int TicksSinceWindowAppeared = 0;

struct Controller
{
	float fingerAxesRaw[4];
	int fingerState[4];
	bool thumbState;
	Gesture gesture;
};

struct Context
{
	Controller hands[2];
};

void BuildMainWindow(Context &ctx);

void CreateConsole()
{
	static bool created = false;
	if (!created)
	{
		AllocConsole();
		FILE *file = nullptr;
		freopen_s(&file, "CONIN$", "r", stdin);
		freopen_s(&file, "CONOUT$", "w", stdout);
		freopen_s(&file, "CONOUT$", "w", stderr);
		created = true;
	}
}

void GLFWErrorCallback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}


static GLFWwindow *glfwWindow = nullptr;
static vr::VRActionHandle_t actionHandles[2][5];
static vr::VRActionSetHandle_t actionSetHandle;

void CreateGLFWWindow()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, false);

	glfwWindow = glfwCreateWindow(400, 400, "VRC Knuckles Emulator", NULL, NULL);
	if (!glfwWindow)
		throw std::runtime_error("Failed to create window");

	glfwMakeContextCurrent(glfwWindow);
	glfwSwapInterval(0);
	gl3wInit();

	//glfwIconifyWindow(glfwWindow);

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.IniFilename = nullptr;
	io.Fonts->AddFontFromMemoryCompressedTTF(DroidSans_compressed_data, DroidSans_compressed_size, 24.0f);

	ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	ImGui::StyleColorsDark();
}

void InitVR()
{
	auto initError = vr::VRInitError_None;
	vr::VR_Init(&initError, vr::VRApplication_Other);
	if (initError != vr::VRInitError_None)
	{
		auto error = vr::VR_GetVRInitErrorAsEnglishDescription(initError);
		throw std::runtime_error("OpenVR error:" + std::string(error));
	}

	if (!vr::VR_IsInterfaceVersionValid(vr::IVRSystem_Version))
	{
		throw std::runtime_error("OpenVR error: Outdated IVRSystem_Version");
	}
	else if (!vr::VR_IsInterfaceVersionValid(vr::IVRInput_Version))
	{
		throw std::runtime_error("OpenVR error: Outdated IVRInput_Version");
	}

	auto manifestPath = WorkingDir + "\\action_manifest.json";
	vr::VRInput()->SetActionManifestPath(manifestPath.c_str());

	vr::VRInput()->GetActionHandle("/actions/main/in/LeftThumb", &actionHandles[0][0]);
	vr::VRInput()->GetActionHandle("/actions/main/in/LeftFinger1", &actionHandles[0][1]);
	vr::VRInput()->GetActionHandle("/actions/main/in/LeftFinger2", &actionHandles[0][2]);
	vr::VRInput()->GetActionHandle("/actions/main/in/LeftFinger3", &actionHandles[0][3]);
	vr::VRInput()->GetActionHandle("/actions/main/in/LeftFinger4", &actionHandles[0][4]);
	vr::VRInput()->GetActionHandle("/actions/main/in/RightThumb", &actionHandles[1][0]);
	vr::VRInput()->GetActionHandle("/actions/main/in/RightFinger1", &actionHandles[1][1]);
	vr::VRInput()->GetActionHandle("/actions/main/in/RightFinger2", &actionHandles[1][2]);
	vr::VRInput()->GetActionHandle("/actions/main/in/RightFinger3", &actionHandles[1][3]);
	vr::VRInput()->GetActionHandle("/actions/main/in/RightFinger4", &actionHandles[1][4]);

	vr::VRInput()->GetActionSetHandle("/actions/main", &actionSetHandle);
}

void BuildMainWindow(Context &ctx)
{
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove;

	auto &io = ImGui::GetIO();

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(io.DisplaySize, ImGuiSetCond_Always);

	if (!ImGui::Begin("MainWindow", nullptr, windowFlags))
	{
		ImGui::End();
		return;
	}

	ImGui::DragFloat4(" Left Hand", ctx.hands[0].fingerAxesRaw);
	ImGui::DragFloat4(" Right Hand", ctx.hands[1].fingerAxesRaw);

	ImGui::Text("Left Gesture: %s", GestureToString(ctx.hands[0].gesture));
	ImGui::Text("Right Gesture: %s", GestureToString(ctx.hands[1].gesture));

	if (InjectedPID)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
		ImGui::Text("Loaded into process: %d", InjectedPID);
	}
	else if (TicksSinceWindowAppeared)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7, 0.6, 0, 1));
		ImGui::Text("Waiting for game to load: %d", 10 - TicksSinceWindowAppeared);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.5, 0.5, 1));
		ImGui::Text("Waiting for game to load");
	}
	ImGui::PopStyleColor();

	ImGui::SetNextWindowPos(ImVec2(10.0f, ImGui::GetWindowHeight() - ImGui::GetItemsLineHeightWithSpacing()));
	ImGui::BeginChild("bottom line", ImVec2(ImGui::GetWindowWidth() - 20.0f, ImGui::GetItemsLineHeightWithSpacing() * 2), false);
	ImGui::Text("VRC Knuckles Emulator by tach/pushrax");
	ImGui::EndChild();

	ImGui::End();
}

const float thresholds[4][2] = {
	{0.05, 0.95},
	{0.1, 0.8},
	{0.05, 0.8},
	{0.4, 0.8},
};

void InferGesture(Controller &ctl)
{
	for (int i = 0; i < 4; i++)
	{
		float raw = ctl.fingerAxesRaw[i];
		ctl.fingerState[i] = raw < thresholds[i][0] ? 0 : (raw < thresholds[i][1] ? 1 : 2);
	}

	auto &state = ctl.fingerState;
	Gesture gesture = Idle;

	if (state[0] == 0) {
		if (state[1]) {
			if (state[3]) {
				gesture = ctl.thumbState ? Point : FingerGun;
			} else {
				gesture = RockNRoll;
			}
		} else if (state[2]) {
			gesture = Peace;
		} else if (!state[3]) {
			gesture = OpenHand;
		}
	} else if (state[0] == 2 && state[2]) {
		gesture = ctl.thumbState ? Fist : ThumbsUp;
	}

	ctl.gesture = gesture;
}

void UpdateControllerIDs()
{
	static uint32_t lastIDs[2] = { ~0, ~0 };

	if (!Shm.mappedContext)
		return;

	uint32_t ids[2] = {
		vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),
		vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand),
	};

	if (ids[0] != lastIDs[0] || ids[1] != lastIDs[1])
	{
		printf("Updating controller IDs: %d %d\n", ids[0], ids[1]);

		lastIDs[0] = Shm.mappedContext->leftID = ids[0];
		lastIDs[1] = Shm.mappedContext->rightID = ids[1];
	}
}

void UpdateGestures(Context &ctx)
{
	static Gesture lastGestures[2] = { Idle, Idle };

	if (!Shm.mappedContext)
		return;

	if (ctx.hands[0].gesture != lastGestures[0])
	{
		lastGestures[0] = Shm.mappedContext->leftGesture = ctx.hands[0].gesture;
	}

	if (ctx.hands[1].gesture != lastGestures[1])
	{
		lastGestures[1] = Shm.mappedContext->rightGesture = ctx.hands[1].gesture;
	}
}

void LoadDLL();

void RunLoop()
{
	while (!glfwWindowShouldClose(glfwWindow))
	{
		LoadDLL();

		int width, height;
		glfwGetFramebufferSize(glfwWindow, &width, &height);
		UpdateControllerIDs();

		Context ctx;

		vr::VRActiveActionSet_t activeActionSet;
		activeActionSet.ulActionSet = actionSetHandle;
		activeActionSet.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;
		activeActionSet.nPriority = 0;
		vr::VRInput()->UpdateActionState(&activeActionSet, sizeof vr::VRActiveActionSet_t, 1);

		vr::InputAnalogActionData_t analogActionData;
		vr::InputDigitalActionData_t digitalActionData;

		for (int hand = 0; hand < 2; hand++)
		{
			vr::VRInput()->GetDigitalActionData(actionHandles[hand][0], &digitalActionData, sizeof digitalActionData, vr::k_ulInvalidInputValueHandle);
			ctx.hands[hand].thumbState = digitalActionData.bState;

			for (int finger = 0; finger < 4; finger++)
			{
				vr::VRInput()->GetAnalogActionData(actionHandles[hand][finger + 1], &analogActionData, sizeof analogActionData, vr::k_ulInvalidInputValueHandle);
				ctx.hands[hand].fingerAxesRaw[finger] = analogActionData.x;
			}
		}

		InferGesture(ctx.hands[0]);
		InferGesture(ctx.hands[1]);
		UpdateGestures(ctx);

		if (width && height)
		{
			ImGui::GetIO().DisplaySize = ImVec2((float)width, (float)height);

			ImGui_ImplGlfw_SetReadMouseFromGlfw(true);
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			BuildMainWindow(ctx);

			ImGui::Render();

			glViewport(0, 0, width, height);
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			glfwSwapBuffers(glfwWindow);
		}

		glfwWaitEventsTimeout(1 / 90.0);
	}
}

void LoadDLL()
{
	static double lastLoadAttempt = 0.0;

	double time = glfwGetTime();
	if (time - lastLoadAttempt < 1.0)
		return;

	lastLoadAttempt = time;

	static bool windowExistedAtBoot = true;

	auto hwnd = FindWindow(nullptr, L"VRChat");
	if (!hwnd)
	{
		windowExistedAtBoot = false;
		InjectedPID = 0;
		return;
	}

	DWORD pid;
	GetWindowThreadProcessId(hwnd, &pid);

	if (pid == InjectedPID)
		return;

	// wait a few seconds before injecting
	if (!windowExistedAtBoot && TicksSinceWindowAppeared++ < 10)
		return;

	printf("Ready to inject DLL in process %d\n", pid);
	TicksSinceWindowAppeared = 0;

	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
	if (!process)
		return;

	void *loadLibraryAddr = GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
	if (!loadLibraryAddr)
		throw std::runtime_error("Failed to get address of LoadLibraryA");

	auto dllPath = WorkingDir + "\\..\\x64\\Release\\vrc_knuckles_emulator.dll";

	if (GetFileAttributesA(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES)
		dllPath = WorkingDir + "\\vrc_knuckles_emulator.dll";

	void *remoteDLLPath = VirtualAllocEx(process, NULL, dllPath.size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	int bytesWritten = WriteProcessMemory(process, remoteDLLPath, dllPath.c_str(), dllPath.size(), NULL);
	if (!bytesWritten)
		throw std::runtime_error("Failed to write remote DLL path");

	HANDLE threadID = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteDLLPath, NULL, NULL);
	if (!threadID)
		throw std::runtime_error("Failed to create remote thread");

	InjectedPID = pid;
	CloseHandle(process);
	printf("Loaded DLL in process %d\n", pid);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	char cwd[MAX_PATH];
	_getcwd(cwd, MAX_PATH);
	WorkingDir = std::string(cwd);

	//CreateConsole();
	printf("Starting in cwd: %s\n", cwd);

	if (!glfwInit())
	{
		MessageBox(nullptr, L"Failed to initialize GLFW", L"", 0);
		return 0;
	}

	glfwSetErrorCallback(GLFWErrorCallback);

	try {
		InitVR();

		Shm.Create();
		*Shm.mappedContext = protocol::Context();

		CreateGLFWWindow();
		RunLoop();

		vr::VR_Shutdown();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
	catch (std::runtime_error &e)
	{
		std::cerr << "Runtime error: " << e.what() << std::endl;
		wchar_t message[1024];
		swprintf(message, 1024, L"%hs", e.what());
		MessageBox(nullptr, message, L"Runtime Error", 0);
	}

	Shm.Release();

	if (glfwWindow)
		glfwDestroyWindow(glfwWindow);

	glfwTerminate();
	return 0;
}
