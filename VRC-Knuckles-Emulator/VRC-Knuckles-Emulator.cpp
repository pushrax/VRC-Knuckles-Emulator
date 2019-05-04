#include "stdafx.h"
#include "EmbeddedFiles.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <openvr.h>
#include <direct.h>

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

struct Controller
{
	float fingerAxesRaw[4];
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
static vr::VRActionHandle_t actionHandles[2][4];
static vr::VRActionSetHandle_t actionSetHandle;

static char cwd[MAX_PATH];

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

	glfwIconifyWindow(glfwWindow);

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
	else if (!vr::VR_IsInterfaceVersionValid(vr::IVRSettings_Version))
	{
		throw std::runtime_error("OpenVR error: Outdated IVRSettings_Version");
	}
	else if (!vr::VR_IsInterfaceVersionValid(vr::IVROverlay_Version))
	{
		throw std::runtime_error("OpenVR error: Outdated IVROverlay_Version");
	}

	std::string manifestPath(cwd);
	manifestPath += "\\action_manifest.json";
	vr::VRInput()->SetActionManifestPath(manifestPath.c_str());

	vr::VRInput()->GetActionHandle("/actions/main/in/LeftFinger1", &actionHandles[0][0]);
	vr::VRInput()->GetActionHandle("/actions/main/in/LeftFinger2", &actionHandles[0][1]);
	vr::VRInput()->GetActionHandle("/actions/main/in/LeftFinger3", &actionHandles[0][2]);
	vr::VRInput()->GetActionHandle("/actions/main/in/LeftFinger4", &actionHandles[0][3]);
	vr::VRInput()->GetActionHandle("/actions/main/in/RightFinger1", &actionHandles[1][0]);
	vr::VRInput()->GetActionHandle("/actions/main/in/RightFinger2", &actionHandles[1][1]);
	vr::VRInput()->GetActionHandle("/actions/main/in/RightFinger3", &actionHandles[1][2]);
	vr::VRInput()->GetActionHandle("/actions/main/in/RightFinger4", &actionHandles[1][3]);

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

	ImGui::DragFloat4("Left hand", ctx.hands[0].fingerAxesRaw);
	ImGui::DragFloat4("Right hand", ctx.hands[1].fingerAxesRaw);

	ImGui::SetNextWindowPos(ImVec2(10.0f, ImGui::GetWindowHeight() - ImGui::GetItemsLineHeightWithSpacing()));
	ImGui::BeginChild("bottom line", ImVec2(ImGui::GetWindowWidth() - 20.0f, ImGui::GetItemsLineHeightWithSpacing() * 2), false);
	ImGui::Text("VRC Knuckles Emulator by tach/pushrax");
	ImGui::EndChild();

	ImGui::End();
}

void RunLoop()
{
	while (!glfwWindowShouldClose(glfwWindow))
	{
		int width, height;
		glfwGetFramebufferSize(glfwWindow, &width, &height);

		Context ctx;

		vr::VRActiveActionSet_t activeActionSet;
		activeActionSet.ulActionSet = actionSetHandle;
		activeActionSet.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;
		activeActionSet.nPriority = 0;
		vr::VRInput()->UpdateActionState(&activeActionSet, sizeof vr::VRActiveActionSet_t, 1);

		for (int hand = 0; hand < 2; hand++)
		{
			for (int finger = 0; finger < 4; finger++)
			{
				vr::InputAnalogActionData_t actionData;
				vr::VRInput()->GetAnalogActionData(actionHandles[hand][finger], &actionData, sizeof actionData, vr::k_ulInvalidInputValueHandle);
				ctx.hands[hand].fingerAxesRaw[finger] = actionData.x;
			}
		}

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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	_getcwd(cwd, MAX_PATH);
	//CreateConsole();

	if (!glfwInit())
	{
		MessageBox(nullptr, L"Failed to initialize GLFW", L"", 0);
		return 0;
	}

	glfwSetErrorCallback(GLFWErrorCallback);

	try {
		InitVR();
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

	if (glfwWindow)
		glfwDestroyWindow(glfwWindow);

	glfwTerminate();
	return 0;
}
