DisplayMode
===========

DisplayMode is a command line tool for quickly showing and changing the current
screen's resolution and refresh rate on X11, like `xrandr(1)`. It has been
designed for simplicity, and to accept non-integral refresh rates.

So here are the main differences to `xrandr(1)`:

 * DisplayMode accepts fractional refresh rates and finds the closest mode
   for it. For example: `displaymode 29.97` will switch the refresh rate to
   29.97 Hz.
   
 * It's about resolution and refresh rate, none of the many additional
   features of `xrandr(1)` exist for DisplayMode. So it's much simpler.

Using DisplayMode
-----------------

To show the current resolution and refreshrate, run `displaymode` without any
parameters:

    $ displaymode
    Refresh rates for HDMI-A-1:
      3840x2160@...    60      **60**      59.94   50      30      _29.97_   25      24      23.98

Refresh rates in **bold** are your display's default modes. The rate in
_italics_ will actually be shown as reverse text in reality, and it indicates
the currently active refresh rate.

To switch to a different refresh rate, is simple:

    $ displaymode 23.9
    
You might have noticed that the call says 23.9 there, but displaymode reported
the rate as 23.98. That's fine, it'll switch to the closest available refresh
rate.

To also change the display resolution, the call might be:

    $ displaymode 1920x1080@60

The list of outputs (i.e. screens) is available with `displaymode --list-outputs`.
If you have more than one active screen, displaymode works on the primary one by
default; to have it work wit another screen use the `-o` parameter;

    $ displaymode -o DisplayPort-0 60

That would switch the screen connected to the first DisplayPort interface to a
refresh rate of 60 Hz.

To see all available resolutions and refresh rates, use
`displaymode --list-modes`.
    
Building DisplayMode
--------------------

DisplayMode consists of a single `.cpp` file, and it depends on Xlib and
libxrandr. If you use Debian/Ubuntu or something similar, that means you
need the `libx11-dev` and `libxrandr-dev` packages to be installed.

To build it, you may use *cmake*, or, because it's just a single file,
just copy and paste this compiler call:

    c++ -O2 displaymode.cpp -o displaymode -lX11 -lXrandr
  
Using cmake would look something like this:

    $ cmake -DCMAKE_BUILD_TYPE=Release .
    $ cmake --build .

License
-------

DisplayMode is licensed under a MIT license. See LICENSE.txt for details.
