#include "ReSTIR.h"

void ReSTIR::processGUI()
{
    if (ImGui::CollapsingHeader("ReSTIR"))
    {
        ImGui::Checkbox("enable", &mEnabled);
        
        const char* items[] = { "No Resampling", "No temporal, spatial reuse", "Temporal reuse only"};
        ImGui::ListBox("listbox", &currentMode, items, IM_ARRAYSIZE(items), 4);

    }
}