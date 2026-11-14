## Battery Manager

Batman is a battery management service which tweaks different nodes and pieces of hardware in real time.

# Building

```
make
```

And to install everything onto the system

```
sudo make install
```

And on Debian based systems to build and package batman

```
dpkg-buildpackage
```

And finally, to enable the service

```
sudo systemctl daemon-reload
sudo systemctl enable --now batman
```

# libbatman

The tuning engine (CPU, GPU, devfreq, Bluetooth, WiFi, binder power hints,
process niceness) is built as `libbatman.so`, so a daemon on another platform
can drive it with its own screen, battery and audio events. It only needs
glib, libgbinder and libnl. The API is in `include/batman.h`, installed as
`<batman/batman.h>` with a `libbatman` pkg-config file.

To build and install only the library, without the FuriOS daemon and its
upower, PulseAudio and wayland dependencies:

```
make lib
sudo make install-lib
```

The library goes to `$(PREFIX)/lib/<triplet>` by default; distributions that
use a plain `/usr/lib` can pass `LIBDIR=/usr/lib`. `CFLAGS`, `CPPFLAGS` and
`LDFLAGS` from the environment or the command line are honoured.

[luna-batman](https://github.com/webOS-ports/luna-batman) is a complete
example: the LuneOS daemon that feeds libbatman the display state, the
charger state and audio playback from luna-service2.

# Configuration

`POWERSAVE`
Powersave allows CPU to go into powersaving mode.


`OFFLINE`
Offlining is the process of CPU cores shutting down.


`GPUSAVE`
GPU save allows GPU to go into powersaving.


`CHARGESAVE`
Chargesave indicates whether or not put device to powersave when it is connected to a power source.


`BUSSAVE`
This option allows devfreq bus nodes to be set to powersave.


`BTSAVE`
This option will allow batman to moderate bluetooth power management and switch states accordingly.


`HYBRIS`
This option will use binder IPC to set various pieces of hardware to powersave mode.


`WIFI`
This option will set the WiFi adapter to powersave mode.
