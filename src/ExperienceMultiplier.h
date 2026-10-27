#pragma once

// Percentage precision: 100 = 1.00x, 0 = no XP, 100000 = 1000.00x.
void ExperienceMultiplier_Configure(int percent, bool player, bool ai);
void ExperienceMultiplier_Install();
bool ExperienceMultiplier_Ready();
