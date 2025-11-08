#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/core.h"

int main() {
    printf("Creating test data for Combo Chracker GUI...\n");
    
    // Create some sample trackers with different states
    ComboState trackers[4];
    
    // Tracker 1: Push-ups tracker with some progress
    combo_init(&trackers[0], "Push-ups");
    combo_resume(&trackers[0]);
    combo_increment(&trackers[0], 25);
    combo_increment(&trackers[0], 10);
    combo_pause(&trackers[0]);
    printf("Created tracker: %s (Score: %d, Combo: %d)\n", 
           trackers[0].label, trackers[0].score, trackers[0].combo);
    
    // Tracker 2: Reading tracker - currently active
    combo_init(&trackers[1], "Reading Minutes");
    combo_resume(&trackers[1]);
    combo_increment(&trackers[1], 45);
    combo_increment(&trackers[1], 30);
    combo_increment(&trackers[1], 15);
    // Leave this one active (not paused)
    printf("Created tracker: %s (Score: %d, Combo: %d, Active: %s)\n", 
           trackers[1].label, trackers[1].score, trackers[1].combo, 
           trackers[1].paused ? "No" : "Yes");
    
    // Tracker 3: Water intake tracker
    combo_init(&trackers[2], "Water Glasses");
    combo_resume(&trackers[2]);
    combo_increment(&trackers[2], 8);
    combo_pause(&trackers[2]);
    printf("Created tracker: %s (Score: %d, Combo: %d)\n", 
           trackers[2].label, trackers[2].score, trackers[2].combo);
    
    // Tracker 4: Meditation tracker - new with no progress
    combo_init(&trackers[3], "Meditation");
    printf("Created tracker: %s (Score: %d, Combo: %d)\n", 
           trackers[3].label, trackers[3].score, trackers[3].combo);
    
    // Save all trackers to the file that the GUI app will load
    combo_save_all_trackers(trackers, 4, "combo_trackers.dat");
    printf("\n✓ Saved 4 test trackers to combo_trackers.dat\n");
    
    // Also create a backup file for testing
    combo_save_all_trackers(trackers, 4, "combo_trackers_backup.dat");
    printf("✓ Created backup file combo_trackers_backup.dat\n");
    
    // Verify the save worked by loading and displaying
    printf("\nVerifying saved data by loading it back:\n");
    ComboState loaded_trackers[8];
    memset(loaded_trackers, 0, sizeof(loaded_trackers));
    int loaded_count = combo_load_all_trackers(loaded_trackers, 8, "combo_trackers.dat");
    
    printf("Loaded %d trackers:\n", loaded_count);
    for (int i = 0; i < loaded_count; i++) {
        printf("  %d. %s - Score: %d, Combo: %d, Status: %s\n", 
               i + 1,
               loaded_trackers[i].label, 
               loaded_trackers[i].score, 
               loaded_trackers[i].combo,
               loaded_trackers[i].paused ? "Paused" : "Active");
    }
    
    // Cleanup
    for (int i = 0; i < loaded_count; i++) {
        if (loaded_trackers[i].objectives) {
            free(loaded_trackers[i].objectives);
        }
    }
    
    printf("\n🎉 Test data created successfully!\n");
    printf("Now you can run the Combo Chracker GUI app and it should load these 4 trackers.\n");
    printf("Try adding new trackers, incrementing/decrementing counters, and restarting the app\n");
    printf("to verify that persistence is working correctly.\n");
    
    return 0;
}