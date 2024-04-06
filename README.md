Like `axwrapper`, but using `forkpty()` instead of `pipe()` so things like shells work as expected.

Performs `CRNL` to `CR` filtering on output.
