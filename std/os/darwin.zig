
const builtin = @import("builtin");
const arch = switch (builtin.arch) {
    builtin.Arch.x86_64 => @import("darwin_x86_64.zig"),
    else => @compileError("unsupported arch"),
};

const c = @cImport({
    @cInclude("sys/mman.h");
    @cInclude("unistd.h");
    @cInclude("fcntl.h");
    @cInclude("sys/wait.h");
    @cInclude("sys/types.h");
    @cInclude("stdio.h");
    @cInclude("sys/stat.h");
});

const errno = @import("errno.zig");

pub const STDIN_FILENO = 0;
pub const STDOUT_FILENO = 1;
pub const STDERR_FILENO = 2;

pub const O_LARGEFILE = 0x0000;
pub const O_RDONLY = 0x0000;
pub const O_RDWR = c.O_RDWR;
pub const O_WRONLY = c.O_WRONLY;
pub const O_CREAT = c.O_CREAT;
pub const O_CLOEXEC = c.O_CLOEXEC;
pub const O_TRUNC = c.O_TRUNC;


pub const SEEK_SET = 0x0;
pub const SEEK_CUR = 0x1;
pub const SEEK_END = 0x2;

pub const SIGHUP    = 1;
pub const SIGINT    = 2;
pub const SIGQUIT   = 3;
pub const SIGILL    = 4;
pub const SIGTRAP   = 5;
pub const SIGABRT   = 6;
pub const SIGIOT    = SIGABRT;
pub const SIGBUS    = 7;
pub const SIGFPE    = 8;
pub const SIGKILL   = 9;
pub const SIGUSR1   = 10;
pub const SIGSEGV   = 11;
pub const SIGUSR2   = 12;
pub const SIGPIPE   = 13;
pub const SIGALRM   = 14;
pub const SIGTERM   = 15;
pub const SIGSTKFLT = 16;
pub const SIGCHLD   = 17;
pub const SIGCONT   = 18;
pub const SIGSTOP   = 19;
pub const SIGTSTP   = 20;
pub const SIGTTIN   = 21;
pub const SIGTTOU   = 22;
pub const SIGURG    = 23;
pub const SIGXCPU   = 24;
pub const SIGXFSZ   = 25;
pub const SIGVTALRM = 26;
pub const SIGPROF   = 27;
pub const SIGWINCH  = 28;
pub const SIGIO     = 29;
pub const SIGPOLL   = 29;
pub const SIGPWR    = 30;
pub const SIGSYS    = 31;
pub const SIGUNUSED = SIGSYS;

pub const PROT_NONE = c.PROT_NONE;
pub const PROT_WRITE= c.PROT_WRITE;
pub const PROT_READ= c.PROT_READ;

pub const MAP_SHARED        = c.MAP_SHARED;
pub const MAP_NOCACHE       = c.MAP_NOCACHE;
pub const MAP_FIXED         = c.MAP_FIXED;
pub const MAP_FILE          = c.MAP_FILE;
pub const MAP_HASSEMAPHORE  = c.MAP_HASSEMAPHORE;
pub const MAP_PRIVATE       = c.MAP_PRIVATE;
pub const MAP_ANONYMOUS     = c.MAP_ANONYMOUS;
pub const MAP_NORESERVE     = c.MAP_NORESERVE;
pub const MAP_FAILED        = @maxValue(usize);

/// copied from linux.zig , may be this is wrong for darwin

fn unsigned(s: i32) -> u32 { *@ptrCast(&u32, &s) }
fn signed(s: u32) -> i32 { *@ptrCast(&i32, &s) }
pub fn WEXITSTATUS(s: i32) -> i32 { signed((unsigned(s) & 0xff00) >> 8) }
pub fn WTERMSIG(s: i32) -> i32 { signed(unsigned(s) & 0x7f) }
pub fn WSTOPSIG(s: i32) -> i32 { WEXITSTATUS(s) }
pub fn WIFEXITED(s: i32) -> bool { WTERMSIG(s) == 0 }
pub fn WIFSTOPPED(s: i32) -> bool { (u16)(((unsigned(s)&0xffff)*%0x10001)>>8) > 0x7f00 }
pub fn WIFSIGNALED(s: i32) -> bool { (unsigned(s)&0xffff)-%1 < 0xff }


