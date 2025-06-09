#ifndef INPUT_H
#define INPUT_H

#include "ui_types.h"
#include "clay.h"

void handle_tracker_form_input(TrackerForm* form, int key);
void handle_interval_form_input(IntervalForm* form, int key);
void handle_input(ComboUI* ui, int key);
void handle_click(Clay_ElementId elementId, Clay_PointerData pointerData, intptr_t userData);

#endif // INPUT_H 