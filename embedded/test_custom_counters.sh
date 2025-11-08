#!/bin/bash

# Simple Custom Counters Test
echo "🎯 Custom Counters Feature Test"
echo "==============================="
echo ""

# Check if combocounter_enhanced exists
if [ ! -f "./combocounter_enhanced" ]; then
    echo "❌ combocounter_enhanced not found!"
    echo "Run: make -f Makefile.enhanced"
    exit 1
fi

echo "✅ Testing Custom Counter Creation!"
echo ""
echo "This will demonstrate:"
echo "• Creating custom counters with your own names"
echo "• Tracking arbitrary things (Push-ups, Water, etc.)"
echo "• Quick incrementing and switching between counters"
echo ""

echo "📋 Manual Test Steps:"
echo "1. Launch: ./combocounter_enhanced"
echo "2. Press 'H' (Settings)"
echo "3. Press 'S' to move to 'Custom Counters'"
echo "4. Press ENTER to access Custom Counters"
echo "5. Press ENTER on '+ Add New Counter'"
echo "6. Type your counter name (e.g., 'Push-ups', 'Water', 'Songs')"
echo "7. Press ENTER to save"
echo "8. Press 'Q' to go back and test with SPACE"
echo ""

echo "🔥 Example Custom Counters:"
echo "• Push-ups"
echo "• Pull-ups"
echo "• Water bottles"
echo "• Songs listened"
echo "• Deep breaths"
echo "• Coffee cups"
echo "• Pages read"
echo "• Miles run"
echo ""

read -p "Press Enter to launch ComboCounter and test custom counters..."

echo ""
echo "🚀 Launching ComboCounter Enhanced..."
echo "Use the steps above to create your custom counters!"
echo ""

./combocounter_enhanced

echo ""
echo "🎉 Custom Counters Test Complete!"
echo ""
echo "💡 What you can now do:"
echo "• Track ANY arbitrary thing you want"
echo "• Create up to 8 different custom counters"
echo "• Quick increment with SPACE"
echo "• Switch between counters with W/S"
echo "• Edit or delete counters anytime"
echo ""
echo "🏁 Perfect for tracking your daily activities! 🏁"
