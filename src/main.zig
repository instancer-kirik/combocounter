const std = @import("std");
const core = @import("app_core.zig");

// Import DVUI directly from source
pub const dvui = @import("../dvui/src/dvui.zig");
pub const backend_kind = @import("build_options").backend;
pub const backend = @import("backend");

// Ensure we're using the SDL backend
comptime {
    std.debug.assert(std.mem.eql(u8, backend_kind, "sdl"));
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    // Init SDL backend
    var be = try backend.init(.{
        .allocator = allocator,
        .size = .{ .w = 1280.0, .h = 720.0 },
        .vsync = true,
        .title = "Combo Counter",
    });
    defer be.deinit();

    // Init DVUI window
    var win = try dvui.Window.init(@src(), allocator, be.backend(), .{});
    defer win.deinit();

    // Initialize combo counter
    var state = core.ComboState{
        .label = [_]u8{ 'M', 'a', 'i', 'n', ' ', 'C', 'o', 'u', 'n', 't', 'e', 'r' } ++ [_]u8{0} ** 52,
    };
    state.init("Main Counter");

    // Main game loop
    var quit = false;
    var last_time = std.time.nanoTimestamp();

    while (!quit) {
        const now = std.time.nanoTimestamp();
        const dt = @as(f32, @floatFromInt(now - last_time)) / 1_000_000_000.0;
        last_time = now;

        // Get current time for the frame
        const nstime = win.beginWait(be.hasEvent());

        // Begin the DVUI frame
        try win.begin(nstime);

        // Process events
        quit = try be.addAllEvents(&win);

        // Clear screen
        backend.clearWindow(0, 0, 0, 255);

        // Update
        state.update(dt);

        // Draw UI
        {
            var box = try dvui.box(@src(), .vertical, .{
                .margin = dvui.Rect.all(20),
                .background = true,
                .corner_radius = dvui.Rect.all(4),
                .color_fill = .{ .color = dvui.Color.init(38, 38, 43, 255) },
            });
            defer box.deinit();

            // Label
            const label_str = state.label[0 .. std.mem.indexOfScalar(u8, &state.label, 0) orelse state.label.len];
            const label_color = if (state.paused)
                dvui.Color.init(128, 128, 128, 255)
            else
                dvui.Color.init(255, 255, 255, 255);

            // Label
            var tl1 = try dvui.textLayout(@src(), .{}, .{
                .color = label_color,
                .margin = dvui.Rect.all(10),
            });
            try tl1.addText(label_str, .{});
            tl1.deinit();

            // Score
            var score_text: [32]u8 = undefined;
            const score_str = try std.fmt.bufPrint(&score_text, "Score: {d}", .{state.score});

            const score_color = if (state.perfect_hits > 0)
                dvui.Color.init(0, 255, 128, 255)
            else if (state.paused)
                dvui.Color.init(128, 128, 128, 255)
            else
                dvui.Color.init(255, 255, 255, 255);

            var tl2 = try dvui.textLayout(@src(), .{}, .{
                .color = score_color,
                .margin = dvui.Rect.all(10),
            });
            try tl2.addText(score_str, .{});
            tl2.deinit();

            // Combo
            var combo_text: [32]u8 = undefined;
            const combo_str = try std.fmt.bufPrint(&combo_text, "Combo: x{d}", .{state.combo});

            var tl3 = try dvui.textLayout(@src(), .{}, .{
                .color = dvui.Color.init(255, 255, 255, 255),
                .margin = dvui.Rect.all(10),
            });
            try tl3.addText(combo_str, .{});
            tl3.deinit();

            // Multiplier
            var mult_text: [32]u8 = undefined;
            const mult_str = try std.fmt.bufPrint(&mult_text, "Multiplier: {d:.1}x", .{state.multiplier});

            var tl4 = try dvui.textLayout(@src(), .{}, .{
                .color = dvui.Color.init(255, 255, 255, 255),
                .margin = dvui.Rect.all(10),
            });
            try tl4.addText(mult_str, .{});
            tl4.deinit();
        }

        // Controls
        var controls = try dvui.box(@src(), .horizontal, .{
            .margin = dvui.Rect.all(10),
            .background = true,
        });
        defer controls.deinit();

        if (try dvui.button(@src(), "Increment (Space)", .{}, .{})) {
            state.increment(10);
        }

        if (try dvui.button(@src(), "Decrement (X)", .{}, .{})) {
            state.decrement(5);
        }

        // End DVUI frame
        const end_micros = try win.end(.{});

        // Cursor management
        be.setCursor(win.cursorRequested());
        be.textInputRect(win.textInputRequested());

        // Render the frame
        be.renderPresent();

        // Wait for next frame
        const wait_event_micros = win.waitTime(end_micros, null);
        be.waitEventTimeout(wait_event_micros);
    }
}
