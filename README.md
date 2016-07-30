# xrestrict

A utility to modify the "Coordinate Transformation Matrix" of an XInput2 device.

## Interactive Usage

Interactive mode is intended to be more user friendly than the "Basic Usage" explained below.

    xrestrict -I [options]

1. Ensure your input device is plugged in.
2. Choose one of your computer monitors you wish to restrict your device to.
3. Invoke `xrestrict -I`
4. Use your device to click somewhere on the monitor you chose.

`xrestrict` will then modify your "Coordinate Transformation Matrix" to restrict your chosen device to your chosen monitor.

> NOTE: In step 4 use device you wish to modify to click.

> NOTE: To function properly, `xrestrict -I` temporarily resets some "Coordinate Transformation Matrix"s to the default value. However, if `xrestrict -I` terminates abnormally, it may not restore the correct "Coordinate Transformation Matrix"s. This is not likely to be a problem because most devices already use the default "Coordinate Transformation Matrix" to begin with, so `xrestrict -I` doesn't change it. In addition, `xrestrict -I` only modifies the "Coordinate Transformation Matrix" of devices with "Abs X" and "Abs Y" axes. Mice generally do not posess these and instead have "Rel X" and "Rel Y" axes. Typically, "Abs X" and "Abs Y" are only found on touchscreens and drawing tablets, and it is likely the only device like that is the one you are modifying.

## Basic Usage

    xrestrict -d $DEVICEID [-c $CRTCINDEX] [options]

* `DEVICEID` is the XInput2 device XID, identical to that reported by `xinput list`
* `CRTCINDEX` is the index in 0..N-1 of the N CRTCs to restrict the pointer device to.
`CRTCINDEX` defaults to 0.
For most multi-monitor setups, each non-mirrored CRTC corresponds to a different monitor.
For most single-monitor setups, there will be only one CRTC which is equal to the size of the virtual screen.
* `options` is a set of extra arguments which control things like alignment and fitting, for a complete list and description please look at `xrestrict`'s usage output.

## Results

Following successful invocation, the "Coordinate Transformation Matrix" of the pointer device will be modified.
The pointer device should only be able to move within the given CRTC, or slightly more, as required to preserve the aspect ratio of the pointer device.
To check the current "Coordinate Transformation Matrix" of a device, invoke `xinput list-props $DEVICEID`.

## Dependencies

xrestrict uses Xlib, XInput2 support from Xlib, and XRandR

### Ubuntu
On Ubuntu 14.04 the above dependencies correspond to the following packages:

    libx11-dev libxi-dev libxrandr-dev

in addition to the basic packages required to build most software, and git to retrieve the source:

    build-essential autoconf automake git-core pkg-config

From the console, a user may install all of these at once with the command:

    sudo apt-get install build-essential autoconf automake pkg-config libx11-dev libxi-dev libxrandr-dev

### openSUSE
On openSUSE 13.2 the above dependencies correspond to the following packages:

    git automake autoconf gcc make libx11-devel libXrandr-devel xinput libXi-devel

From the console, a user may install all of these at once with the command:

    sudo zypper install git automake autoconf gcc make libX11-devel libXrandr-devel xinput libXi-devel

## Building

xrestrict uses a standard autotools configuration.
To build xrestrict, first obtain the source code.
If you do not have the xrestrict source code you may obtain it via git

From a console, change directories to where you wish to store the source code, then issue:

    git clone https://github.com/ademan/xrestrict

After this, change into the directory of the source code you just retrieved:

    cd xrestrict

At this point all you need to do is initialize the build system:

    autoreconf -i
    ./configure

You are now ready to build xrestrict, which can be done by invoking Make:

    make

Make will produce plenty of output, but when it's done, assuming there was no error, xrestrict should be built.
You can invoke your newly built xrestrict with `src/xrestrict`.

## License

xrestrict is provided without warrantee under the MIT license. See COPYING.
