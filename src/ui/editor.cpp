#include "ui/editor.hpp"
#include "ui/imgui.hpp"
#include "ui/project.hpp"

#include "ImGuiFileDialog.h"

#include <algorithm>
#include <iostream>
#include <sstream>


Editor::Editor(BlockRegistry registry)
    : Registry(std::move(registry))
{
    CanvasView.EditorRef = this;
    CanvasView.Registry = &Registry;
}


void Editor::Draw()
{
    DrawMenuBar();
    DrawProjectSettingsPopup();

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::Begin(
        "##root",
        nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 avail = ImGui::GetContentRegionAvail();

    if (ShowOutputPanel) {
        constexpr float SplitterHeight = 4.0f;
        constexpr float MinMainContentHeight = 100.0f;

        OutputPanelHeight = std::min(OutputPanelHeight, avail.y - SplitterHeight - MinMainContentHeight);

        const float mainHeight = avail.y - OutputPanelHeight - SplitterHeight;
        DrawMainContent(ImVec2(avail.x, mainHeight));

        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));
        ImGui::Button("##output_splitter", ImVec2(-1.0f, SplitterHeight));
        if (ImGui::IsItemActive())
            OutputPanelHeight -= ImGui::GetIO().MouseDelta.y;
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        DrawOutputPanel(ImVec2(avail.x, OutputPanelHeight));
    } else {
        DrawMainContent(avail);
    }

    ImGui::End();

    DrawFileDialog();

    if (ShowPalette) {
        HandlePaletteDrag();
    }

    DrawNotifications();

    if (CanvasView.ShowDebugWindow) {
        ImGui::ShowMetricsWindow();
        CanvasView.DrawDebugWindow();
    }
}

