#include "ui/source_map.hpp"

#include <algorithm>


const SourceRange *SourceMap::Find(uint32_t blockId) const
{
    auto it = Ranges.find(blockId);
    return it == Ranges.end() ? nullptr : &it->second;
}

TextEditor::Coordinates SourceMap::OffsetToCoordinates(const std::string &text, size_t offset)
{
    int line = 0;
    int col = 0;

    size_t n = std::min(offset, text.size());
    for (size_t i = 0; i < n; ++i) {
        if (text[i] == '\n') {
            ++line;
            col = 0;
        } else {
            ++col;
        }
    }

    return TextEditor::Coordinates(line, col);
}
