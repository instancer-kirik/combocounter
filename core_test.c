#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "src/core.h"

int main() {
    printf("Testing core functionality...\n");
    
    // Test 1: Basic initialization
    printf("Test 1: Basic initialization\n");
    ComboState tracker;
    combo_init(&tracker, "Test");
    printf("  Label: %s\n", tracker.label);
    printf("  Score: %d\n", tracker.score);
    printf("  Paused: %d\n", tracker.paused);
    assert(strcmp(tracker.label, "Test") == 0);
    assert(tracker.score == 0);
    assert(tracker.paused == true);
    printf("  ✓ Initialization test passed\n");
    
    // Test 2: Resume and increment
    printf("Test 2: Resume and increment\n");
    combo_resume(&tracker);
    printf("  After resume - Paused: %d\n", tracker.paused);
    assert(tracker.paused == false);
    
    combo_increment(&tracker, 5);
    printf("  After increment - Score: %d, Combo: %d\n", tracker.score, tracker.combo);
    assert(tracker.score == 5);
    assert(tracker.combo == 1);
    printf("  ✓ Resume and increment test passed\n");
    
    // Test 3: Multiple trackers array
    printf("Test 3: Multiple trackers\n");
    ComboState trackers[3];
    combo_init(&trackers[0], "Tracker A");
    combo_init(&trackers[1], "Tracker B");
    combo_init(&trackers[2], "Tracker C");
    
    combo_resume(&trackers[0]);
    combo_increment(&trackers[0], 10);
    
    combo_resume(&trackers[1]);
    combo_increment(&trackers[1], 20);
    combo_increment(&trackers[1], 5);
    
    printf("  Tracker A: %s, Score: %d\n", trackers[0].label, trackers[0].score);
    printf("  Tracker B: %s, Score: %d\n", trackers[1].label, trackers[1].score);
    printf("  Tracker C: %s, Score: %d\n", trackers[2].label, trackers[2].score);
    
    assert(trackers[0].score == 10);
    assert(trackers[1].score == 25);
    assert(trackers[2].score == 0);
    printf("  ✓ Multiple trackers test passed\n");
    
    // Test 4: Save/load multiple trackers
    printf("Test 4: Save and load multiple trackers\n");
    combo_save_all_trackers(trackers, 3, "test_core.dat");
    printf("  Saved 3 trackers to file\n");
    
    // Clear the array and reload
    ComboState loaded_trackers[8];
    memset(loaded_trackers, 0, sizeof(loaded_trackers));
    int loaded_count = combo_load_all_trackers(loaded_trackers, 8, "test_core.dat");
    
    printf("  Loaded %d trackers from file\n", loaded_count);
    assert(loaded_count == 3);
    
    for (int i = 0; i < loaded_count; i++) {
        printf("  Loaded Tracker %d: %s, Score: %d\n", i, loaded_trackers[i].label, loaded_trackers[i].score);
        assert(strcmp(trackers[i].label, loaded_trackers[i].label) == 0);
        assert(trackers[i].score == loaded_trackers[i].score);
    }
    printf("  ✓ Save/load test passed\n");
    
    // Cleanup
    for (int i = 0; i < loaded_count; i++) {
        if (loaded_trackers[i].objectives) {
            free(loaded_trackers[i].objectives);
        }
    }
    
    // Remove test file
    remove("test_core.dat");
    
    printf("\n🎉 All core tests passed!\n");
    return 0;
}