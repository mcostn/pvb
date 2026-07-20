#include "ui/editor.hpp"
#include "ui/imgui.hpp"
#include "ui/project.hpp"

#include "ImGuiFileDialog.h"

#include <iostream>


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
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (ShowPalette) {
        float height = ImGui::GetContentRegionAvail().y;
        Palette.Draw(CanvasView, Registry, "palette", height);
        ImGui::SameLine();
    }

    if (ShowCodeView) {
        ImVec2 avail = ImGui::GetContentRegionAvail();

        constexpr ImGuiTableFlags flags =
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_BordersInnerV;

        if (ImGui::BeginTable("##canvas_code_split", 2, flags, avail)) {
            ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch, 0.65f);
            ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthStretch, 0.35f);

            ImGui::TableNextRow(ImGuiTableRowFlags_None, avail.y);

            ImGui::TableSetColumnIndex(0);
            CanvasView.Draw("canvas", ImGui::GetContentRegionAvail());

            ImGui::TableSetColumnIndex(1);
            Code.Draw("codeview", ImGui::GetContentRegionAvail());

            ImGui::EndTable();
        }
    } else {
        CanvasView.Draw("canvas", ImGui::GetContentRegionAvail());
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

        if (ImGui::MenuItem("Compile")) {
            GenerateCode();
        }

        if (ImGui::MenuItem("Run")) {
        }

        if (ImGui::MenuItem("Compile and Run")) {
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
