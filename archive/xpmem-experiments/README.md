# xpmem-experiments
An experiment within an experiment. What could the result be?

This has been tested against Nathan Hjelm's implementation of XPMEM:
https://github.com/hjelmn/xpmem

Tip: configure it with `./configure --prefix=/` for best results. You'll also need to start the xpmem module via `sudo modprobe xpmem` then make the xpmem device user-accessible with a bit of help from `sudo chmod 0666 /dev/xpmem`. The device is, by default, only usable by `root` and I have not been able to figure out how to reconfigure its rules file to get it working correctly 🙃
