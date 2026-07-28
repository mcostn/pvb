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

VisualBlock *BlockManager::AddBlock(const BlockDefinition &def, ImVec2 worldPos)
{
    auto block = std::make_unique<VisualBlock>();

    block->Id = NextId++;
    block->Def = &def;
    block->Pos = worldPos;

    for (const BlockSchemaItem &item : def.Schema) {
        if (item.Type == BlockSchemaType::Body || item.Type == BlockSchemaType::LineBreak)
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

            for (auto &[key, a] : clone->Args) {
                if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&a))
                    AdoptArgChild(clone.get(), key, held->get());
            }

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

VisualBlock *BlockManager::CloneNode(const VisualBlock &src, bool includeArgs, bool includeBodies)
{
    auto clone = std::make_unique<VisualBlock>();
    clone->Id = NextId++;
    clone->Def = src.Def;
    clone->Pos = src.Pos;

    for (const auto &[key, arg] : src.Args) {
        if (includeArgs) {
            clone->Args.emplace(key, CloneArg(arg));
        } else {
            const BlockSchemaItem *item = FindSchemaItem(src.Def, key);
            clone->Args.emplace(key, (item && src.Def)
                ? MakeDefaultArg(*src.Def, *item)
                : VisualArg{LiteralValue{std::in_place_type<int>, 0}});
        }
    }

    for (const auto &[slot, bodyHead] : src.BodyRoots) {
        VisualBlock *clonedHead = (includeBodies && bodyHead) ? CloneChain(bodyHead) : nullptr;
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

VisualBlock *BlockManager::CloneRange(VisualBlock *first, VisualBlock *last)
{
    VisualBlock *prevClone = nullptr;
    VisualBlock *headClone = nullptr;

    for (VisualBlock *cur = first; cur; cur = cur->Next) {
        VisualBlock *result = CloneNode(*cur);

        if (!headClone)
            headClone = result;
        if (prevClone) {
            prevClone->Next = result;
            result->Prev = prevClone;
        }
        prevClone = result;

        if (cur == last)
            break;
    }

    return headClone;
}

VisualBlock *BlockManager::CloneChain(VisualBlock *head)
{
    return CloneRange(head, nullptr);
}

VisualBlock *BlockManager::DuplicateBlock(VisualBlock *block)
{
    VisualBlock *copy = CloneNode(*block);
    copy->Pos = block->Pos + ImVec2(kDuplicateOffset, kDuplicateOffset);

    Roots.push_back(copy);
    return copy;
}

VisualBlock *BlockManager::DuplicateBlockWithoutArgs(VisualBlock *block)
{
    if (!block)
        return nullptr;

    VisualBlock *copy = CloneNode(*block, /*includeArgs=*/false, /*includeBodies=*/true);
    copy->Pos = block->Pos + ImVec2(kDuplicateOffset, kDuplicateOffset);

    Roots.push_back(copy);
    return copy;
}

VisualBlock *BlockManager::DuplicateBlockWithoutBodies(VisualBlock *block)
{
    if (!block)
        return nullptr;

    VisualBlock *copy = CloneNode(*block, /*includeArgs=*/true, /*includeBodies=*/false);
    copy->Pos = block->Pos + ImVec2(kDuplicateOffset, kDuplicateOffset);

    Roots.push_back(copy);
    return copy;
}

// Clones `block` through its tail (inclusive), matching DeleteBelow's range.
VisualBlock *BlockManager::DuplicateBelow(VisualBlock *block)
{
    if (!block)
        return nullptr;

    VisualBlock *copyHead = CloneChain(block);
    copyHead->Pos = block->Pos + ImVec2(kDuplicateOffset, kDuplicateOffset);

    Roots.push_back(copyHead);
    return copyHead;
}

VisualBlock *BlockManager::DuplicateAbove(VisualBlock *block)
{
    if (!block)
        return nullptr;

    VisualBlock *root = FindRoot(block);
    VisualBlock *copyHead = CloneRange(root, block);
    copyHead->Pos = root->Pos + ImVec2(kDuplicateOffset, kDuplicateOffset);

    Roots.push_back(copyHead);
    return copyHead;
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

    if (first->ArgOwner) {
        VisualBlock *owner = first->ArgOwner;
        std::string key = first->ArgSlot;

        auto argIt = owner->Args.find(key);
        if (argIt != owner->Args.end()) {
            const BlockSchemaItem *item = FindSchemaItem(owner->Def, key);
            argIt->second = (item && owner->Def)
                ? MakeDefaultArg(*owner->Def, *item)
                : VisualArg{LiteralValue{std::in_place_type<int>, 0}};
        }

        return;
    }

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

bool BlockManager::HasPluggedArgs(const VisualBlock *block) const
{
    if (!block)
        return false;

    for (const auto &[key, arg] : block->Args) {
        auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg);
        if (held && held->get())
            return true;
    }

    return false;
}

void BlockManager::DeleteArgs(VisualBlock *block)
{
    if (!block)
        return;

    for (auto &[key, arg] : block->Args) {
        if (!std::get_if<std::unique_ptr<VisualBlock>>(&arg))
            continue;

        const BlockSchemaItem *item = FindSchemaItem(block->Def, key);
        arg = (item && block->Def)
            ? MakeDefaultArg(*block->Def, *item)
            : VisualArg{LiteralValue{std::in_place_type<int>, 0}};
    }
}

void BlockManager::DetachArgs(VisualBlock *block)
{
    if (!block)
        return;

    float stackOffset = 0.0f;

    for (auto &[key, arg] : block->Args) {
        auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg);
        if (!held || !held->get())
            continue;

        VisualBlock *child = held->get();
        child->ArgOwner = nullptr;
        child->ArgSlot.clear();
        child->Pos = block->Pos + ImVec2(block->Size.x + kDuplicateOffset, stackOffset);
        stackOffset += child->Size.y + kDuplicateOffset;

        Blocks.push_back(std::move(*held));
        Roots.push_back(child);

        const BlockSchemaItem *item = FindSchemaItem(block->Def, key);
        arg = (item && block->Def)
            ? MakeDefaultArg(*block->Def, *item)
            : VisualArg{LiteralValue{std::in_place_type<int>, 0}};
    }
}

void BlockManager::DeleteWithoutArgs(VisualBlock *block)
{
    if (!block)
        return;

    DetachArgs(block);
    DeleteBlock(block);
}

bool BlockManager::HasBodies(const VisualBlock *block) const
{
    if (!block)
        return false;

    for (const auto &[slot, head] : block->BodyRoots)
        if (head)
            return true;

    return false;
}

void BlockManager::DeleteBodies(VisualBlock *block)
{
    if (!block)
        return;

    for (auto &[slot, head] : block->BodyRoots) {
        VisualBlock *child = head;
        while (child) {
            VisualBlock *next = child->Next;
            DestroyBlock(child);
            child = next;
        }
        head = nullptr;
    }
}

void BlockManager::DetachBodies(VisualBlock *block)
{
    if (!block)
        return;

    float stackOffset = 0.0f;

    for (auto &[slot, head] : block->BodyRoots) {
        if (!head)
            continue;

        VisualBlock *chainHead = head;
        chainHead->BodyOwner = nullptr;
        chainHead->BodySlot.clear();
        chainHead->Pos = block->Pos + ImVec2(block->Size.x + kDuplicateOffset, stackOffset);
        stackOffset += chainHead->Size.y + kDuplicateOffset;

        Roots.push_back(chainHead);
        head = nullptr;
    }
}

void BlockManager::DeleteWithoutBodies(VisualBlock *block)
{
    if (!block)
        return;

    DetachBodies(block);
    DeleteBlock(block);
}

void BlockManager::DeleteAll()
{
    std::vector<VisualBlock *> toDelete;

    for (VisualBlock *root : Roots) {
        bool isCustomDefinition =
            root->Def &&
            root->Def->Shape == BlockShape::Hat &&
            root->Def->Category == BlockCategory::Custom;

        if (!isCustomDefinition)
            toDelete.push_back(root);
    }

    for (VisualBlock *root : toDelete)
        DeleteRange(root, FindTail(root));
}

static VisualBlock *FindInPluggedArgs(VisualBlock *block, u32 id)
{
    for (auto &[key, arg] : block->Args) {
        if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg)) {
            VisualBlock *child = held->get();
            if (!child)
                continue;

            if (child->Id == id)
                return child;

            if (VisualBlock *hit = FindInPluggedArgs(child, id))
                return hit;
        }
    }

    return nullptr;
}

