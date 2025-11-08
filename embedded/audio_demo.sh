#!/bin/bash

# ComboCounter Audio Demo Script
# Tests the rally co-driver style TTS counting system

echo "🏁 ComboCounter Audio Demo - Rally Co-Driver Style"
echo "=================================================="
echo ""

# Check if espeak-ng is available
if ! command -v espeak-ng &> /dev/null; then
    echo "❌ espeak-ng not found! Please install it first."
    exit 1
fi

echo "🎤 Testing Rally Co-Driver Voice Synthesis..."
echo ""

# Test basic counting (1-10)
echo "📢 Demo 1: Basic Counting (Rally Style)"
echo "---------------------------------------"
for i in {1..10}; do
    echo "Count: $i"
    espeak-ng -v en-us -s 180 -p 35 -a 75 -g 8 "$i!" 2>/dev/null
    sleep 0.8
done
echo ""

# Test quality feedback
echo "📢 Demo 2: Form Feedback"
echo "------------------------"
feedback=("Perfect form!" "Good rep!" "Keep pushing!" "Reset and go!")
for phrase in "${feedback[@]}"; do
    echo "Feedback: $phrase"
    espeak-ng -v en-us -s 170 -p 30 -a 70 -g 10 "$phrase" 2>/dev/null
    sleep 1.2
done
echo ""

# Test combo milestones
echo "📢 Demo 3: Combo Milestones"
echo "---------------------------"
milestones=("Ten combo! Building momentum!" "Twenty five combo! On fire!" "Fifty combo! Unstoppable!" "Hundred combo! Legendary!")
for milestone in "${milestones[@]}"; do
    echo "Milestone: $milestone"
    espeak-ng -v en-us -s 180 -p 35 -a 75 -g 8 "$milestone" 2>/dev/null
    sleep 2
done
echo ""

# Test combo events
echo "📢 Demo 4: Combo Events"
echo "-----------------------"
events=("Combo broken! Back to one!" "New personal record! Outstanding!")
for event in "${events[@]}"; do
    echo "Event: $event"
    espeak-ng -v en-us -s 170 -p 30 -a 70 -g 10 "$event" 2>/dev/null
    sleep 1.5
done
echo ""

echo "🎯 Demo Complete! Now testing with actual ComboCounter..."
echo ""
echo "Instructions:"
echo "1. Run: ./combocounter_enhanced"
echo "2. Press 'A' for audio settings"
echo "3. Switch to 'Voice Clips' mode"
echo "4. Press SPACE to count with TTS"
echo "5. Press 'P' for Perfect, 'G' for Good, etc."
echo "6. Switch to Combo counter with 'W/S' for combo effects"
echo ""
echo "🏁 Rally Co-Driver Features:"
echo "   • American English voice (en-us)"
echo "   • Fast, assertive counting (180 wpm)"
echo "   • Higher pitch for urgency (60+ Hz)"
echo "   • Phonk-style emphasis in Music mode"
echo "   • Real-time TTS with espeak-ng"
