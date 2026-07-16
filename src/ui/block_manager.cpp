#include <cmath>

#include "ui/block_manager.hpp"
#include "ui/const.hpp"

static void AdoptBodyHead(VisualBlock *owner, const std::string &slot, VisualBlock *head)
{
    if (!head)
        return;

    head->BodyOwner = owner;
    head->BodySlot = slot;
}

static void AdoptArgChild(VisualBlock *owner, const std::string &key, VisualBlock *child)
{
    if (!child)
        return;

    child->ArgOwner = owner;
    child->ArgSlot = key;
}

static const BlockSchemaItem *FindSchemaItem(const BlockDefinition *def, const std::string &key)
{
    if (!def)
        return nullptr;

    for (const BlockSchemaItem &item : def->Schema)
        if (item.Name == key)
            return &item;

    return nullptr;
}

static float DistanceToRect(ImVec2 p, ImVec2 rectMin, ImVec2 rectMax)
{
    float dx = std::max({rectMin.x - p.x, 0.0f, p.x - rectMax.x});
    float dy = std::max({rectMin.y - p.y, 0.0f, p.y - rectMax.y});
    return std::sqrt(dx * dx + dy * dy);
}

VisualBlock *BlockManager::AddBlock(const BlockDefinition &def, ImVec2 worldPos)
{
    auto block = std::make_unique<VisualBlock>();

    block->Id = NextId++;
    block->Def = &def;
    block->Pos = worldPos;

    for (const BlockSchemaItem &item : def.Schema) {
        if (item.Type == BlockSchemaType::Body)
            continue;
        block->Args.emplace(item.Name, MakeDefaultArg(def, item));
    }

    VisualBlock *result = block.get();
    Blocks.push_back(std::move(block));
    Roots.push_back(result);

    return result;
}

VisualArg BlockManager::CloneArg(const VisualArg &arg)
{
    return std::visit([this](const auto &value) -> VisualArg
    {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<VisualBlock>>) {
            if (!value)
                return std::unique_ptr<VisualBlock>();

            auto clone = std::make_unique<VisualBlock>();
            clone->Id = NextId++;
            clone->Def = value->Def;
            clone->Pos = value->Pos;

            for (const auto &[key, a] : value->Args)
                clone->Args.emplace(key, CloneArg(a));
            for (const auto &[slot, bodyHead] : value->BodyRoots) {
                VisualBlock *clonedHead = bodyHead ? CloneChain(bodyHead) : nullptr;
                clone->BodyRoots[slot] = clonedHead;
                AdoptBodyHead(clone.get(), slot, clonedHead);
            }

            return clone;
        } else {
            return value;
        }
    }, arg);
}

VisualBlock *BlockManager::CloneNode(const VisualBlock &src)
{
    auto clone = std::make_unique<VisualBlock>();
    clone->Id = NextId++;
    clone->Def = src.Def;
    clone->Pos = src.Pos;

    for (const auto &[key, arg] : src.Args)
        clone->Args.emplace(key, CloneArg(arg));
    for (const auto &[slot, bodyHead] : src.BodyRoots) {
        VisualBlock *clonedHead = bodyHead ? CloneChain(bodyHead) : nullptr;
        clone->BodyRoots[slot] = clonedHead;
        AdoptBodyHead(clone.get(), slot, clonedHead);
    }

    for (auto &[key, arg] : clone->Args) {
        if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg))
            AdoptArgChild(clone.get(), key, held->get());
    }

    VisualBlock *result = clone.get();
    Blocks.push_back(std::move(clone));
    return result;
}

VisualBlock *BlockManager::CloneChain(VisualBlock *head)
{
    VisualBlock *prevClone = nullptr;
    VisualBlock *headClone = nullptr;

    for (VisualBlock *cur = head; cur; cur = cur->Next) {
        VisualBlock *result = CloneNode(*cur);

        if (!headClone)
            headClone = result;
        if (prevClone) {
            prevClone->Next = result;
            result->Prev = prevClone;
        }
        prevClone = result;
    }

    return headClone;
}

