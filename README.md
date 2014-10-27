# xrestrict

A utility to modify the "Coordinate Transformation Matrix" of an XInput2 device.

## Basic Usage

    xrestrict -d $DEVICEID -c $CRTCINDEX

DEVICEID is the XInput2 device XID, identical to that reported by `xinput list`

CRTCINDEX is the index in 0..N-1 of the N CRTCs to restrict the pointer device to.
For most multi-monitor setups, each non-mirrored CRTC corresponds to a different monitor.
For most single-monitor setups, there will be only one CRTC which is equal to the size of the virtual screen.

## Results

Following successful invocation, the "Coordinate Transformation Matrix" of the pointer device will be modified.
The pointer device should only be able to move within the given CRTC, or slightly more, as required to preserve the aspect ratio of the pointer device.
To check the current "Coordinate Transformation Matrix" of a device, invoke `xinput list-props $DEVICEID`.

## License

xrestrict is provided without warrantee under the MIT license. See COPYING.
