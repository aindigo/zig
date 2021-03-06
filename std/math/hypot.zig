// Special Cases:
//
// - hypot(+-inf, y)  = +inf
// - hypot(x, +-inf)  = +inf
// - hypot(nan, y)    = nan
// - hypot(x, nan)    = nan

const math = @import("index.zig");
const assert = @import("../debug.zig").assert;

// TODO issue #393
pub const hypot = hypot_workaround;

pub fn hypot_workaround(comptime T: type, x: T, y: T) -> T {
    switch (T) {
        f32 => @inlineCall(hypot32, x, y),
        f64 => @inlineCall(hypot64, x, y),
        else => @compileError("hypot not implemented for " ++ @typeName(T)),
    }
}

fn hypot32(x: f32, y: f32) -> f32 {
    var ux = @bitCast(u32, x);
    var uy = @bitCast(u32, y);

    ux &= @maxValue(u32) >> 1;
    uy &= @maxValue(u32) >> 1;
    if (ux < uy) {
        const tmp = ux;
        ux = uy;
        uy = tmp;
    }

    var xx = @bitCast(f32, ux);
    var yy = @bitCast(f32, uy);
    if (uy == 0xFF << 23) {
        return yy;
    }
    if (ux >= 0xFF << 23 or uy == 0 or ux - uy >= (25 << 23)) {
        return xx + yy;
    }

    var z: f32 = 1.0;
    if (ux >= (0x7F+60) << 23) {
        z = 0x1.0p90;
        xx *= 0x1.0p-90;
        yy *= 0x1.0p-90;
    } else if (uy < (0x7F-60) << 23) {
        z = 0x1.0p-90;
        xx *= 0x1.0p-90;
        yy *= 0x1.0p-90;
    }

    z * math.sqrt(f32(f64(x) * x + f64(y) * y))
}

fn sq(hi: &f64, lo: &f64, x: f64) {
    const split: f64 = 0x1.0p27 + 1.0;
    const xc = x * split;
    const xh = x - xc + xc;
    const xl = x - xh;
    *hi = x * x;
    *lo = xh * xh - *hi + 2 * xh * xl + xl * xl;
}

fn hypot64(x: f64, y: f64) -> f64 {
    var ux = @bitCast(u64, x);
    var uy = @bitCast(u64, y);

    ux &= @maxValue(u64) >> 1;
    uy &= @maxValue(u64) >> 1;
    if (ux < uy) {
        const tmp = ux;
        ux = uy;
        uy = tmp;
    }

    const ex = ux >> 52;
    const ey = uy >> 52;
    var xx = @bitCast(f64, ux);
    var yy = @bitCast(f64, uy);

    // hypot(inf, nan) == inf
    if (ey == 0x7FF) {
        return yy;
    }
    if (ex == 0x7FF or uy == 0) {
        return xx;
    }

    // hypot(x, y) ~= x + y * y / x / 2 with inexact for small y/x
    if (ex - ey > 64) {
        return xx + yy;
    }

    var z: f64 = 1;
    if (ex > 0x3FF + 510) {
        z = 0x1.0p700;
        xx *= 0x1.0p-700;
        yy *= 0x1.0p-700;
    } else if (ey < 0x3FF - 450) {
        z = 0x1.0p-700;
        xx *= 0x1.0p700;
        yy *= 0x1.0p700;
    }

    var hx: f64 = undefined;
    var lx: f64 = undefined;
    var hy: f64 = undefined;
    var ly: f64 = undefined;

    sq(&hx, &lx, x);
    sq(&hy, &ly, y);

    z * math.sqrt(ly + lx + hy + hx)
}

test "math.hypot" {
    assert(hypot(f32, 0.0, -1.2) == hypot32(0.0, -1.2));
    assert(hypot(f64, 0.0, -1.2) == hypot64(0.0, -1.2));
}

test "math.hypot32" {
    const epsilon = 0.000001;

    assert(math.approxEq(f32, hypot32(0.0, -1.2), 1.2, epsilon));
    assert(math.approxEq(f32, hypot32(0.2, -0.34), 0.394462, epsilon));
    assert(math.approxEq(f32, hypot32(0.8923, 2.636890), 2.783772, epsilon));
    assert(math.approxEq(f32, hypot32(1.5, 5.25), 5.460083, epsilon));
    assert(math.approxEq(f32, hypot32(37.45, 159.835), 164.163742, epsilon));
    assert(math.approxEq(f32, hypot32(89.123, 382.028905), 392.286865, epsilon));
    assert(math.approxEq(f32, hypot32(123123.234375, 529428.707813), 543556.875, epsilon));
}

test "math.hypot64" {
    const epsilon = 0.000001;

    assert(math.approxEq(f64, hypot64(0.0, -1.2), 1.2, epsilon));
    assert(math.approxEq(f64, hypot64(0.2, -0.34), 0.394462, epsilon));
    assert(math.approxEq(f64, hypot64(0.8923, 2.636890), 2.783772, epsilon));
    assert(math.approxEq(f64, hypot64(1.5, 5.25), 5.460082, epsilon));
    assert(math.approxEq(f64, hypot64(37.45, 159.835), 164.163728, epsilon));
    assert(math.approxEq(f64, hypot64(89.123, 382.028905), 392.286876, epsilon));
    assert(math.approxEq(f64, hypot64(123123.234375, 529428.707813), 543556.885247, epsilon));
}

test "math.hypot32.special" {
    assert(math.isPositiveInf(hypot32(math.inf(f32), 0.0)));
    assert(math.isPositiveInf(hypot32(-math.inf(f32), 0.0)));
    assert(math.isPositiveInf(hypot32(0.0, math.inf(f32))));
    assert(math.isPositiveInf(hypot32(0.0, -math.inf(f32))));
    assert(math.isNan(hypot32(math.nan(f32), 0.0)));
    assert(math.isNan(hypot32(0.0, math.nan(f32))));
}

test "math.hypot64.special" {
    assert(math.isPositiveInf(hypot64(math.inf(f64), 0.0)));
    assert(math.isPositiveInf(hypot64(-math.inf(f64), 0.0)));
    assert(math.isPositiveInf(hypot64(0.0, math.inf(f64))));
    assert(math.isPositiveInf(hypot64(0.0, -math.inf(f64))));
    assert(math.isNan(hypot64(math.nan(f64), 0.0)));
    assert(math.isNan(hypot64(0.0, math.nan(f64))));
}