VisualBlock *BlockManager::FindBlock(u32 id)
{
    for (auto &b : Blocks) {
        if (b->Id == id) return b.get();

        if (VisualBlock *hit = FindInPluggedArgs(b.get(), id))
            return hit;
    }

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
    DISCARD(zoom);

    SnapResult result;
    float bestDist = kSnapDistance;

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
            if (dragging->Def->Shape != BlockShape::Hat &&
                    block->Def->Shape != BlockShape::Cap) {
                float dx = BottomSnap(*block).x - TopSnap(*dragging).x;
                float dy = BottomSnap(*block).y - TopSnap(*dragging).y;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < bestDist) {
                    bestDist = dist;
                    result.Block = block;
                    result.Slot.clear();
                    result.Type = SnapType::Append;
                }
            }

            if (dragging->Def->Shape != BlockShape::Cap &&
                    block->Def->Shape != BlockShape::Hat) {
                float dx = BottomSnap(*dragging).x - TopSnap(*block).x;
                float dy = BottomSnap(*dragging).y - TopSnap(*block).y;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < bestDist) {
                    bestDist = dist;
                    result.Block = block;
                    result.Slot.clear();
                    result.Type = SnapType::Prepend;
                }
            }
        }

        if (IsReporter(dragging)) {
            ImVec2 dragTop = TopSnap(*dragging);

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

                    ImVec2 slotTop = block->Pos + argSlot.Pos;

                    float dx = slotTop.x - dragTop.x;
                    float dy = slotTop.y - dragTop.y;
                    float adist = std::sqrt(dx * dx + dy * dy);

                    if (adist < bestDist) {
                        bestDist = adist;
                        result.Block = block;
                        result.Slot = argSlot.Item->Name;
                        result.Type = SnapType::EnterArg;
                    }
                }
            }
        }

        for (const RowLayout &row : block->Layout.Rows) {
            if (!row.IsBody || !row.BodyItem) {
                continue;
            }

            const std::string &slot = row.BodyItem->Name;
            auto it = block->BodyRoots.find(slot);
            VisualBlock *bodyHead = (it != block->BodyRoots.end()) ? it->second : nullptr;

            if (bodyHead)
                SearchChainForSnap(bodyHead, dragging, bestDist, result);

            if (!bodyHead && IsStatement(dragging)) {
                ImVec2 opening(block->Pos.x, block->Pos.y + row.Top);
                ImVec2 dragTop = TopSnap(*dragging);

                float dx = opening.x - dragTop.x;
                float dy = opening.y - dragTop.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < bestDist) {
                    bestDist = dist;
                    result.Block = block;
                    result.Slot = slot;
                    result.Type = SnapType::EnterBody;
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