void Editor::DrawMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save")) {
            if (ProjectPath.empty()) {
                ShowFileDialog(
                    "SaveProject",
                    "Save Project",
                    ".pvb",
                    [this](auto& path) { SaveTo(path); }
                );
            } else {
                SaveTo(ProjectPath);
            }
        }

        if (ImGui::MenuItem("Save As")) {
            ShowFileDialog(
                "SaveProject",
                "Save Project",
                ".pvb",
                [this](auto& path) { SaveTo(path); }
            );
        }

        if (ImGui::MenuItem("Open")) {
            ShowFileDialog(
                "OpenProject",
                "Open Project",
                ".pvb",
                [this](auto& path) { LoadFrom(path); }
            );
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Quit")) {
            // TODO: close application
        }

        ImGui::EndMenu();
    }


    if (ImGui::BeginMenu("Build")) {
        if (ImGui::MenuItem("Generate Code")) {
            GenerateCode();
        }

        if (ImGui::MenuItem("Run")) {
            CompileAndRunProject();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Project")) {
        if (ImGui::MenuItem("Settings")) {
            ShowProjectSettings = true;
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Palette", nullptr, &ShowPalette);
        ImGui::MenuItem("Generated Code", nullptr, &ShowCodeView);
        ImGui::MenuItem("Output", nullptr, &ShowOutputPanel);
        ImGui::MenuItem("Debug", nullptr, &CanvasView.ShowDebugWindow);

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void Editor::DrawProjectSettingsPopup()
{
    if (ShowProjectSettings) {
        ImGui::OpenPopup("Project Settings");
        ShowProjectSettings = false;
    }

    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Project Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::InputText("Name", &ProjectName);
    ImGui::InputTextMultiline("Description", &ProjectDescription, ImVec2(320.0f, 80.0f));

    static const char *kLanguageNames[] = { "C++", "Python" };
    int languageIndex = (Language == CodeLanguage::Cpp) ? 0 : 1;
    if (ImGui::Combo("Language", &languageIndex, kLanguageNames, IM_ARRAYSIZE(kLanguageNames)))
        Language = (languageIndex == 0) ? CodeLanguage::Cpp : CodeLanguage::Python;

    ImGui::Separator();

    if (ImGui::Button("Close", ImVec2(120, 0)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void Editor::BeginPaletteDrag(const BlockDefinition &def)
{
    Drag = {};
    Drag.Active = true;
    Drag.Definition = &def;

    auto ghost = std::make_unique<VisualBlock>();
    ghost->Def = &def;

    for (const auto &item : def.Schema) {
        ghost->Args[item.Name] = MakeDefaultArg(def, item);
    }

    CanvasView.LayoutBlock(*ghost);
    Drag.Ghost = std::move(ghost);
}

void Editor::HandlePaletteDrag()
{
    if (!Drag.Active)
        return;

    ImVec2 mouse = ImGui::GetIO().MousePos;

    VisualBlock &ghost = *Drag.Ghost;
    ImDrawList *draw = ImGui::GetForegroundDrawList();

    float zoom = CanvasView.Zoom;

    ImVec2 topLeft = mouse;
    BlockOutline outline = BuildOutline(ghost, topLeft, zoom);

    outline.Fill(draw, CategoryColor( ghost.Def->Category));
    outline.Stroke(draw, IM_COL32(0,0,0,150), 1.0f * zoom);

    DrawBlockLayout(
        draw,
        ghost,
        topLeft,
        zoom,
        ImGui::GetFont(),
        ImGui::GetFontSize() * zoom,
        IM_COL32(255,255,255,220),
        false);

    CanvasView.CurrentSnap = SnapResult{};

    if (CanvasView.Hovered) {
        ghost.Pos = CanvasView.ScreenToWorld(mouse, CanvasView.Origin);
        CanvasView.CurrentSnap = CanvasView.Manager.FindSnapTarget(&ghost, zoom);

        if (CanvasView.CurrentSnap.Block)
            CanvasView.DrawSnapPreview(draw, CanvasView.Origin);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (CanvasView.Hovered) {
            ImVec2 world = CanvasView.ScreenToWorld(mouse, CanvasView.Origin);
            VisualBlock *placed = CanvasView.AddBlock(*Drag.Definition, world);

            if (placed && CanvasView.CurrentSnap.Block) {
                switch (CanvasView.CurrentSnap.Type) {
                    case SnapType::Append:
                        CanvasView.Manager.AttachAfter(CanvasView.CurrentSnap.Block, placed);
                        break;

                    case SnapType::Prepend:
                        CanvasView.Manager.AttachBefore(CanvasView.CurrentSnap.Block, placed);
                        break;

                    case SnapType::EnterBody:
                        CanvasView.Manager.AttachToBody(CanvasView.CurrentSnap.Block, CanvasView.CurrentSnap.Slot, placed);
                        break;

                    case SnapType::EnterArg:
                        CanvasView.Manager.AttachToArg(CanvasView.CurrentSnap.Block, CanvasView.CurrentSnap.Slot, placed);
                        break;

                    default:
                        break;
                }
            }
        }

        CanvasView.CurrentSnap = SnapResult{};
        Drag = {};
    }
}

void Editor::SaveTo(const std::string& path)
{
    ProjectSettings settings { ProjectName, ProjectDescription, Language };
    Error err = SaveProject(CanvasView, Registry, path, settings);

    if (err == Error::Ok) {
        ProjectPath = path;
        Notify("Saved Project: " + ProjectPath);
    } else {
        Notify("Save failed: " + std::string(to_string(err)), true);
    }
}

void Editor::LoadFrom(const std::string& path)
{
    ProjectSettings settings;
    Error err = LoadProject(CanvasView, Registry, path, settings);

    if (err == Error::Ok) {
        ProjectPath = path;
        ProjectName = settings.Name;
        ProjectDescription = settings.Description;
        Language = settings.Language;
        Notify("Loaded project: " + ProjectPath);
    } else {
        Notify("Load failed: " + std::string(to_string(err)), true);
    }
}

void Editor::DrawFileDialog()
{
    if (ActiveDialog.empty())
        return;

    if (ImGuiFileDialog::Instance()->Display(
            ActiveDialog.c_str(),
            ImGuiWindowFlags_NoCollapse,
            ImVec2(900, 600))) {

        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path =
                ImGuiFileDialog::Instance()->GetFilePathName();

            FileDialogCallback(path);
        }

        ImGuiFileDialog::Instance()->Close();

        ActiveDialog.clear();
        FileDialogCallback = {};
    }
}

void Editor::ShowFileDialog(
    const char* id,
    const char* title,
    const char* extension,
    std::function<void(const std::string&)> callback)
{
    ActiveDialog = id;
    FileDialogCallback = callback;

    IGFD::FileDialogConfig config;
    config.path = ".";
    config.flags = ImGuiFileDialogFlags_Modal;

    ImGuiFileDialog::Instance()->OpenDialog(
            id,
            title,
            extension,
            config);
}

void Editor::GenerateCode()
{
    Error err = Code.Generate(CanvasView, Registry, Language);

    if (err == Error::Ok) {
        ShowCodeView = true;
        Notify("Code generated successfully");
    } else {
        Notify("Code generation failed: " + std::string(to_string(err)), true);
    }
}

static std::string BuildResultSummary(const BuildResult &result)
{
    std::ostringstream summary;
    summary << to_string(result.Status);

    if (result.Tool)
        summary << " (" << result.Tool->Command << ")";

    if (result.ExitCode != 0)
        summary << " [exit " << result.ExitCode << "]";

    return summary.str();
}

void Editor::ShowBuildResult(const std::string &title, const BuildResult &result)
{
    std::string text = result.Output;

    if (text.empty()) text = BuildResultSummary(result);
    else if (result.Status != Error::Ok) text = BuildResultSummary(result) + "\n\n" + text;

    SetOutput(title, text);

    if (result.Status == Error::Ok) Notify(title + " succeeded");
    else Notify(title + " failed: " + std::string(to_string(result.Status)), true);
}

void Editor::SetOutput(const std::string &title, const std::string &text)
{
    OutputTitle = title;
    OutputText = text;
    ShowOutputPanel = true;
    OutputScrollToBottom = true;
}

void Editor::DrawMainContent(const ImVec2 &size)
{
    ImGui::BeginChild("##main_content", size, false, ImGuiWindowFlags_NoScrollbar);

    const int columnCount = 1 + (ShowPalette ? 1 : 0) + (ShowCodeView ? 1 : 0);

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersInnerV;

    ImVec2 avail = ImGui::GetContentRegionAvail();

    if (ImGui::BeginTable("##main_layout", columnCount, flags, avail)) {
        if (ShowPalette)
            ImGui::TableSetupColumn("Palette", ImGuiTableColumnFlags_WidthFixed, PaletteWidth);

        ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch, 0.65f);

        if (ShowCodeView)
            ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthFixed, CodeViewWidth);

        ImGui::TableNextRow(ImGuiTableRowFlags_None, avail.y);

        if (ShowPalette) {
            ImGui::TableSetColumnIndex(0);
            Palette.Width = ImGui::GetContentRegionAvail().x;
            Palette.Draw(CanvasView, Registry, "palette", ImGui::GetContentRegionAvail().y);
        }

        ImGui::TableSetColumnIndex(ShowPalette ? 1 : 0);
        CanvasView.Draw("canvas", ImGui::GetContentRegionAvail());

        if (ShowCodeView) {
            ImGui::TableSetColumnIndex(ShowPalette ? 2 : 1);
            CodeViewWidth = ImGui::GetContentRegionAvail().x;
            Code.HighlightBlock(CanvasView.HoveredBlockId);
            Code.Draw("codeview", ImGui::GetContentRegionAvail());
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void Editor::DrawOutputPanel(const ImVec2 &size)
{
    ImGui::BeginChild(
        "##output_panel",
        size,
        true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::TextUnformatted(OutputTitle.empty() ? "Output" : OutputTitle.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
    if (ImGui::SmallButton("Clear"))
        OutputText.clear();

    ImGui::Separator();

    ImGui::BeginChild(
        "##output_text",
        ImVec2(0, 0),
        false,
        ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleColor(
        ImGuiCol_Text,
        ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::TextUnformatted(OutputText.c_str());
    ImGui::PopStyleColor();

    if (OutputScrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        OutputScrollToBottom = false;
    }

    ImGui::EndChild();
    ImGui::EndChild();
}

void Editor::CompileAndRunProject()
{
    Error err = Code.Generate(CanvasView, Registry, Language);
    if (err != Error::Ok) {
        SetOutput("Code Generation", "Code generation failed: " + std::string(to_string(err)));
        Notify("Code generation failed: " + std::string(to_string(err)), true);
        return;
    }

    ShowCodeView = true;

    const std::string projectName = ProjectName.empty() ? "program" : ProjectName;
    BuildResult result = ProjectRunner::CompileAndRunInTerminal(Code.GetCode(), Language, projectName);
    ShowBuildResult("Run", result);
}

void Editor::Notify(const std::string &text, bool error)
{
    Notifications.push_back({ text, 3.0f, error });
}

void Editor::DrawNotifications()
{
    constexpr float Padding = 20.0f;
    constexpr float Spacing = 10.0f;
    float offset = 0.0f;

    int id = 0;
    for (auto it = Notifications.begin(); it != Notifications.end();) {
        ImGui::SetNextWindowPos(
                ImVec2(
                    ImGui::GetIO().DisplaySize.x - Padding,
                    ImGui::GetIO().DisplaySize.y - Padding - offset),
                ImGuiCond_Always,
                ImVec2(1.0f, 1.0f));

        ImGui::SetNextWindowSizeConstraints(
                ImVec2(200, 0),
                ImVec2(400, FLT_MAX));

        ImGui::SetNextWindowBgAlpha(0.85f);

        ImGui::Begin(
                ("##notification_" + std::to_string(id++)).c_str(),
                nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings);

        if (it->Error)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
        ImGui::TextWrapped("%s", it->Text.c_str());
        if (it->Error)
            ImGui::PopStyleColor();

        float height = ImGui::GetWindowHeight();

        ImGui::End();

        offset += height + Spacing;

        it->Time -= ImGui::GetIO().DeltaTime;
        if (it->Time <= 0.0f) it = Notifications.erase(it);
        else ++it;
    }
}
