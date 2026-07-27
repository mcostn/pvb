#include <algorithm>
#include <functional>

#include "ui/canvas.hpp"
#include "ui/const.hpp"
#include "ui/custom_block.hpp"
#include "block/registry.hpp"
#include "util/macro.hpp"
#include "util/error.hpp"

static void CollectBlocksMatching(
        VisualBlock *block,
        const std::function<bool(const VisualBlock &)> &pred,
        std::vector<VisualBlock *> &out)
{
    if (!block)
        return;

    if (pred(*block)) {
        out.push_back(block);
        return;
    }

    for (auto &[key, arg] : block->Args) {
        if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg))
            CollectBlocksMatching(held->get(), pred, out);
    }
}

static void ClearVariableRefs(VisualBlock *block, const std::string &name)
{
    if (!block)
        return;

    for (auto &[key, arg] : block->Args) {
        if (auto *ref = std::get_if<VariableRef>(&arg)) {
            if (ref->Name != name)
                continue;

            const BlockSchemaItem *item = nullptr;
            if (block->Def) {
                for (const BlockSchemaItem &schemaItem : block->Def->Schema) {
                    if (schemaItem.Name == key) {
                        item = &schemaItem;
                        break;
                    }
                }
            }

            arg = (item && block->Def)
                ? MakeDefaultArg(*block->Def, *item)
                : VisualArg{ LiteralValue{ std::in_place_type<int>, 0 } };

            continue;
        }

        if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg))
            ClearVariableRefs(held->get(), name);
    }
}

static void RenameVariableRefs(VisualBlock *block, const std::string &oldName, const std::string &newName)
{
    if (!block)
        return;

    for (auto &[key, arg] : block->Args) {
        if (auto *ref = std::get_if<VariableRef>(&arg)) {
            if (ref->Name == oldName)
                ref->Name = newName;
            continue;
        }

        if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg))
            RenameVariableRefs(held->get(), oldName, newName);
    }
}

void Canvas::DeleteVariable(const std::string &name)
{
    if (!Registry || !Registry->HasVariable(name))
        return;

    const std::string getOp = VarGetOpCode(name);
    const std::string setOp = VarSetOpCode(name);

    auto isVariableBlock = [&](const VisualBlock &b) {
        return b.Def && (b.Def->OpCode == getOp || b.Def->OpCode == setOp);
    };

    std::vector<VisualBlock *> toDelete;
    for (auto &blockPtr : Manager.Blocks)
        CollectBlocksMatching(blockPtr.get(), isVariableBlock, toDelete);

    for (auto &blockPtr : Manager.Blocks)
        ClearVariableRefs(blockPtr.get(), name);

    for (VisualBlock *block : toDelete)
        Manager.DeleteBlock(block);

    DISCARD(Registry->RemoveVariable(name));
}

Error Canvas::RenameVariable(const std::string &oldName, const std::string &newName)
{
    if (!Registry)
        return Error::Failed;

    Error err = Registry->RenameVariable(oldName, newName);
    if (err != Error::Ok)
        return err;

    for (auto &blockPtr : Manager.Blocks)
        RenameVariableRefs(blockPtr.get(), oldName, newName);

    return Error::Ok;
}

void Canvas::RequestVariableCreation(VisualBlock *targetBlock, const std::string &targetKey, Value requiredType)
{
    VarCreateRequest.Requested = true;
    VarCreateRequest.TargetBlock = targetBlock;
    VarCreateRequest.TargetKey = targetKey;
    VarCreateRequest.RequiredType = requiredType;

    static const Value kTypesInOrder[] = { VAL_INT, VAL_FLOAT, VAL_BOOL, VAL_STRING };

    NewVarTypeIndex = 0;
    for (int i = 0; i < 4; ++i) {
        if (requiredType & kTypesInOrder[i]) {
            NewVarTypeIndex = i;
            break;
        }
    }

    NewVarNameBuf[0] = '\0';
    NewVarError.clear();
}

