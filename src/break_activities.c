#include "break_activities.h"
#include <stddef.h>

const BreakActivity default_activities[] = {
    {
        .name = "Quick Stretch",
        .description = "Take a quick stretch break",
        .duration = 300,  // 5 minutes
        .is_quick_break = true
    },
    {
        .name = "Walk Break",
        .description = "Take a short walk",
        .duration = 900,  // 15 minutes
        .is_quick_break = false
    },
    {
        .name = "Meditation",
        .description = "Clear your mind with meditation",
        .duration = 600,  // 10 minutes
        .is_quick_break = true
    },
    {
        .name = "Lunch Break",
        .description = "Take a proper lunch break",
        .duration = 3600,  // 1 hour
        .is_quick_break = false
    }
};

const size_t default_activities_count = sizeof(default_activities) / sizeof(default_activities[0]); 