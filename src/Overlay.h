#pragma once

// Overlay: DXGI/D3D11 hooks + ImGui frame pump. Self-contained (no Dust).
void Overlay_Init();

// implemented in KenshiTrainer.cpp
void Trainer_LoadSettings(const char* directory);
void DrawTrainerUI();
bool Trainer_MenuOpen();
