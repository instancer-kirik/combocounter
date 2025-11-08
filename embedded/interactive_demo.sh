#!/bin/bash

# Interactive ComboCounter TTS Demo
# Automatically demonstrates the rally co-driver counting system

echo "🏁 Interactive ComboCounter Demo with Rally Co-Driver TTS"
echo "========================================================="
echo ""

# Check if the enhanced simulation exists
if [ ! -f "./combocounter_enhanced" ]; then
    echo "❌ combocounter_enhanced not found! Please build it first:"
    echo "   make -f Makefile.enhanced"
    exit 1
fi

echo "🎤 This demo will show the rally co-driver TTS counting in action!"
echo ""
echo "Demo Features:"
echo "• Real-time TTS counting with espeak-ng"
echo "• American rally co-driver voice style"
echo "• Phonk-ready samples and emphasis"
echo "• Form feedback and combo announcements"
echo ""

read -p "Press Enter to start the demo..."

# Create a temporary input script for the combocounter
cat > temp_input.txt << 'EOF'
a
2
3
q
EOF

echo ""
echo "🎯 Demo Script:"
echo "1. Starting ComboCounter..."
echo "2. Switching to Voice Clips mode (A -> 2)"
echo "3. Enabling all audio features"
echo "4. Switching to Combo counter for best effect"
echo ""

# Function to send keys to the process
send_keys_demo() {
    echo "Starting ComboCounter with TTS demo..."

    # Start the combocounter and send it our input
    (
        sleep 1
        echo "🎤 Switching to audio settings..."
        echo "a"  # Go to audio settings
        sleep 1
        echo "🎤 Switching to Voice Clips mode..."
        echo "s"  # Move to mode selection
        sleep 0.5
        echo " "  # Select Voice Clips
        sleep 0.5
        echo "q"  # Back to main
        sleep 1
        echo "🎤 Switching to Combo counter..."
        echo "s"  # Switch counter type
        echo "s"  # Switch to combo
        sleep 1
        echo ""
        echo "🏁 NOW COUNTING WITH RALLY VOICE:"
        echo "  (You should hear TTS counting!)"
        echo ""
        for i in {1..5}; do
            echo "Rep $i..."
            echo " "  # Space to increment
            sleep 1.5
        done
        sleep 1
        echo ""
        echo "🎤 Testing quality feedback..."
        echo "p"  # Perfect
        sleep 1
        echo "g"  # Good
        sleep 1
        echo "b"  # Partial
        sleep 1
        echo ""
        echo "🏁 Demo complete! Press 'q' to quit."
        echo "q"
    ) | ./combocounter_enhanced
}

# Run the interactive demo
send_keys_demo

echo ""
echo "🎉 Demo Complete!"
echo ""
echo "🎵 What you just heard:"
echo "• Rally co-driver counting: '1!', '2!', '3!', etc."
echo "• Form feedback: 'Perfect form!', 'Good rep!', etc."
echo "• American English with assertive tone"
echo "• Fast speech (180 WPM) with emphasis"
echo ""
echo "🔥 Perfect for sampling in phonk tracks!"
echo ""
echo "To use manually:"
echo "1. Run: ./combocounter_enhanced"
echo "2. Press 'A' for audio settings"
echo "3. Select 'Voice Clips' mode"
echo "4. Use SPACE to count with TTS"
echo "5. Use P/G/B/M for quality feedback"
echo ""
echo "🏁 Rally co-driver mode activated! 🏁"