pub fn exit(status: usize) -> noreturn {
    _ = arch.syscall1(arch.SYS_exit, status);
    unreachable
}

/// Get the errno from a syscall return value, or 0 for no error.
pub fn getErrno(r: usize) -> usize {
    const signed_r = *@ptrCast(&const isize, &r);
    if (signed_r > -4096 and signed_r < 0) usize(-signed_r) else 0
}

pub fn write(fd: i32, buf: &const u8, count: usize) -> usize {
    arch.syscall3(arch.SYS_write, usize(fd), usize(buf), count)
}

pub fn close(fd: i32) -> usize {
    arch.syscall1(arch.SYS_close, usize(fd))
}

pub fn open(path: &const u8, flags: usize, perm: usize) -> usize {
    arch.syscall3(arch.SYS_open, usize(path), flags, perm)
}

pub fn read(fd: i32, buf: &u8, count: usize) -> usize {
    arch.syscall3(arch.SYS_read, usize(fd), usize(buf), count)
}

pub fn lseek(fd: i32, offset: usize, ref_pos: usize) -> usize {
    arch.syscall3(arch.SYS_lseek, usize(fd), offset, ref_pos)
}

pub const stat = arch.stat;
pub const timespec = arch.timespec;

pub fn fstat(fd: i32, stat_buf: &stat) -> usize {
    arch.syscall2(arch.SYS_fstat, usize(fd), usize(stat_buf))
}

error Unexpected;

pub fn getrandom(buf: &u8, count: usize) -> usize {
   // const rr = open_c(c"/dev/urandom", O_LARGEFILE | O_RDONLY, 0);

   // if(getErrno(rr) > 0) return rr;

   // var fd: i32 = i32(rr);
   // const readRes = read(fd, buf, count);
   usize(42)
}

pub fn raise(sig: i32) -> i32 {
    // TODO investigate whether we need to block signals before calling kill
    // like we do in the linux version of raise

    //var set: sigset_t = undefined;
    //blockAppSignals(&set);
    const pid = i32(arch.syscall0(arch.SYS_getpid));
    const ret = i32(arch.syscall2(arch.SYS_kill, usize(pid), usize(sig)));
    //restoreSignals(&set);
    return ret;
}

pub fn isatty(fd: i32) -> bool {
    return true;
}

/// From now on std for OSX will call the standard libc

pub  fn mmap(address: ?&u8, length: usize, prot: usize, flags: usize, fd: i32, offset: usize)
    -> usize
{
    //usize(c.mmap(@ptrCast(&c_void,address), length, c_int(prot), c_int(flags), fd, c_longlong(offset)))
    usize(42)
}

pub  fn pipe(fd: &[2]i32) -> usize {
    usize(c.pipe(@ptrCast(&c_int, &fd)))
}

pub  fn fork() -> usize {
    usize(c.fork())    
}

pub  fn dup2(fildes1: i32, fildes2: i32) -> usize {
    usize(c.dup2(fildes1, fildes2))
}

pub  fn execve(path: &const u8, argv: &const ?&u8, envp: &const ?&u8) -> usize {
    usize(c.execve(path, argv, envp))
}
pub fn chdir(path: &const u8) -> usize {
    usize(c.chdir(path))
}

pub fn rename(old: &const u8, new: &const u8) -> usize {
    usize(c.rename(old, new))
}

pub fn munmap(address: &u8, length: usize) -> usize {
    usize(c.munmap(@ptrCast(&c_void, address), length))
}

pub fn waitpid(pid: i32, status: &i32, options: i32) -> usize {
    usize(c.waitpid(pid, @ptrCast(&c_int, status), options))
}

pub fn unlink(path: &const u8) -> usize {
    usize(c.unlink(path))
}

pub fn getcwd(buf: &u8, size: usize) -> usize {
    usize(c.getcwd(buf, size))
}

pub fn symlink(existing: &const u8, new: &const u8) -> usize {
    usize(c.symlink(existing, new))
}

pub fn mkdir(path: &const u8, mode: usize) -> usize {
    usize(c.mkdir(path, c_ushort(mode)))
}

