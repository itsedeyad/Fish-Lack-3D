#pragma once

namespace lf::settings {
constexpr int timerMs = 16;
constexpr float maxStep = 1.0f / 60.0f;
constexpr float mouseRetrieveDelay = 0.35f;
constexpr float waterStep = 1.0f;
constexpr float rippleWidth = 0.9f;
// Presentation thresholds only: these do not change fishing physics.
constexpr float tensionLow = 0.12f;
constexpr float tensionWarning = 0.60f;
constexpr float tensionDanger = 0.80f;
constexpr int previewArcSamples = 30;
constexpr int castTrailLimit = 128;
constexpr float aimPreviewPower = 0.0f;
constexpr float aimRingRadius = 1.20f;
constexpr float aimReticleRadius = 18.0f;
constexpr float alertPulseHz = 1.2f;

} // namespace lf::settings
