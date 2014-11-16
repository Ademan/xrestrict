# xrestrict

A utility to modify the "Coordinate Transformation Matrix" of an XInput2 device.

## Interactive Usage

Interactive mode is intended to be more user friendly than the "Basic Usage" explained below.

    xrestrict -i [options]

1. Ensure your input device is plugged in.
2. Choose one of your computer monitors you wish to restrict your device to.
3. Invoke `xrestrict -i`
3. Use your device to click somewhere on the monitor you chose.

`xrestrict` will then modify your "Coordinate Transformation Matrix" to restrict your chosen device to your chosen monitor.

> NOTE: Because you have restricted your device to one screen, if you wish to change this later, you will not be able to use interactive mode to select a different screen! To work around this, issue `xrestrict --full -i` to interactively select your device, but "confine" it to your entire virtual monitor. This will allow you to click on all of your screens using your device again. Once you have done this you can use `xrestrict -i` again to select the new monitor. I have a plan to address this, but I am also open to suggestions for improving this issue.

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

On Ubuntu 14.04 the above dependencies correspond to the following packages:

    libx11-dev libxi-dev libxrandr-dev

in addition to the basic packages required to build most software, and git to retrieve the source:

    build-essential autoconf automake git-core

From the console, a user may install all of these at once with the command:

    sudo apt-get install build-essential autoconf automake libx11-dev libxi-dev libxrandr-dev

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
