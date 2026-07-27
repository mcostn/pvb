#pragma once

constexpr ImU32 kCanvasBgColor = IM_COL32(30, 30, 34, 255);

constexpr float kLineHeightFactor = 1.3f;
constexpr float kRowGap = 3.0f;

constexpr float kBodyIndent = 18.0f;
constexpr float kBodyMinHeight = 28.0f;
constexpr float kBodyBottomBarHeight = 14.0f;
constexpr float kTextHorizontalPadding = 6.0f;
constexpr float kTextVerticalPadding = 4.0f;
constexpr float kMinBlockWidth = 60.0f;

constexpr float kReporterPadding = 10.0f;
constexpr float kBoolInsetRatio = 0.30f;
constexpr float kBoolAspectRatio = 1.4f;

constexpr float kDuplicateOffset = 24.0f;
constexpr float kSnapDistance = 32.0f;

constexpr float kMinZoom = 0.25f;
constexpr float kMaxZoom = 2.5f;

constexpr float kCommentResizeGrip = 14.0f;

constexpr float kPopupFieldWidth = 220.0f;
constexpr float kPopupFieldWidthWide = 280.0f;

constexpr float PopupButtonWidth(float fieldWidth)
{
    return fieldWidth * 0.5f - 4.0f;
}

constexpr ImU32 kCommentBgColor = IM_COL32(255, 243, 140, 255);
constexpr ImU32 kCommentTitleBgColor = IM_COL32(255, 220, 80, 255);
constexpr ImU32 kCommentBorderColor = IM_COL32(180, 170, 70, 255);
constexpr ImU32 kCommentTextColor = IM_COL32(60, 50, 10, 255);

constexpr float kCustomBlockSpawnOffset = 60.0f;
