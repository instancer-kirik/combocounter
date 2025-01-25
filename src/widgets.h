#ifndef WIDGETS_H
#define WIDGETS_H

#include "ui_types.h"
#include "clay.h"

void widget_break_menu(BreakMenu* menu, ComboUI* ui);
void widget_objective_progress(ComboState* tracker, Clay_Color objective_color, Clay_Color completed_color, Clay_Color paused_color);
void widget_interval_tracker(IntervalTracker* intervals, Clay_Color active_color, Clay_Color paused_color);
void widget_tracker_card(ComboState* tracker, int index, Clay_Color active_color, Clay_Color paused_color, Clay_Color perfect_color);
void widget_controls_panel(Clay_Color active_color, Clay_Color paused_color, Clay_Color break_color);

#endif // WIDGETS_H 