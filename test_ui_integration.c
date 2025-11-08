#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "src/ui.h"
#include "src/core.h"

void test_ui_initialization() {
    printf("Test 1: UI Initialization\n");
    
    ComboUI ui;
    memset(&ui, 0, sizeof(ComboUI));
    init_ui(&ui);
    
    assert(ui.tracker_count == 0);
    assert(ui.tracker_form.form_visible == false);
    assert(ui.interval_form.form_visible == false);
    assert(ui.break_menu.visible == false);
    
    printf("  ✓ UI initialized correctly\n");
}

void test_ui_save_load_empty() {
    printf("Test 2: Save/Load with no trackers\n");
    
    ComboUI ui;
    memset(&ui, 0, sizeof(ComboUI));
    init_ui(&ui);
    
    // Save empty state
    save_ui_state(&ui);
    
    // Create new UI and load
    ComboUI ui2;
    memset(&ui2, 0, sizeof(ComboUI));
    init_ui(&ui2);
    load_ui_state(&ui2);
    
    assert(ui2.tracker_count == 0);
    printf("  ✓ Empty UI state save/load works correctly\n");
}

void test_ui_add_tracker() {
    printf("Test 3: Adding trackers through UI\n");
    
    ComboUI ui;
    memset(&ui, 0, sizeof(ComboUI));
    init_ui(&ui);
    
    // Simulate adding a tracker
    strcpy(ui.tracker_form.label_buffer, "Test Tracker");
    ui.tracker_form.form_visible = true;
    add_new_tracker(&ui);
    
    assert(ui.tracker_count == 1);
    assert(strcmp(ui.trackers[0].label, "Test Tracker") == 0);
    assert(ui.tracker_form.form_visible == false);
    printf("  ✓ Added tracker: %s\n", ui.trackers[0].label);
    
    // Add another tracker
    strcpy(ui.tracker_form.label_buffer, "Second Tracker");
    ui.tracker_form.form_visible = true;
    add_new_tracker(&ui);
    
    assert(ui.tracker_count == 2);
    assert(strcmp(ui.trackers[1].label, "Second Tracker") == 0);
    printf("  ✓ Added second tracker: %s\n", ui.trackers[1].label);
}

void test_ui_persistence_cycle() {
    printf("Test 4: Complete persistence cycle\n");
    
    // Session 1: Create and use trackers
    {
        ComboUI ui;
        memset(&ui, 0, sizeof(ComboUI));
        init_ui(&ui);
        
        // Add trackers
        strcpy(ui.tracker_form.label_buffer, "Push-ups");
        add_new_tracker(&ui);
        
        strcpy(ui.tracker_form.label_buffer, "Reading");
        add_new_tracker(&ui);
        
        // Use the trackers
        combo_resume(&ui.trackers[0]);
        combo_increment(&ui.trackers[0], 25);
        combo_pause(&ui.trackers[0]);
        
        combo_resume(&ui.trackers[1]);
        combo_increment(&ui.trackers[1], 45);
        // Leave reading tracker active
        
        printf("  Session 1 - Created 2 trackers:\n");
        printf("    %s: Score %d, %s\n", ui.trackers[0].label, ui.trackers[0].score, 
               ui.trackers[0].paused ? "Paused" : "Active");
        printf("    %s: Score %d, %s\n", ui.trackers[1].label, ui.trackers[1].score,
               ui.trackers[1].paused ? "Paused" : "Active");
        
        // Save state (this should happen automatically when trackers are added)
        save_ui_state(&ui);
    }
    
    // Session 2: Load and verify
    {
        ComboUI ui;
        memset(&ui, 0, sizeof(ComboUI));
        init_ui(&ui);
        load_ui_state(&ui);
        
        assert(ui.tracker_count == 2);
        assert(strcmp(ui.trackers[0].label, "Push-ups") == 0);
        assert(strcmp(ui.trackers[1].label, "Reading") == 0);
        assert(ui.trackers[0].score == 25);
        assert(ui.trackers[1].score == 45);
        assert(ui.trackers[0].paused == true);
        assert(ui.trackers[1].paused == false);
        
        printf("  Session 2 - Loaded 2 trackers:\n");
        printf("    %s: Score %d, %s ✓\n", ui.trackers[0].label, ui.trackers[0].score,
               ui.trackers[0].paused ? "Paused" : "Active");
        printf("    %s: Score %d, %s ✓\n", ui.trackers[1].label, ui.trackers[1].score,
               ui.trackers[1].paused ? "Paused" : "Active");
        
        // Continue using trackers
        combo_increment(&ui.trackers[0], 10); // Won't work, it's paused
        combo_increment(&ui.trackers[1], 15); // Will work, it's active
        
        // Add a new tracker
        strcpy(ui.tracker_form.label_buffer, "Water");
        add_new_tracker(&ui);
        combo_resume(&ui.trackers[2]);
        combo_increment(&ui.trackers[2], 8);
        
        assert(ui.tracker_count == 3);
        assert(ui.trackers[0].score == 25); // Unchanged (paused)
        assert(ui.trackers[1].score == 60); // Increased
        assert(ui.trackers[2].score == 8);  // New tracker
        
        printf("  Session 2 - After updates:\n");
        printf("    %s: Score %d (no change - paused)\n", ui.trackers[0].label, ui.trackers[0].score);
        printf("    %s: Score %d (increased)\n", ui.trackers[1].label, ui.trackers[1].score);
        printf("    %s: Score %d (new tracker)\n", ui.trackers[2].label, ui.trackers[2].score);
        
        save_ui_state(&ui);
        
        // Cleanup
        for (int i = 0; i < ui.tracker_count; i++) {
            if (ui.trackers[i].objectives) {
                free(ui.trackers[i].objectives);
            }
        }
    }
    
    // Session 3: Final verification
    {
        ComboUI ui;
        memset(&ui, 0, sizeof(ComboUI));
        init_ui(&ui);
        load_ui_state(&ui);
        
        assert(ui.tracker_count == 3);
        assert(ui.trackers[0].score == 25);
        assert(ui.trackers[1].score == 60);
        assert(ui.trackers[2].score == 8);
        
        printf("  Session 3 - Final verification:\n");
        printf("    All 3 trackers loaded with correct scores ✓\n");
        
        // Cleanup
        for (int i = 0; i < ui.tracker_count; i++) {
            if (ui.trackers[i].objectives) {
                free(ui.trackers[i].objectives);
            }
        }
    }
    
    printf("  ✓ Complete persistence cycle test passed\n");
}

