Lowrider
========

Lowrider is a low-latency audio loopback tool, similar to alsaloop. However unlike alsaloop, it uses a timer-based approach instead of the traditional period-based approach, which results in lower latency especially with asynchronous USB audio devices. However because of the rather unconventional approach, it does not work well with buggy hardware/drivers that can't reliably report the current ring buffer position. Lowrider does not require the input and output device to be synchronized, in fact it works about equally well and with similar latency in either case. It uses a custom low-latency variable-rate resampler to resample the incoming audio stream in order to keep it in sync with the audio clock of the output device with minimal latency.

The simplest way to run lowrider is as follows:

	lowrider --device-in=hw:1 --device-out=hw:2

This will work fine as long as nothing else is using the CPU, but as soon as you run anything CPU-heavy, you will probably get underruns. Lowrider is a lot more reliable when launched with realtime priority as follows:

	chrt 60 lowrider --device-in=hw:1 --device-out=hw:2

This requires realtime privileges and works best with a realtime kernel or at least a preemptive kernel

Options
--------

You can see the full list of command-line options by running `lowrider --help`. The main ones you may want to change are:

| Option                      | Description                                               |
| --------------------------- | --------------------------------------------------------- |
| --device-in=NAME            | Set the input device (e.g. `hw:1`).                       |
| --device-out=NAME           | Set the output device (e.g. `hw:2`).                      |
| --channels-in=NUM           | Set the number of input channels (default 2).             |
| --channels-out=NUM          | Set the number of output channels (default 2).            |
| --rate-in=RATE              | Set the input sample rate (default 48000 Hz).             |
| --rate-out=RATE             | Set the output sample rate (default 48000 Hz).            |
| --target-level=LEVEL        | Set the targeted buffer fill level (default 128).         |
| --timer-period=NANOSECONDS  | Set the timer period (default 620000 ns).                 |
| --loop-bandwidth=FREQUENCY  | Set the bandwidth of the feedback loop (default 0.1 Hz).  |

Lowrider also provides options to change the period and buffer size, but *this will not change the latency*. The latency is determined by the `--target-level` option and the resampler parameters. If you encounter underruns (which usually sounds like clicks or crackling), try increasing the `--target-level` option until it disappears.

The `--timer-period` option can be decreased to reduce the latency, but since USB devices are limited to one transfer every millisecond, there is little gain in making it significantly smaller. The default value of 620 µs was chosen specifically because it doesn't align with common refresh rates, which results in more accurate averaging of buffer fill levels. Higher `--timer-period` values can be used to reduce CPU usage if low latency is less important.

The `--loop-bandwidth` option controls how aggressively lowrider will resample incoming audio in order to keep it in sync with the audio clock of the output device. Higher values increase the aggressiveness of the feedback loop, which results in better tracking (i.e. lower risk of underruns and more consistent latency) but more jitter in the final audio. Lower values provide better jitter filtering but worse tracking, and also increase the 'faststart' time (i.e. how long it takes for the feedback loop to stabilize after startup). The default value of 0.1 Hz is fine in most cases.

License
-------

GNU GPL v3 - read 'COPYING' for more info.

Compiling
---------

Lowrider uses CMake for compilation. In order to compile the 'release' version (with optimizations enabled), run:

	mkdir -p build-release
	cd build-release
	cmake -DCMAKE_BUILD_TYPE=Release "$@" ..
	make

The `lowrider` program can then be found in `build-release/src/lowrider`.

Build dependencies
------------------

You will need the following packages to compile lowrider:

- GCC (>= 4.8)
- cmake
- pkg-config
- ALSA library
