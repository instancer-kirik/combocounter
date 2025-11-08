#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "src/core.h"

void test_single_tracker_save_load() {
    printf("Testing single tracker save/load...\n");
    
    // Create and setup a tracker
    ComboState original;
    printf("  Step 1: Creating tracker...\n");
    fflush(stdout);
    combo_init(&original, "Test Counter");
    printf("  Step 2: Tracker created, label='%s'\n", original.label);
    fflush(stdout);
    
    printf("  Step 3: Resuming tracker...\n");
    fflush(stdout);
    combo_resume(&original);
    printf("  Step 4: Tracker resumed, paused=%d\n", original.paused);
    fflush(stdout);
    
    printf("  Step 5: Incrementing tracker first time...\n");
    fflush(stdout);
    combo_increment(&original, 5);
    printf("  Step 6: After first increment: score=%d, combo=%d\n", original.score, original.combo);
    fflush(stdout);
    
    printf("  Step 7: Incrementing tracker second time...\n");
    fflush(stdout);
    combo_increment(&original, 3);
    printf("  Step 8: After second increment: score=%d, combo=%d\n", original.score, original.combo);
    fflush(stdout);
    
    printf("  Step 9: Pausing tracker...\n");
    fflush(stdout);
    combo_pause(&original);
    printf("  Step 10: Original stats: score=%d, combo=%d, paused=%d\n", original.score, original.combo, original.paused);
    fflush(stdout);
    
    // Save the tracker
    printf("  Saving tracker to file...\n");
    combo_save_state(&original, "test_single.dat");
    
    // Create a new tracker and load
    ComboState loaded;
    memset(&loaded, 0, sizeof(ComboState));
    printf("  Loading tracker from file...\n");
    combo_load_state(&loaded, "test_single.dat");
    printf("  Loaded stats: label=%s, score=%d, combo=%d, paused=%d\n", loaded.label, loaded.score, loaded.combo, loaded.paused);
    
    // Verify the data was loaded correctly
    printf("  Verifying data...\n");
    assert(strcmp(original.label, loaded.label) == 0);
    assert(original.score == loaded.score);
    assert(original.combo == loaded.combo);
    assert(original.max_combo == loaded.max_combo);
    assert(original.paused == loaded.paused);
    assert(original.total_hits == loaded.total_hits);
    assert(original.perfect_hits == loaded.perfect_hits);
    
    printf("✓ Single tracker save/load test passed!\n");
    
    // Cleanup
    if (loaded.objectives) {
        free(loaded.objectives);
    }
}

void test_multiple_trackers_save_load() {
    printf("Testing multiple trackers save/load...\n");
    
    // Create multiple trackers
    ComboState trackers[3];
    combo_init(&trackers[0], "Counter 1");
    combo_init(&trackers[1], "Counter 2"); 
    combo_init(&trackers[2], "Counter 3");
    
    // Set different values for each
    combo_resume(&trackers[0]);
    combo_increment(&trackers[0], 10);
    combo_pause(&trackers[0]);
    
    combo_resume(&trackers[1]);
    combo_increment(&trackers[1], 5);
    combo_increment(&trackers[1], 7);
    
    combo_resume(&trackers[2]);
    combo_increment(&trackers[2], 1);
    combo_pause(&trackers[2]);
    
    // Save all trackers
    combo_save_all_trackers(trackers, 3, "test_multiple.dat");
    
    // Load into new array
    ComboState loaded_trackers[8]; // MAX_TRACKERS size
    memset(loaded_trackers, 0, sizeof(loaded_trackers));
    int loaded_count = combo_load_all_trackers(loaded_trackers, 8, "test_multiple.dat");
    
    // Verify count
    assert(loaded_count == 3);
    
    // Verify each tracker
    for (int i = 0; i < 3; i++) {
        assert(strcmp(trackers[i].label, loaded_trackers[i].label) == 0);
        assert(trackers[i].score == loaded_trackers[i].score);
        assert(trackers[i].combo == loaded_trackers[i].combo);
        assert(trackers[i].max_combo == loaded_trackers[i].max_combo);
        assert(trackers[i].paused == loaded_trackers[i].paused);
        assert(trackers[i].total_hits == loaded_trackers[i].total_hits);
        assert(trackers[i].perfect_hits == loaded_trackers[i].perfect_hits);
        
        printf("✓ Tracker %d: %s (score: %d, combo: %d)\n", 
               i, loaded_trackers[i].label, loaded_trackers[i].score, loaded_trackers[i].combo);
    }
    
    printf("✓ Multiple trackers save/load test passed!\n");
    
    // Cleanup
    for (int i = 0; i < loaded_count; i++) {
        if (loaded_trackers[i].objectives) {
            free(loaded_trackers[i].objectives);
        }
    }
}

