# xrestrict

A utility to modify the "Coordinate Transformation Matrix" of an XInput2 device.

## Basic Usage

    xrestrict -d $DEVICEID [-c $CRTCINDEX]

* `DEVICEID` is the XInput2 device XID, identical to that reported by `xinput list`
* `CRTCINDEX` is the index in 0..N-1 of the N CRTCs to restrict the pointer device to.
`CRTCINDEX` defaults to 0.
For most multi-monitor setups, each non-mirrored CRTC corresponds to a different monitor.
For most single-monitor setups, there will be only one CRTC which is equal to the size of the virtual screen.

## Results

Following successful invocation, the "Coordinate Transformation Matrix" of the pointer device will be modified.
The pointer device should only be able to move within the given CRTC, or slightly more, as required to preserve the aspect ratio of the pointer device.
To check the current "Coordinate Transformation Matrix" of a device, invoke `xinput list-props $DEVICEID`.

## Dependencies

xrestrict uses Xlib, XInput2 support from Xlib, xcb, and XRandR support from xcb

On Ubuntu 14.04 the above dependencies correspond to the following packages:

    libx11-dev libxi-dev libxcb1-dev libx11-xcb-dev libxcb-randr0-dev

in addition to the basic packages required to build most software, and git to retrieve the source:

    build-essential autoconf automake git-core

From the console, a user may install all of these at once with the command:

    sudo apt-get install build-essential autoconf automake libx11-dev libxi-dev libxcb1-dev libx11-xcb-dev libxcb-randr0-dev git-core

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