void test_error_handling() {
    printf("Test 5: Error handling\n");
    
    ComboUI ui;
    memset(&ui, 0, sizeof(ComboUI));
    init_ui(&ui);
    
    // Test empty label
    memset(ui.tracker_form.label_buffer, 0, sizeof(ui.tracker_form.label_buffer));
    ui.tracker_form.form_visible = true;
    int original_count = ui.tracker_count;
    add_new_tracker(&ui);
    
    assert(ui.tracker_count == original_count); // Should not have added
    assert(ui.tracker_form.error_visible == true);
    assert(ui.tracker_form.error_message != NULL);
    printf("  ✓ Empty label error handling works\n");
    
    // Test maximum trackers
    ui.tracker_count = MAX_TRACKERS;
    strcpy(ui.tracker_form.label_buffer, "Too Many");
    ui.tracker_form.error_visible = false;
    add_new_tracker(&ui);
    
    assert(ui.tracker_count == MAX_TRACKERS);
    assert(ui.tracker_form.error_visible == true);
    printf("  ✓ Maximum trackers error handling works\n");
}

void test_state_consistency() {
    printf("Test 6: State consistency across operations\n");
    
    ComboUI ui;
    memset(&ui, 0, sizeof(ComboUI));
    init_ui(&ui);
    
    // Add a tracker and modify it
    strcpy(ui.tracker_form.label_buffer, "Consistency Test");
    add_new_tracker(&ui);
    
    ComboState* tracker = &ui.trackers[0];
    combo_resume(tracker);
    
    // Perform various operations
    combo_increment(tracker, 10);
    combo_increment(tracker, 5);
    assert(tracker->score == 15);
    assert(tracker->combo == 2);
    assert(tracker->max_combo == 2);
    assert(tracker->total_hits == 2);
    
    combo_decrement(tracker, 3);
    assert(tracker->score == 12);
    assert(tracker->combo == 0); // Should reset combo
    assert(tracker->max_combo == 2); // Should preserve max
    
    combo_pause(tracker);
    assert(tracker->paused == true);
    
    // Try operations while paused
    int score_before_pause = tracker->score;
    combo_increment(tracker, 100); // Should not work
    assert(tracker->score == score_before_pause);
    
    printf("  ✓ State consistency maintained across all operations\n");
}

int main() {
    printf("🧪 ComboCounter UI Integration Tests\n");
    printf("=====================================\n\n");
    
    test_ui_initialization();
    printf("\n");
    
    test_ui_save_load_empty();
    printf("\n");
    
    test_ui_add_tracker();
    printf("\n");
    
    test_ui_persistence_cycle();
    printf("\n");
    
    test_error_handling();
    printf("\n");
    
    test_state_consistency();
    printf("\n");
    
    printf("🎉 All UI integration tests passed!\n");
    printf("\nSummary:\n");
    printf("✓ UI initialization works correctly\n");
    printf("✓ Save/load functionality is working\n");
    printf("✓ Tracker addition through UI works\n");
    printf("✓ Complete app restart simulation works\n");
    printf("✓ Error handling is robust\n");
    printf("✓ State consistency is maintained\n");
    printf("\nThe counter state saving and loading issue has been resolved!\n");
    printf("New custom counters will now persist across application restarts.\n");
    
    // Cleanup test files
    remove("combo_trackers.dat");
    
    return 0;
}