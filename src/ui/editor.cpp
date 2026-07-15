#include "ui/editor.hpp"
#include "ui/imgui.hpp"
#include "ui/project_ini.hpp"

#include "ImGuiFileDialog.h"

#include <iostream>


Editor::Editor(BlockRegistry registry)
    : Registry(std::move(registry))
{
    CanvasView.EditorRef = this;
}


void Editor::Draw()
{
    DrawMenuBar();

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

    float height = ImGui::GetContentRegionAvail().y;
    Palette.Draw(CanvasView, Registry, "palette", height);

    ImGui::SameLine();

    CanvasView.Draw("canvas", ImGui::GetContentRegionAvail());

    ImGui::End();

    DrawSaveProjectDialog();
    DrawOpenProjectDialog();

    HandlePaletteDrag();

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
                SaveProjectAs();
            } else {
                DISCARD(SaveProject(
                    CanvasView,
                    ProjectPath,
                    ProjectName));
            }
        }

        if (ImGui::MenuItem("Save As")) {
            SaveProjectAs();
        }

        if (ImGui::MenuItem("Open")) {
            OpenProject();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Quit")) {
            // TODO: close application
        }

        ImGui::EndMenu();
    }



    if (ImGui::BeginMenu("Build")) {
        if (ImGui::MenuItem("Compile")) {
        }

        if (ImGui::MenuItem("Run")) {
        }

        if (ImGui::MenuItem("Compile and Run")) {
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Generated Code")) {
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
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

    ImVec2 topLeft = mouse;
    BlockOutline outline = BuildOutline(ghost, topLeft, 1.0f);

    outline.Fill(draw, CategoryColor( ghost.Def->Category));
    outline.Stroke(draw, IM_COL32(0,0,0,150), 1.0f);

    DrawBlockLayout(
        draw,
        ghost,
        topLeft,
        1.0f,
        ImGui::GetFont(),
        ImGui::GetFontSize(),
        IM_COL32(255,255,255,220),
        false);

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (CanvasView.Hovered) {
            ImVec2 world = CanvasView.ScreenToWorld(mouse, CanvasView.Origin);
            CanvasView.AddBlock(*Drag.Definition, world);
        }

        Drag = {};
    }
}

void Editor::SaveProjectAs()
{
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.flags = ImGuiFileDialogFlags_Modal;

    ImGuiFileDialog::Instance()->OpenDialog(
        "SaveProject",
        "Save Project",
        ".pvb",
        config);
}

void Editor::OpenProject()
{
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.flags = ImGuiFileDialogFlags_Modal;

    ImGuiFileDialog::Instance()->OpenDialog(
            "OpenProject",
            "Open Project",
            ".pvb",
            config);
}

void Editor::DrawOpenProjectDialog()
{
    if (ImGuiFileDialog::Instance()->Display(
                "OpenProject",
                ImGuiWindowFlags_NoCollapse,
                ImVec2(900, 600))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();

            if (LoadProject(CanvasView, filePath) == Error::Ok) {
                ProjectPath = filePath;
            }
        }

        ImGuiFileDialog::Instance()->Close();
    }
}

void Editor::DrawSaveProjectDialog()
{
    if (ImGuiFileDialog::Instance()->Display(
                "SaveProject",
                ImGuiWindowFlags_NoCollapse,
                ImVec2(900, 600))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            DISCARD(SaveProject(CanvasView, path, ProjectName));
            ProjectPath = path;
        }

        ImGuiFileDialog::Instance()->Close();
    }
}