VisualBlock *BlockManager::DuplicateBlock(VisualBlock *block)
{
    VisualBlock *copy = CloneNode(*block);
    copy->Pos = block->Pos + ImVec2(kDuplicateOffset, kDuplicateOffset);

    Roots.push_back(copy);
    return copy;
}

void BlockManager::DestroyBlock(VisualBlock *block)
{
    if (!block)
        return;

    for (auto &[slot, bodyHead] : block->BodyRoots) {
        VisualBlock *child = bodyHead;
        while (child) {
            VisualBlock *next = child->Next;
            DestroyBlock(child);
            child = next;
        }
    }

    Blocks.erase(
        std::remove_if(
            Blocks.begin(),
            Blocks.end(),
            [&](const auto& ptr)
            {
                return ptr.get() == block;
            }),
        Blocks.end());
}

void BlockManager::DeleteRange(VisualBlock *first, VisualBlock *last)
{
    if (!first || !last)
        return;

    VisualBlock *before = first->Prev;
    VisualBlock *after  = last->Next;

    if (before) {
        before->Next = after;
    } else if (first->BodyOwner) {
        VisualBlock *owner = first->BodyOwner;
        std::string slot = first->BodySlot;

        owner->BodyRoots[slot] = after;

        if (after) {
            after->BodyOwner = owner;
            after->BodySlot = slot;
        }
    } else {
        auto it = std::find(Roots.begin(), Roots.end(), first);
        if (it != Roots.end()) {
            if (after)
                *it = after;
            else
                Roots.erase(it);
        }
    }

    if (after)
        after->Prev = before;

    first->Prev = nullptr;
    first->BodyOwner = nullptr;
    first->BodySlot.clear();
    last->Next = nullptr;

    VisualBlock *block = first;

    while (block) {
        VisualBlock *next = block->Next;
        DestroyBlock(block);
        block = next;
    }
}

VisualBlock *BlockManager::FindBlock(u32 id)
{
    for (auto &b : Blocks)
        if (b->Id == id) return b.get();

    return nullptr;
}

void BlockManager::Detach(VisualBlock *block)
{
    if (!block)
        return;

    if (block->Prev) {
        block->Prev->Next = nullptr;
        block->Prev = nullptr;
    } else if (block->BodyOwner) {
        block->BodyOwner->BodyRoots[block->BodySlot] = nullptr;
        block->BodyOwner = nullptr;
        block->BodySlot.clear();
    } else if (block->ArgOwner) {
        VisualBlock *owner = block->ArgOwner;
        std::string key = block->ArgSlot;

        auto argIt = owner->Args.find(key);
        if (argIt != owner->Args.end()) {
            if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&argIt->second)) {
                if (held->get() == block)
                    Blocks.push_back(std::move(*held));
            }

            const BlockSchemaItem *item = FindSchemaItem(owner->Def, key);
            argIt->second = (item && owner->Def)
                ? MakeDefaultArg(*owner->Def, *item)
                : VisualArg{LiteralValue{std::in_place_type<int>, 0}};
        }

        block->ArgOwner = nullptr;
        block->ArgSlot.clear();
    } else {
        Roots.erase(
            std::remove(
                Roots.begin(),
                Roots.end(),
                block),
            Roots.end());
    }

    Roots.push_back(block);
}

