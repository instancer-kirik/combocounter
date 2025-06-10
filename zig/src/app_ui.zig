const std = @import("std");
const dvui = @import("dvui");
const core = @import("app_core.zig");
const ray = @import("raylib.zig"); // Keep for compatibility

pub const Color = struct {
    r: u8,
    g: u8,
    b: u8,
    a: u8 = 255,

    pub const white = Color{ .r = 255, .g = 255, .b = 255 };
    pub const black = Color{ .r = 0, .g = 0, .b = 0 };
    pub const gray = Color{ .r = 128, .g = 128, .b = 128 };
    pub const perfect = Color{ .r = 0, .g = 255, .b = 128 };
    pub const good = Color{ .r = 0, .g = 192, .b = 255 };
    pub const miss = Color{ .r = 255, .g = 64, .b = 64 };
};

pub const UIState = struct {
    font_size: f32 = 20.0,
    padding: f32 = 10.0,
    animation_time: f32 = 0.0,

    pub fn init() UIState {
        return .{};
    }
};

pub fn render(state: *core.ComboState) !void {
    const ui = UIState.init();

    // Draw tracker card
    const card_width: f32 = 280;
    const card_height: f32 = 120;
    const x: f32 = 20;
    const y: f32 = 20;

    // Create a rectangular card
    const rect = dvui.Rect{
        .x = x,
        .y = y,
        .w = card_width,
        .h = card_height,
    };

    // Background
    try dvui.box(@src(), rect, .{
        .color = dvui.Color.init(38, 38, 43, 255),
        .corner_radius = 4,
    });

    // Label
    const label_str = state.label[0 .. std.mem.indexOfScalar(u8, &state.label, 0) orelse state.label.len];
    const label_color = if (state.paused)
        dvui.Color.init(128, 128, 128, 255)
    else
        dvui.Color.init(255, 255, 255, 255);

    try dvui.label(@src(), label_str, .{
        .rect = dvui.Rect{
            .x = x + ui.padding,
            .y = y + ui.padding,
            .w = card_width - 20,
            .h = ui.font_size,
        },
        .color = label_color,
        .font_size = ui.font_size,
    });

    // Score
    var score_text: [32]u8 = undefined;
    const score_str = try std.fmt.bufPrint(&score_text, "{d}", .{state.score});

    const score_color = if (state.perfect_hits > 0)
        dvui.Color.init(0, 255, 128, 255)
    else if (state.paused)
        dvui.Color.init(128, 128, 128, 255)
    else
        dvui.Color.init(255, 255, 255, 255);

    try dvui.label(@src(), score_str, .{
        .rect = dvui.Rect{
            .x = x + card_width - 100,
            .y = y + ui.padding,
            .w = 100,
            .h = ui.font_size,
        },
        .color = score_color,
        .font_size = ui.font_size,
    });

    // Combo
    var combo_text: [32]u8 = undefined;
    const combo_str = try std.fmt.bufPrint(&combo_text, "x{d}", .{state.combo});

    try dvui.label(@src(), combo_str, .{
        .rect = dvui.Rect{
            .x = x + ui.padding,
            .y = y + ui.padding * 2 + ui.font_size,
            .w = 100,
            .h = ui.font_size,
        },
        .color = dvui.Color.init(255, 255, 255, 255),
        .font_size = ui.font_size,
    });

    // Multiplier
    var mult_text: [32]u8 = undefined;
    const mult_str = try std.fmt.bufPrint(&mult_text, "{d:.1}x", .{state.multiplier});

    try dvui.label(@src(), mult_str, .{
        .rect = dvui.Rect{
            .x = x + card_width - 100,
            .y = y + ui.padding * 2 + ui.font_size,
            .w = 100,
            .h = ui.font_size,
        },
        .color = dvui.Color.init(255, 255, 255, 255),
        .font_size = ui.font_size,
    });
}
