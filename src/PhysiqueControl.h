#pragma once

class Character;
class AppearanceHuman;

struct PhysiqueValues
{
    float bulk;
    float muscle;
    float skinny;
};

void PhysiqueControl_Install();
bool PhysiqueControl_Ready();
bool PhysiqueControl_Read(Character* character, PhysiqueValues* values,
    bool* locked, AppearanceHuman** identity);
bool PhysiqueControl_Set(Character* character, const PhysiqueValues& values, bool locked);
