#pragma once

#include <vector>
#include <memory>

#include "ui/block.hpp"

enum class SnapType
{
    None,
    Append,
    Prepend,
    EnterBody,
    EnterArg
};

struct SnapResult
{
    VisualBlock *Block = nullptr;
    std::string Slot;
    SnapType Type = SnapType::None;
};

class BlockManager
{
public:
    VisualBlock *AddBlock(const BlockDefinition& def, ImVec2 worldPos);
    VisualBlock *DuplicateBlock(VisualBlock *block);

    void DestroyBlock(VisualBlock *block);
    void DeleteRange(VisualBlock *first, VisualBlock *last);
    void DeleteBlock(VisualBlock *block) { DeleteRange(block, block); }
    void DeleteBelow(VisualBlock *block) { DeleteRange(block, FindTail(block)); }
    void DeleteAbove(VisualBlock *block) { DeleteRange(FindRoot(block), block); }
    void DeleteAll();

    VisualBlock *FindBlock(u32 id);

    void Detach(VisualBlock *block);
    void AttachAfter(VisualBlock *parent, VisualBlock *child);
    void AttachBefore(VisualBlock *parent, VisualBlock *child);
    void AttachToBody(VisualBlock *owner, const std::string &slot, VisualBlock *child);
    void AttachToArg(VisualBlock *owner, const std::string &key, VisualBlock *child);

    SnapResult FindSnapTarget(VisualBlock *dragging, float zoom);

    bool IsInChain(VisualBlock *root, VisualBlock *test);
    VisualBlock *FindTail(VisualBlock *block);
    VisualBlock *FindRoot(VisualBlock *block);

    std::vector<std::unique_ptr<VisualBlock>> Blocks;
    std::vector<VisualBlock*> Roots;

    VisualBlock *CloneNode(const VisualBlock &src);
    VisualBlock *CloneChain(VisualBlock *head);
    VisualArg CloneArg(const VisualArg &arg);

    u32 NextId = 1;

private:
    void SearchChainForSnap(VisualBlock *chain, VisualBlock *dragging, float &bestDist, SnapResult &result);
};
