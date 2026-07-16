#pragma once

#include "block/registry.hpp"
#include "ui/canvas.hpp"
#include "ui/palette.hpp"
#include "ui/code_view.hpp"

struct PaletteDragState
{
    bool Active = false;
    const BlockDefinition *Definition = nullptr;
    std::unique_ptr<VisualBlock> Ghost;
};

struct EditorNotification
{
    std::string Text;
    float Time = 0.0f;
    bool Error = false;
};

class Editor
{
public:
    explicit Editor(BlockRegistry registry);

    void Draw();
    void DrawMenuBar();

    bool ShowPalette = true;
    BlockPalette Palette;
    PaletteDragState Drag;
    void BeginPaletteDrag(const BlockDefinition &def);
    void HandlePaletteDrag();

    Canvas CanvasView;
    BlockRegistry Registry;

    std::string ProjectPath;
    std::string ProjectName;
    std::string ProjectDescription;

    void SaveTo(const std::string& path);
    void LoadFrom(const std::string& path);

    bool ShowProjectSettings = false;
    void DrawProjectSettingsPopup();

    std::string ActiveDialog;
    std::function<void(const std::string&)> FileDialogCallback;
    void DrawFileDialog();
    void ShowFileDialog(
        const char* id,
        const char* title,
        const char* extension,
        std::function<void(const std::string&)> callback);

    std::deque<EditorNotification> Notifications;
    void Notify(const std::string &text, bool error = false);
    void DrawNotifications();

    CodeView Code;
    CodeLanguage Language = CodeLanguage::Python;
    bool ShowCodeView = false;
    void GenerateCode();
};
