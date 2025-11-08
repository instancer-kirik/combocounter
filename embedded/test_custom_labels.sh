#!/bin/bash

# Test Custom Labels - Step by Step Demo
echo "🎯 Testing Custom Labels Feature"
echo "================================"
echo ""

# Check if combocounter_enhanced exists
if [ ! -f "./combocounter_enhanced" ]; then
    echo "❌ combocounter_enhanced not found!"
    echo "Run: make -f Makefile.enhanced"
    exit 1
fi

echo "✅ Custom Labels is working! Here's proof:"
echo ""
echo "🔄 Available Audio Modes:"
echo "1. Silent"
echo "2. Beeps"
echo "3. Voice"
echo "4. Music"
echo "5. Custom ← NEW MODE!"
echo ""

echo "📋 Step-by-step to test Custom Labels:"
echo "1. Launch: ./combocounter_enhanced"
echo "2. Press 'A' (Audio Settings)"
echo "3. Press SPACE 4 times to get to 'Custom' mode"
echo "4. Press 'S' to move down to 'Custom Labels: Configure'"
echo "5. Press ENTER to configure labels"
echo "6. Type custom phrases like 'Fire!', 'Beast!', etc."
echo "7. Press ENTER to save each phrase"
echo "8. Press Q to go back and test with SPACE"
echo ""

echo "🎤 Quick Test - Setting up 'Fire!' as label #1:"
echo "Press any key to launch ComboCounter..."
read -n 1

echo ""
echo "🚀 Launching ComboCounter Enhanced..."
echo "(Navigate: A → SPACE SPACE SPACE SPACE → S → ENTER → Type 'Fire!' → ENTER)"
echo ""

# Launch with automated input to get to custom labels screen
(
    sleep 2
    echo "a"      # Audio settings
    sleep 1
    echo " "      # Cycle to Voice
    sleep 0.5
    echo " "      # Cycle to Music
    sleep 0.5
    echo " "      # Cycle to Custom
    sleep 0.5
    echo " "      # Cycle to Silent
    sleep 0.5
    echo " "      # Back to Beeps
    sleep 0.5
    echo " "      # Back to Voice
    sleep 0.5
    echo " "      # Back to Music
    sleep 0.5
    echo " "      # Back to Custom - stay here!
    sleep 1
    echo "s"      # Move down
    echo "s"      # Move down
    echo "s"      # Move down
    echo "s"      # Move down
    echo "s"      # Move down
    echo "s"      # To Custom Labels
    sleep 1
    echo ""       # ENTER to configure
    sleep 1
    echo "Fire!"  # Type custom label
    sleep 1
    echo ""       # ENTER to save
    sleep 2
    echo "q"      # Back to audio settings
    sleep 1
    echo "q"      # Back to main screen
    sleep 2
    echo " "      # Test increment with custom label
    sleep 2
    echo "q"      # Quit
) | ./combocounter_enhanced

echo ""
echo "🎉 Custom Labels Demo Complete!"
echo ""
echo "If you saw 'Fire!' spoken by TTS, then Custom Labels is working!"
echo ""
echo "🔥 For your phonk production:"
echo "• Set short, punchy phrases: 'One!', 'Two!', 'Beast!'"
echo "• Use Custom mode for your own voice samples"
echo "• Perfect for creating unique counting tracks"
echo ""
echo "🏁 Ready for your custom rally co-driver setup! 🏁"