void test_empty_file_load() {
    printf("Testing load from non-existent file...\n");
    
    ComboState trackers[8];
    memset(trackers, 0, sizeof(trackers));
    
    // Try to load from non-existent file
    int loaded_count = combo_load_all_trackers(trackers, 8, "nonexistent.dat");
    
    // Should return 0
    assert(loaded_count == 0);
    
    printf("✓ Empty file load test passed!\n");
}

void test_persistence_simulation() {
    printf("Testing app restart simulation...\n");
    
    // Simulate app session 1
    {
        ComboState trackers[8];
        memset(trackers, 0, sizeof(trackers));
        
        // Initialize some trackers like the app would
        combo_init(&trackers[0], "Push-ups");
        combo_init(&trackers[1], "Reading");
        
        // Use them
        combo_resume(&trackers[0]);
        combo_increment(&trackers[0], 20); // Did 20 push-ups
        combo_pause(&trackers[0]);
        
        combo_resume(&trackers[1]);
        combo_increment(&trackers[1], 30); // Read for 30 minutes
        
        // Save on "app exit"
        combo_save_all_trackers(trackers, 2, "app_session.dat");
        printf("✓ App session 1: Saved 2 trackers\n");
    }
    
    // Simulate app session 2 (restart)
    {
        ComboState trackers[8];
        memset(trackers, 0, sizeof(trackers));
        
        // Load on "app start"
        int count = combo_load_all_trackers(trackers, 8, "app_session.dat");
        assert(count == 2);
        
        printf("✓ App session 2: Loaded %d trackers\n", count);
        printf("  - %s: score %d, %s\n", 
               trackers[0].label, trackers[0].score, trackers[0].paused ? "paused" : "active");
        printf("  - %s: score %d, %s\n", 
               trackers[1].label, trackers[1].score, trackers[1].paused ? "paused" : "active");
        
        // Continue using them
        combo_increment(&trackers[1], 15); // Read 15 more minutes
        combo_resume(&trackers[0]);
        combo_increment(&trackers[0], 10); // 10 more push-ups
        
        // Add a new tracker
        combo_init(&trackers[2], "Water");
        combo_resume(&trackers[2]);
        combo_increment(&trackers[2], 8); // 8 glasses of water
        
        // Save again
        combo_save_all_trackers(trackers, 3, "app_session.dat");
        printf("✓ App session 2: Saved 3 trackers\n");
        
        // Cleanup
        for (int i = 0; i < count + 1; i++) {
            if (trackers[i].objectives) {
                free(trackers[i].objectives);
            }
        }
    }
    
    // Simulate app session 3 (another restart)
    {
        ComboState trackers[8];
        memset(trackers, 0, sizeof(trackers));
        
        // Load on "app start"
        int count = combo_load_all_trackers(trackers, 8, "app_session.dat");
        assert(count == 3);
        
        printf("✓ App session 3: Loaded %d trackers\n", count);
        for (int i = 0; i < count; i++) {
            printf("  - %s: score %d\n", trackers[i].label, trackers[i].score);
        }
        
        // Verify expected values
        assert(trackers[0].score == 30); // 20 + 10 push-ups
        assert(trackers[1].score == 45); // 30 + 15 reading
        assert(trackers[2].score == 8);  // 8 water
        
        // Cleanup
        for (int i = 0; i < count; i++) {
            if (trackers[i].objectives) {
                free(trackers[i].objectives);
            }
        }
    }
    
    printf("✓ App restart simulation test passed!\n");
}

int main() {
    printf("Running ComboCounter save/load tests...\n\n");
    
    test_single_tracker_save_load();
    printf("\n");
    
    test_multiple_trackers_save_load();
    printf("\n");
    
    test_empty_file_load();
    printf("\n");
    
    test_persistence_simulation();
    printf("\n");
    
    printf("🎉 All tests passed! Save/load functionality is working correctly.\n");
    
    // Cleanup test files
    remove("test_single.dat");
    remove("test_multiple.dat");
    remove("app_session.dat");
    
    return 0;
}