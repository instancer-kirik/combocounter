#!/bin/bash

# Custom Labels Demo Script for ComboCounter
# Demonstrates user-defined counting phrases and quality feedback

echo "✏️ ComboCounter Custom Labels Demo"
echo "=================================="
echo ""

# Check if the enhanced simulation exists
if [ ! -f "./combocounter_enhanced" ]; then
    echo "❌ combocounter_enhanced not found! Please build it first:"
    echo "   make -f Makefile.enhanced"
    exit 1
fi

echo "🎤 This demo showcases customizable counting labels!"
echo ""
echo "Features:"
echo "• User-defined counting phrases (1-10)"
echo "• Custom quality feedback labels"
echo "• Perfect for phonk production with your own samples"
echo "• Rally co-driver TTS speaks YOUR custom phrases"
echo ""

echo "Example Custom Labels:"
echo "• Counting: 'One rep!', 'Two reps!', 'Three strong!' etc."
echo "• Quality: 'Beast mode!', 'Solid work!', 'Push harder!', 'Reset bro!'"
echo "• Phonk style: 'One!', 'Two!', 'Three!', 'Fire!', 'Clean!', 'Mid!', 'Trash!'"
echo ""

read -p "Press Enter to start the demo..."

echo ""
echo "🎯 Demo Instructions:"
echo "1. Starting ComboCounter Enhanced..."
echo "2. Navigate to Audio Settings (A)"
echo "3. Switch to Custom Labels mode"
echo "4. Configure your own phrases"
echo "5. Test with TTS voice synthesis"
echo ""

echo "📋 Quick Setup Guide:"
echo "─────────────────────"
echo "• A = Audio Settings"
echo "• S = Move down to 'Audio Mode'"
echo "• SPACE = Cycle to 'Custom' mode"
echo "• S = Move to 'Custom Labels'"
echo "• ENTER = Configure labels"
echo "• Type your custom phrase + ENTER to save"
echo "• TAB = Switch between Counting/Quality labels"
echo "• Q = Back to main screen"
echo ""

echo "💡 Custom Label Ideas:"
echo "• Phonk style: 'One!', 'Two!', 'Three!' (short, punchy)"
echo "• Motivational: 'Beast!', 'Strong!', 'Power!'"
echo "• Gym bro: 'Rep one!', 'Keep going!', 'Almost there!'"
echo "• Rally style: 'Mark one!', 'Mark two!', 'Mark three!'"
echo "• Gaming: 'Frag one!', 'Double kill!', 'Triple kill!'"
echo ""

echo "🔥 For Phonk Production:"
echo "Set short, aggressive phrases perfect for sampling:"
echo "1: 'One!'     6: 'Six!'"
echo "2: 'Two!'     7: 'Seven!'"
echo "3: 'Three!'   8: 'Eight!'"
echo "4: 'Four!'    9: 'Nine!'"
echo "5: 'Five!'    10: 'Ten!'"
echo ""
echo "Quality feedback:"
echo "Perfect: 'Fire!'    Partial: 'Mid!'"
echo "Good: 'Clean!'      Miss: 'Trash!'"
echo ""

read -p "Ready to launch ComboCounter? Press Enter..."

echo ""
echo "🚀 Launching ComboCounter Enhanced..."
echo "Use the guide above to set up your custom labels!"
echo ""

# Launch the application
./combocounter_enhanced

echo ""
echo "🎉 Demo Complete!"
echo ""
echo "💾 Your custom labels are automatically saved!"
echo ""
echo "🎵 Next Steps for Phonk Production:"
echo "1. Set up your perfect counting phrases"
echo "2. Use Music mode for beat-synced TTS"
echo "3. Record the TTS output for sampling"
echo "4. Layer with your beats and 808s"
echo ""
echo "✏️ Pro Tips:"
echo "• Keep phrases short (1-2 words) for clean samples"
echo "• Use aggressive, punchy words"
echo "• Test different voices with espeak-ng -v"
echo "• Combine with combo mode for milestone phrases"
echo ""
echo "🏁 Custom Labels Demo Complete! 🏁"
