#pragma once

// Overlay: DXGI/D3D11 hooks + ImGui frame pump. Self-contained (no Dust).
void Overlay_Init();

// implemented in KenshiTrainer.cpp
void DrawTrainerUI();
bool Trainer_MenuOpen();
