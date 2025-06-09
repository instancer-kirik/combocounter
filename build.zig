const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Create a module for the UI code
    const ui_module = b.createModule(.{
        .root_source_file = b.path("src/ui.zig"),
    });

    // Create the executable
    const exe = b.addExecutable(.{
        .name = "combo-counter",
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Link with libc
    exe.linkLibC();

    // Add our UI module
    exe.root_module.addImport("dvui", ui_module);

    // Link with necessary system libraries
    exe.linkSystemLibrary("SDL2");
    exe.linkSystemLibrary("GL");

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