void BlockManager::AttachBefore(VisualBlock *parent, VisualBlock *child)
{
    if (!parent || !child)
        return;
    if (!IsStatement(parent) || !IsStatement(child))
        return;
    if (parent->Def && parent->Def->Shape == BlockShape::Hat)
        return;
    if (child->Def && child->Def->Shape == BlockShape::Hat)
        return;

    Detach(child);

    Roots.erase(
            std::remove(Roots.begin(), Roots.end(), child),
            Roots.end());

    VisualBlock *prev = parent->Prev;

    child->Next = parent;
    child->Prev = prev;

    parent->Prev = child;

    if (prev) {
        prev->Next = child;
    } else if (parent->BodyOwner) {
        child->BodyOwner = parent->BodyOwner;
        child->BodySlot = parent->BodySlot;
        child->BodyOwner->BodyRoots[child->BodySlot] = child;

        parent->BodyOwner = nullptr;
        parent->BodySlot.clear();
    } else {
        auto it = std::find(Roots.begin(), Roots.end(), parent);
        if (it != Roots.end())
            *it = child;
    }
}

void BlockManager::AttachToBody(VisualBlock *owner, const std::string &slot, VisualBlock *child)
{
    if (!owner || !child)
        return;
    if (!IsStatement(child))
        return;

    Detach(child);

    Roots.erase(
        std::remove(Roots.begin(), Roots.end(), child),
        Roots.end());

    VisualBlock *existingHead = owner->BodyRoots[slot];

    child->Prev = nullptr;

    if (existingHead) {
        VisualBlock *tail = FindTail(child);
        tail->Next = existingHead;
        existingHead->Prev = tail;
        existingHead->BodyOwner = nullptr;
        existingHead->BodySlot.clear();
    }

    child->BodyOwner = owner;
    child->BodySlot = slot;
    owner->BodyRoots[slot] = child;
}

void BlockManager::AttachToArg(VisualBlock *owner, const std::string &key, VisualBlock *child)
{
    if (!owner || !child)
        return;
    if (!IsReporter(child))
        return;

    Detach(child);

    Roots.erase(
        std::remove(Roots.begin(), Roots.end(), child),
        Roots.end());

    VisualArg &slot = owner->Args[key];

    if (auto *existing = std::get_if<std::unique_ptr<VisualBlock>>(&slot)) {
        if (VisualBlock *previous = existing->get()) {
            previous->ArgOwner = nullptr;
            previous->ArgSlot.clear();
            previous->Pos = owner->Pos + ImVec2(owner->Size.x + kDuplicateOffset, 0.0f);

            Blocks.push_back(std::move(*existing));
            Roots.push_back(previous);
        }
    }

    child->ArgOwner = owner;
    child->ArgSlot = key;
    child->Prev = nullptr;
    child->Next = nullptr;

    auto it = std::find_if(Blocks.begin(), Blocks.end(),
        [&](const auto &ptr) { return ptr.get() == child; });

    if (it != Blocks.end()) {
        slot = std::move(*it);
        Blocks.erase(it);
    }
}

void BlockManager::AttachAfter(VisualBlock *parent, VisualBlock *child)
{
    if (!parent || !child)
        return;
    if (!IsStatement(parent) || !IsStatement(child))
        return;
    if (child->Def && child->Def->Shape == BlockShape::Hat)
        return;

    Detach(child);

    Roots.erase(
            std::remove(Roots.begin(), Roots.end(), child),
            Roots.end());

    VisualBlock *tail = FindTail(child);
    VisualBlock *oldNext = parent->Next;

    parent->Next = child;
    child->Prev = parent;

    tail->Next = oldNext;
    if (oldNext)
        oldNext->Prev = tail;
}

SnapResult BlockManager::FindSnapTarget(VisualBlock *dragging, float zoom)
{
    SnapResult result;

    float bestDist = kSnapDistance / std::max(zoom, 0.01f);

    for (VisualBlock *root : Roots)
        SearchChainForSnap(root, dragging, bestDist, result);

    return result;
}

