#pragma once
// Building placement validity bypass: trainer-side state + accessors.
// Implementation (and the game-header include) lives in BuildBypass.cpp,
// kept separate so kenshi/Building/Building.h's global BuildingDesignation
// enum does not clash with kenshi/Platoon.h in KenshiTrainer.cpp.

bool BuildBypass_Enabled();
void BuildBypass_Set(bool enabled);
void BuildBypass_Install();   // installs the placementVerification hook
