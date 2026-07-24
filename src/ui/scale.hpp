#pragma once

inline constexpr float kUiScaleOptions[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

constexpr float kMinUiScale = kUiScaleOptions[0];
constexpr float kMaxUiScale = kUiScaleOptions[6];
constexpr float kDefaultUiScale = 1.0f;

void InitUiScale(float initialScale = kDefaultUiScale);
void SetUiScale(float scale);
float GetUiScale();