void BlockManager::SearchChainForSnap(VisualBlock *chain, VisualBlock *dragging, float &bestDist, SnapResult &result)
{
    for (VisualBlock *block = chain; block; block = block->Next) {
        if (IsInChain(dragging, block))
            continue;

        if (IsStatement(block) && IsStatement(dragging)) {
            float dx = BottomSnap(*block).x - TopSnap(*dragging).x;
            float dy = BottomSnap(*block).y - TopSnap(*dragging).y;
            float dist = std::sqrt(dx  *dx + dy  *dy);

            if (dist < bestDist) {
                bestDist = dist;
                result.Block = block;
                result.Slot.clear();
                result.Type = SnapType::Append;
            }

            dx = BottomSnap(*dragging).x - TopSnap(*block).x;
            dy = BottomSnap(*dragging).y - TopSnap(*block).y;
            dist = std::sqrt(dx  *dx + dy  *dy);

            if (dist < bestDist) {
                bestDist = dist;
                result.Block = block;
                result.Slot.clear();
                result.Type = SnapType::Prepend;
            }
        }

        if (IsStatement(dragging)) {
            for (const RowLayout &row : block->Layout.Rows) {
                if (!row.IsBody || !row.BodyItem)
                    continue;

                const std::string &slot = row.BodyItem->Name;
                auto it = block->BodyRoots.find(slot);
                VisualBlock *bodyHead = (it != block->BodyRoots.end()) ? it->second : nullptr;

                if (bodyHead) {
                    SearchChainForSnap(bodyHead, dragging, bestDist, result);
                    continue;
                }

                ImVec2 opening(block->Pos.x + kBodyIndent + dragging->Size.x * 0.5f, block->Pos.y + row.Top);
                ImVec2 dragTop = TopSnap(*dragging);

                float odx = opening.x - dragTop.x;
                float ody = opening.y - dragTop.y;
                float odist = std::sqrt(odx  *odx + ody  *ody);

                if (odist < bestDist) {
                    bestDist = odist;
                    result.Block = block;
                    result.Slot = slot;
                    result.Type = SnapType::EnterBody;
                }
            }
        }

        if (IsReporter(dragging)) {
            ImVec2 dragCenter = dragging->Pos + dragging->Size * 0.5f;

            for (const RowLayout &row : block->Layout.Rows) {
                if (row.IsBody)
                    continue;

                for (const SlotLayout &argSlot : row.Slots) {
                    if (argSlot.Item->Type != BlockSchemaType::Input &&
                         argSlot.Item->Type != BlockSchemaType::Var)
                        continue;

                    auto arg = block->Args.find(argSlot.Item->Name);
                    if (arg != block->Args.end() &&
                        (std::holds_alternative<std::unique_ptr<VisualBlock>>(arg->second) ||
                         std::holds_alternative<VariableRef>(arg->second)))
                        continue;

                    ImVec2 rectMin = block->Pos + argSlot.Pos;
                    ImVec2 rectMax = rectMin + argSlot.Size;

                    float adist = DistanceToRect(dragCenter, rectMin, rectMax);

                    if (adist < bestDist) {
                        bestDist = adist;
                        result.Block = block;
                        result.Slot = argSlot.Item->Name;
                        result.Type = SnapType::EnterArg;
                    }
                }
            }
        }

        for (const auto &[key, arg] : block->Args) {
            if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg)) {
                if (held->get())
                    SearchChainForSnap(held->get(), dragging, bestDist, result);
            }
        }
    }
}

bool BlockManager::IsInChain(VisualBlock *root, VisualBlock *test)
{
    for (VisualBlock *block = root; block; block = block->Next) {
        if (block == test)
            return true;

        for (const auto &[slot, body] : block->BodyRoots) {
            if (body && IsInChain(body, test))
                return true;
        }

        for (const auto &[key, arg] : block->Args) {
            if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg)) {
                if (held->get() && IsInChain(held->get(), test))
                    return true;
            }
        }
    }

    return false;
}

VisualBlock *BlockManager::FindTail(VisualBlock *block)
{
    if (!block)
        return nullptr;

    while (block->Next)
        block = block->Next;

    return block;
}

VisualBlock *BlockManager::FindRoot(VisualBlock *b)
{
    while (b->Prev)
        b = b->Prev;

    return b;
}