void Canvas::DrawCreateVariablePopup()
{
    static const char *kAllNames[]  = { "Int", "Float", "Bool", "String" };
    static const Value  kAllValues[] = { VAL_INT, VAL_FLOAT, VAL_BOOL, VAL_STRING };

    if (VarCreateRequest.Requested) {
        ImGui::OpenPopup("##canvas_create_variable_popup");
        VarCreateRequest.Requested = false;
    }

    if (!ImGui::BeginPopup("##canvas_create_variable_popup"))
        return;

    const char *allowedNames[4];
    Value allowedValues[4];
    int allowedCount = 0;

    for (int i = 0; i < 4; ++i) {
        if (VarCreateRequest.RequiredType & kAllValues[i]) {
            allowedNames[allowedCount] = kAllNames[i];
            allowedValues[allowedCount] = kAllValues[i];
            allowedCount++;
        }
    }

    if (allowedCount == 0) {
        allowedNames[0] = kAllNames[0];
        allowedValues[0] = kAllValues[0];
        allowedCount = 1;
    }

    NewVarTypeIndex = std::clamp(NewVarTypeIndex, 0, allowedCount - 1);

    ImGui::TextUnformatted("New Variable");
    ImGui::Separator();

    ImGui::SetNextItemWidth(kPopupFieldWidth);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere(0);
    bool enterPressed = ImGui::InputTextWithHint(
            "##new_var_name",
            "name",
            NewVarNameBuf,
            sizeof(NewVarNameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::SetNextItemWidth(kPopupFieldWidth);
    ImGui::Combo("##new_var_type", &NewVarTypeIndex, allowedNames, allowedCount);

    if (!NewVarError.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kPopupFieldWidth);
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", NewVarError.c_str());
        ImGui::PopTextWrapPos();
    }

    bool createClicked = ImGui::Button("Create", ImVec2(PopupButtonWidth(kPopupFieldWidth), 0.0f));
    ImGui::SameLine();
    bool cancelClicked = ImGui::Button("Cancel", ImVec2(PopupButtonWidth(kPopupFieldWidth), 0.0f));

    if (createClicked || enterPressed) {
        std::string name(NewVarNameBuf);
        Value type = allowedValues[NewVarTypeIndex];

        Error err = Registry->AddVariable(name, type);

        if (err == Error::Ok) {
            if (VarCreateRequest.TargetBlock && !VarCreateRequest.TargetKey.empty())
                VarCreateRequest.TargetBlock->Args[VarCreateRequest.TargetKey] = VisualArg{ VariableRef{ name, type } };

            NewVarError.clear();
            NewVarNameBuf[0] = '\0';
            VarCreateRequest = PendingVariableCreate{};
            ImGui::CloseCurrentPopup();
        } else {
            NewVarError = ErrorDetail::Message;
        }
    }

    if (cancelClicked) {
        NewVarError.clear();
        VarCreateRequest = PendingVariableCreate{};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void Canvas::RequestVariableRename(const std::string &name)
{
    VarRenameRequest.Requested = true;
    VarRenameRequest.OldName = name;

    std::snprintf(RenameVarNameBuf, sizeof(RenameVarNameBuf), "%s", name.c_str());
    RenameVarError.clear();
}

void Canvas::DrawRenameVariablePopup()
{
    if (VarRenameRequest.Requested) {
        ImGui::OpenPopup("##canvas_rename_variable_popup");
        VarRenameRequest.Requested = false;
    }

    if (!ImGui::BeginPopup("##canvas_rename_variable_popup"))
        return;

    ImGui::TextUnformatted("Rename Variable");
    ImGui::Separator();

    ImGui::SetNextItemWidth(kPopupFieldWidth);
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere(0);
    }
    bool enterPressed = ImGui::InputTextWithHint(
            "##rename_var_name",
            "name",
            RenameVarNameBuf,
            sizeof(RenameVarNameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);

    if (!RenameVarError.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kPopupFieldWidth);
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", RenameVarError.c_str());
        ImGui::PopTextWrapPos();
    }

    bool renameClicked = ImGui::Button("Rename", ImVec2(PopupButtonWidth(kPopupFieldWidth), 0.0f));
    ImGui::SameLine();
    bool cancelClicked = ImGui::Button("Cancel", ImVec2(PopupButtonWidth(kPopupFieldWidth), 0.0f));

    if (renameClicked || enterPressed) {
        std::string newName(RenameVarNameBuf);
        Error err = RenameVariable(VarRenameRequest.OldName, newName);

        if (err == Error::Ok) {
            RenameVarError.clear();
            VarRenameRequest = PendingVariableRename{};
            ImGui::CloseCurrentPopup();
        } else {
            RenameVarError = ErrorDetail::Message;
        }
    }

    if (cancelClicked) {
        RenameVarError.clear();
        VarRenameRequest = PendingVariableRename{};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void Canvas::DeleteCustomBlock(const std::string &name)
{
    if (!Registry || !IsCustomBlockRegistered(*Registry, name))
        return;

    const std::string callOp = CustomCallOpCode(name);
    const std::string hatOp  = CustomHatOpCode(name);

    std::vector<std::string> paramOps;
    for (const BlockDefinition *paramDef : CustomBlockParamDefs(*Registry, name))
        paramOps.push_back(paramDef->OpCode);

    std::vector<VisualBlock *> hatRoots;
    for (VisualBlock *root : Manager.Roots)
        if (root->Def && root->Def->OpCode == hatOp)
            hatRoots.push_back(root);

    for (VisualBlock *hatRoot : hatRoots)
        Manager.DeleteBelow(hatRoot);

    auto isCallOrParamBlock = [&](const VisualBlock &b) {
        if (!b.Def)
            return false;
        if (b.Def->OpCode == callOp)
            return true;
        return std::find(paramOps.begin(), paramOps.end(), b.Def->OpCode) != paramOps.end();
    };

    std::vector<VisualBlock *> toDelete;
    for (auto &blockPtr : Manager.Blocks)
        CollectBlocksMatching(blockPtr.get(), isCallOrParamBlock, toDelete);

    for (VisualBlock *block : toDelete)
        Manager.DeleteBlock(block);

    DISCARD(UnregisterCustomBlock(*Registry, name));
}

void Canvas::RequestCustomBlockCreation()
{
    CustomBlockCreateRequested = true;
    NewCustomNameBuf[0] = '\0';
    NewCustomDescBuf[0] = '\0';
    NewCustomParams.clear();
    NewCustomError.clear();
}

void Canvas::DrawCreateCustomBlockPopup()
{
    static const char *kTypeNames[]  = { "Int", "Float", "Bool", "String" };
    static const Value  kTypeValues[] = { VAL_INT, VAL_FLOAT, VAL_BOOL, VAL_STRING };

    if (CustomBlockCreateRequested) {
        ImGui::OpenPopup("##canvas_create_custom_block_popup");
        CustomBlockCreateRequested = false;
    }

    if (!ImGui::BeginPopup("##canvas_create_custom_block_popup"))
        return;

    ImGui::TextUnformatted("Create a Block");
    ImGui::Separator();

    ImGui::SetNextItemWidth(kPopupFieldWidthWide);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere(0);
    ImGui::InputTextWithHint(
            "##custom_block_name",
            "block name",
            NewCustomNameBuf,
            sizeof(NewCustomNameBuf));

    ImGui::SetNextItemWidth(kPopupFieldWidthWide);
    ImGui::InputTextMultiline(
            "##custom_block_desc",
            NewCustomDescBuf,
            sizeof(NewCustomDescBuf),
            ImVec2(kPopupFieldWidthWide, 50.0f));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Optional description");

    ImGui::Spacing();
    ImGui::TextUnformatted("Inputs");

    int removeIndex = -1;
    for (int i = 0; i < (int)NewCustomParams.size(); ++i) {
        ImGui::PushID(i);

        CustomParamEdit &p = NewCustomParams[i];

        ImGui::SetNextItemWidth(kPopupFieldWidthWide * 0.5f - 18.0f);
        ImGui::InputTextWithHint("##param_name", "arg name", p.NameBuf, sizeof(p.NameBuf));

        ImGui::SameLine();
        ImGui::SetNextItemWidth(kPopupFieldWidthWide * 0.35f - 18.0f);
        ImGui::Combo("##param_type", &p.TypeIndex, kTypeNames, IM_ARRAYSIZE(kTypeNames));

        ImGui::SameLine();
        if (ImGui::Button("x"))
            removeIndex = i;

        ImGui::PopID();
    }

    if (removeIndex >= 0)
        NewCustomParams.erase(NewCustomParams.begin() + removeIndex);

    if (ImGui::Button("+ Add an Input", ImVec2(kPopupFieldWidthWide, 0.0f)))
        NewCustomParams.push_back(CustomParamEdit{});

    if (!NewCustomError.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kPopupFieldWidthWide);
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", NewCustomError.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::Spacing();
    bool createClicked = ImGui::Button("Create Block", ImVec2(PopupButtonWidth(kPopupFieldWidthWide), 0.0f));
    ImGui::SameLine();
    bool cancelClicked = ImGui::Button("Cancel", ImVec2(PopupButtonWidth(kPopupFieldWidthWide), 0.0f));

    if (createClicked) {
        std::string name(NewCustomNameBuf);

        if (name.empty()) {
            NewCustomError = "Give the block a name";
        } else {
            CustomBlockSpec spec;
            spec.Name = name;
            spec.Description = NewCustomDescBuf;

            for (CustomParamEdit &p : NewCustomParams) {
                std::string pname(p.NameBuf);
                if (pname.empty())
                    continue;
                spec.Params.push_back({ pname, kTypeValues[p.TypeIndex] });
            }

            Error err = RegisterCustomBlock(*Registry, spec);

            if (err == Error::Ok) {
                const BlockDefinition *hatDef =
                    FindDefinitionByOpCode(*Registry, CustomHatOpCode(name));

                if (hatDef) {
                    ImVec2 spawnScreen = Origin + ImVec2(kCustomBlockSpawnOffset, kCustomBlockSpawnOffset);
                    AddBlock(*hatDef, ScreenToWorld(spawnScreen, Origin));
                }

                NewCustomError.clear();
                NewCustomParams.clear();
                ImGui::CloseCurrentPopup();
            } else {
                NewCustomError = ErrorDetail::Message;
            }
        }
    }

    if (cancelClicked) {
        NewCustomError.clear();
        NewCustomParams.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

Error Canvas::RenameCustomBlock(const std::string &oldName, const std::string &newName)
{
    if (!Registry)
        return Error::Failed;

    return ::RenameCustomBlock(*Registry, oldName, newName);
}

void Canvas::RequestCustomBlockRename(const std::string &name)
{
    CustomRenameRequest.Requested = true;
    CustomRenameRequest.OldName = name;

    std::snprintf(RenameCustomNameBuf, sizeof(RenameCustomNameBuf), "%s", name.c_str());
    RenameCustomError.clear();
}

void Canvas::DrawRenameCustomBlockPopup()
{
    if (CustomRenameRequest.Requested) {
        ImGui::OpenPopup("##canvas_rename_custom_block_popup");
        CustomRenameRequest.Requested = false;
    }

    if (!ImGui::BeginPopup("##canvas_rename_custom_block_popup"))
        return;

    ImGui::TextUnformatted("Rename Block");
    ImGui::Separator();

    ImGui::SetNextItemWidth(kPopupFieldWidth);
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere(0);
    }
    bool enterPressed = ImGui::InputTextWithHint(
            "##rename_custom_block_name",
            "block name",
            RenameCustomNameBuf,
            sizeof(RenameCustomNameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);

    if (!RenameCustomError.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kPopupFieldWidth);
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", RenameCustomError.c_str());
        ImGui::PopTextWrapPos();
    }

    bool renameClicked = ImGui::Button("Rename", ImVec2(PopupButtonWidth(kPopupFieldWidth), 0.0f));
    ImGui::SameLine();
    bool cancelClicked = ImGui::Button("Cancel", ImVec2(PopupButtonWidth(kPopupFieldWidth), 0.0f));

    if (renameClicked || enterPressed) {
        std::string newName(RenameCustomNameBuf);
        Error err = RenameCustomBlock(CustomRenameRequest.OldName, newName);

        if (err == Error::Ok) {
            RenameCustomError.clear();
            CustomRenameRequest = PendingCustomBlockRename{};
            ImGui::CloseCurrentPopup();
        } else {
            RenameCustomError = ErrorDetail::Message;
        }
    }

    if (cancelClicked) {
        RenameCustomError.clear();
        CustomRenameRequest = PendingCustomBlockRename{};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}
