#include "ui/canvas.hpp"

static std::string DebugArgValue(const VisualArg &arg)
{
    if (auto *lit = std::get_if<LiteralValue>(&arg)) {
        return std::visit([](const auto &v) -> std::string
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) return "\"" + v + "\"";
            else if constexpr (std::is_same_v<T, bool>)    return v ? "true" : "false";
            else                                           return std::to_string(v);
        }, *lit);
    }

    if (auto *ref = std::get_if<VariableRef>(&arg))
        return "var: " + (ref->Name.empty() ? std::string("(unset)") : ref->Name);

    if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg)) {
        VisualBlock *plugged = held->get();
        if (!plugged)
            return "(empty)";
        return "expr: " + (plugged->Def ? plugged->Def->OpCode : std::string("unknown"))
             + " (#" + std::to_string(plugged->Id) + ")";
    }

    return "?";
}

static void DrawChainDebug(VisualBlock *chain)
{
    int index = 0;

    for (VisualBlock *block = chain; block; block = block->Next) {
        ImGui::BulletText(
                "[%d] ID %u - %s",
                index++,
                block->Id,
                block->Def
                ? block->Def->OpCode.c_str()
                : "unknown");

        if (block->BodyRoots.empty() && block->Args.empty())
            continue;

        ImGui::Indent();

        for (auto &[slot, body] : block->BodyRoots) {
            ImGui::TextDisabled("Body \"%s\":", slot.c_str());

            ImGui::Indent();
            if (body)
                DrawChainDebug(body);
            else
                ImGui::TextDisabled("(empty)");
            ImGui::Unindent();
        }

        for (auto &[key, arg] : block->Args) {
            if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg); held && held->get()) {
                ImGui::TextDisabled("Arg \"%s\":", key.c_str());
                ImGui::Indent();
                DrawChainDebug(held->get());
                ImGui::Unindent();
            } else {
                ImGui::BulletText("Arg \"%s\": %s", key.c_str(), DebugArgValue(arg).c_str());
            }
        }

        ImGui::Unindent();
    }
}

void Canvas::DrawDebugWindow()
{
    ImGui::Begin("Block Debug", &ShowDebugWindow);

    const auto &blocks = Manager.Blocks;
    const auto &roots = Manager.Roots;

    ImGui::Text("Total Blocks: %zu", blocks.size());
    ImGui::Text("Root Blocks: %zu", roots.size());
    ImGui::Separator();

    if (SelectedId == 0) {
        ImGui::Text("Selected: none");
        ImGui::End();
        return;
    }

    VisualBlock *selected = Manager.FindBlock(SelectedId);
    if (!selected) {
        ImGui::Text("Selected block missing");
        ImGui::End();
        return;
    }

    ImGui::Text("Selected ID: %u", selected->Id);
    ImGui::Text( "Position: %.1f, %.1f", selected->Pos.x, selected->Pos.y);

    ImGui::Separator();

    if (selected->Def) {
        ImGui::Text("Opcode: %s", selected->Def->OpCode.c_str());
        ImGui::Text("Category: %d", (int)selected->Def->Category);
        ImGui::Text("Statement: %s", selected->Def->StmtBuilder ? "yes" : "no");

        ImGui::Text("Expression: %s", selected->Def->ExprBuilder ? "yes" : "no");
    }

    ImGui::Separator();

    ImGui::Text("Prev: %s", selected->Prev ? std::to_string(selected->Prev->Id).c_str() : "none");
    ImGui::Text( "Next: %s", selected->Next ? std::to_string(selected->Next->Id).c_str() : "none");

    if (selected->BodyOwner) {
        ImGui::Text("Body Owner: %u (slot \"%s\")", selected->BodyOwner->Id, selected->BodySlot.c_str());
    }

    ImGui::Separator();

    ImGui::Text("Bodies: %zu", selected->BodyRoots.size());

    for (auto &[slot, body] : selected->BodyRoots) {
        ImGui::BulletText("\"%s\": %s", slot.c_str(),
                body ? std::to_string(body->Id).c_str() : "empty");
    }

    ImGui::Separator();

    if (selected->ArgOwner) {
        ImGui::Text("Arg Owner: %u (slot \"%s\")", selected->ArgOwner->Id, selected->ArgSlot.c_str());
        ImGui::Separator();
    }

    ImGui::Text("Args: %zu", selected->Args.size());

    for (auto &[key, arg] : selected->Args) {
        ImGui::BulletText("\"%s\": %s", key.c_str(), DebugArgValue(arg).c_str());
    }

    ImGui::Separator();

    ImGui::Text("Chain:");

    VisualBlock *root = selected;
    while (root->Prev)
        root = root->Prev;

    DrawChainDebug(root);

    ImGui::End();

    if (ImGui::IsWindowAppearing())
        ImGui::SetWindowFocus("Block Debug");
}
