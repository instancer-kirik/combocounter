const std = @import("std");

pub const ComboState = struct {
    label: [64]u8,
    score: i32 = 0,
    combo: i32 = 0,
    max_combo: i32 = 0,
    paused: bool = false,
    multiplier: f32 = 1.0,
    decay_pause: f32 = 0.0,
    total_hits: u32 = 0,
    perfect_hits: u32 = 0,
    miss_hits: u32 = 0,

    pub fn init(self: *ComboState, label: []const u8) void {
        @memcpy(self.label[0..@min(label.len, self.label.len)], label);
        if (label.len < self.label.len) {
            self.label[label.len] = 0;
        }
        self.reset();
    }

    pub fn reset(self: *ComboState) void {
        self.score = 0;
        self.combo = 0;
        self.max_combo = 0;
        self.paused = false;
        self.multiplier = 1.0;
        self.decay_pause = 0.0;
        self.total_hits = 0;
        self.perfect_hits = 0;
        self.miss_hits = 0;
    }

    pub fn increment(self: *ComboState, amount: u32) void {
        if (self.paused) return;

        self.total_hits += 1;
        self.combo += 1;
        if (self.combo > self.max_combo) {
            self.max_combo = self.combo;
        }

        const score_increment = @as(i32, @intFromFloat(@as(f32, @floatFromInt(amount)) * self.multiplier));
        self.score += score_increment;

        // Increase multiplier
        self.multiplier += 0.1;
        if (self.multiplier > 2.0) {
            self.multiplier = 2.0;
        }
    }

    pub fn decrement(self: *ComboState, amount: u32) void {
        if (self.paused) return;

        self.miss_hits += 1;
        self.combo = 0;
        self.multiplier = 1.0;

        const safe_amount = if (amount > std.math.maxInt(i32))
            std.math.maxInt(i32)
        else
            @as(i32, @intCast(amount));

        if (self.score >= safe_amount) {
            self.score -= safe_amount;
        } else {
            self.score = 0;
        }
    }

    pub fn update(self: *ComboState, dt: f32) void {
        if (self.paused) return;

        if (self.decay_pause > 0) {
            self.decay_pause -= dt;
            return;
        }

        // Decay multiplier over time
        if (self.multiplier > 1.0) {
            self.multiplier -= dt * 0.1;
            if (self.multiplier < 1.0) {
                self.multiplier = 1.0;
            }
        }
    }
};
