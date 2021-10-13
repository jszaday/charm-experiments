# XPMEM Pingpong
A ping-pong-like microbenchmark for comparing Converse and XPMEM. Assumes non-SMP builds of Converse/Charm++, generally expected to be run with two PEs. Usage is:
`./tester <numIters>? <size>?`

This has been tested against Nathan Hjelm's implementation of XPMEM:
https://github.com/hjelmn/xpmem

Tip: configure it with `./configure --prefix=/` for best results. You'll also need to start the xpmem module via `sudo modprobe xpmem` then make the xpmem device user-accessible with a bit of help from `sudo chmod 0666 /dev/xpmem`. The device is, by default, only usable by `root` and I have not been able to figure out how to reconfigure its rules file to get it working correctly 🙃

Try running with an error-checking enabled version of Converse if you run into problems; the code has various assertions throughout it.
