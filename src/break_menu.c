#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "ui_types.h"

void break_menu_init(BreakMenu* menu) {
    menu->visible = false;
    menu->activity_count = 0;
}

void break_menu_add_activity(BreakMenu* menu, const char* name, const char* description, int duration, bool is_quick_break) {
    if (menu->activity_count >= MAX_BREAK_ACTIVITIES) return;
    
    // Copy activity data
    BreakActivity* activity = &menu->activities[menu->activity_count];
    strncpy(activity->name, name, MAX_LABEL_LENGTH - 1);
    activity->name[MAX_LABEL_LENGTH - 1] = '\0';
    
    strncpy(activity->description, description, MAX_LABEL_LENGTH - 1);
    activity->description[MAX_LABEL_LENGTH - 1] = '\0';
    
    activity->duration = duration;
    activity->is_quick_break = is_quick_break;
    
    menu->activity_count++;
}

void break_menu_update(BreakMenu* menu, float dt) {
    if (menu->break_active && menu->remaining_time > 0) {
        menu->remaining_time -= dt;
        
        if (menu->remaining_time <= 0) {
            menu->remaining_time = 0;
            menu->break_active = false;
            
            // Could add notification or sound here
        }
    }
}

void break_menu_toggle(BreakMenu* menu) {
    menu->menu_visible = !menu->menu_visible;
    
    // Reset selection when opening menu
    if (menu->menu_visible) {
        menu->selected_activity = 0;
    }
}

void break_menu_select(BreakMenu* menu, int32_t index) {
    if (index >= 0 && index < menu->activity_count) {
        menu->selected_activity = index;
    }
}

void break_menu_start_break(BreakMenu* menu) {
    if (menu->selected_activity >= 0 && menu->selected_activity < menu->activity_count) {
        BreakActivity* activity = &menu->activities[menu->selected_activity];
        menu->remaining_time = activity->duration;
        menu->break_active = true;
        
        // Could add notification or sound here
        printf("Starting %s break for %d minutes\n", 
               activity->name, 
               activity->duration / 60);
    }
}

void break_menu_end_break(BreakMenu* menu) {
    menu->break_active = false;
    menu->remaining_time = 0;
    menu->menu_visible = false;
    
    // Could add notification or sound here
    printf("Break ended early\n");
} 