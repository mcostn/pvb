#include "ui/app.hpp"
#include "ui/scale.hpp"

Error Window::Init(const char *title, int width, int height)
{
    FAIL_COND_V(!glfwInit(), Error::Failed);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    Handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
    FAIL_COND_V(!Handle, Error::Failed);

    glfwMakeContextCurrent(Handle);
    glfwSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui_ImplGlfw_InitForOpenGL(Handle, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    InitUiScale(kDefaultUiScale);

    return Error::Ok;
}

bool Window::BeginFrame()
{
    if (glfwWindowShouldClose(Handle))
        return false;

    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    return true;
}

void Window::EndFrame()
{
    ImGui::Render();

    int width, height;
    glfwGetFramebufferSize(Handle, &width, &height);

    glViewport(0, 0, width, height);

    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(Handle);
}

void Window::Shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    if (Handle) {
        glfwDestroyWindow(Handle);
        Handle = nullptr;
    }

    glfwTerminate();
}

Error StartApp()
{
    Window window;
    TRY(window.Init("PVB",1280,720));

    Editor editor(GetBlockRegistry());
    while (window.BeginFrame()) {
        editor.Draw();
        window.EndFrame();

        if (editor.QuitRequested)
            glfwSetWindowShouldClose(window.Handle, GLFW_TRUE);
    }

    window.Shutdown();

    return Error::Ok;
}
