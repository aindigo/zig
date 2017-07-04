

pub const SYS_exit = 1;
pub const SYS_read = 3;
pub const SYS_write = 4;
pub const SYS_open = 5;
pub const SYS_close =6;
pub const SYS_kill = 25;
pub const SYS_getpid = 20;
pub const SYS_fstat = 62;
pub const SYS_lseek = 19;

pub inline fn syscall0(number: usize) -> usize {
    asm volatile ("syscall"
        : [ret] "={rax}" (-> usize)
        : [number] "{rax}" (number)
        : "rcx", "r11")
}

pub inline fn syscall1(number: usize, arg1: usize) -> usize {
    asm volatile ("syscall"
        : [ret] "={rax}" (-> usize)
        : [number] "{rax}" (number),
            [arg1] "{rdi}" (arg1)
        : "rcx", "r11")
}

pub inline fn syscall2(number: usize, arg1: usize, arg2: usize) -> usize {
    asm volatile ("syscall"
        : [ret] "={rax}" (-> usize)
        : [number] "{rax}" (number),
            [arg1] "{rdi}" (arg1),
            [arg2] "{rsi}" (arg2)
        : "rcx", "r11")
}

pub inline fn syscall3(number: usize, arg1: usize, arg2: usize, arg3: usize) -> usize {
    asm volatile ("syscall"
        : [ret] "={rax}" (-> usize)
        : [number] "{rax}" (number),
            [arg1] "{rdi}" (arg1),
            [arg2] "{rsi}" (arg2),
            [arg3] "{rdx}" (arg3)
        : "rcx", "r11")
}




pub const stat = extern struct {
    dev: u32,
    mode: u16,
    nlink: u16,
    ino: u64,
    uid: u32,
    gid: u32,
    rdev: u64,

    atim: timespec,
    mtim: timespec,
    ctim: timespec,

    size: u64,
    blocks: u64,
    blksize: u32,
    flags: u32,
    gen: u32,
    lspare: i32,
    qspare: [2]u64,

};

pub const timespec = extern struct {
    tv_sec: isize,
    tv_nsec: isize,
};
