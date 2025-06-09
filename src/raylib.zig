const std = @import("std");

pub const Color = struct {
    r: u8,
    g: u8,
    b: u8,
    a: u8,
};

pub fn DrawRectangle(posX: i32, posY: i32, width: i32, height: i32, color: Color) void {
    _ = posX;
    _ = posY;
    _ = width;
    _ = height;
    _ = color;
    // This is a stub - actual implementation will be provided by dvui
}

pub fn DrawText(text: [*c]const u8, posX: i32, posY: i32, fontSize: i32, color: Color) void {
    _ = text;
    _ = posX;
    _ = posY;
    _ = fontSize;
    _ = color;
    // This is a stub - actual implementation will be provided by dvui
}
