const std = @import("std");

const cpp_flags = [_][]const u8{
    "-std=c++23",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wconversion",
    "-Wshadow",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const library = b.addLibrary(.{
        .name = "shitnet",
        .linkage = .static,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });
    configureCpp(b, library.root_module);
    b.installArtifact(library);
    library.installHeader(b.path("include/shitnet/shitnet.h"), "shitnet/shitnet.h");

    const tests = b.addExecutable(.{
        .name = "shitnet-tests",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });
    configureCpp(b, tests.root_module);
    tests.root_module.addCSourceFile(.{
        .file = b.path("tests/ethernet.cpp"),
        .flags = &cpp_flags,
    });

    const run_tests = b.addRunArtifact(tests);
    b.step("test", "Run shitnet tests").dependOn(&run_tests.step);
    b.step("run", "Run the scaffold verification").dependOn(&run_tests.step);
}

fn configureCpp(b: *std.Build, module: *std.Build.Module) void {
    module.link_libc = true;
    module.link_libcpp = true;
    module.addIncludePath(b.path("include"));
    module.addCSourceFile(.{
        .file = b.path("src/shitnet.cpp"),
        .flags = &cpp_flags,
    });
}
