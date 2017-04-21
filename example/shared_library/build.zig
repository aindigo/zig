const Builder = @import("std").build.Builder;

pub fn build(b: &Builder) {
    const release = b.option(bool, "release", "optimizations on and safety off") ?? false;

    const lib = b.addSharedLibrary("mathtest", "mathtest.zig", b.version(1, 0, 0));
    lib.setRelease(release);

    const exe = b.addCExecutable("test");
    exe.addCompileFlagsForRelease(release);
    exe.addCompileFlags([][]const u8 {
        "-std=c99",
    });
    exe.addSourceFile("test.c");
    exe.linkLibrary(lib);
    exe.addIncludeDir(".");

    b.default_step.dependOn(&exe.step);
}
