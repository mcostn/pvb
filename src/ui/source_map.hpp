#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "TextEditor.h"

struct SourceRange
{
    size_t Start = 0; // inclusive
    size_t End = 0; // exclusive
};

class SourceMap
{
public:
    void Clear() { Ranges.clear(); }
    void Add(uint32_t blockId, SourceRange range) { Ranges[blockId] = range; }
    const SourceRange *Find(uint32_t blockId) const;

    static TextEditor::Coordinates OffsetToCoordinates(const std::string &text, size_t offset);

private:
    std::unordered_map<uint32_t, SourceRange> Ranges;
};
