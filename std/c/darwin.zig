pub extern "c" fn getrandom(buf_ptr: &u8, buf_len: usize) -> c_int;
extern "c" fn __errno_location() -> &c_int;

pub const _errno = __error;
