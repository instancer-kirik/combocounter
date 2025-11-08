#ifndef UI_H
#define UI_H

#include "ui_types.h"
#include "clay.h"

void init_ui(ComboUI* ui);
Clay_RenderCommandArray combo_ui_render(ComboUI* ui);
void add_new_tracker(ComboUI* ui);
void add_new_interval(ComboUI* ui);
void save_ui_state(ComboUI* ui);
void load_ui_state(ComboUI* ui);

#endif // UI_H
