Huddly-updater is a utility to upgrade the huddly camera firmware.


## Requirements
TODO: The GNU C library, libusb 1.0 and libudev are required.

## Building
At the root of the repository,
```
$ make
```
Alternatively at Chromium OS development environment,
```
$ emerge-$${BOARD} huddly-updater
```

## How to use
```
$ huddly-updater --help
```

## TODO
It will also provide assistant tools to query the firmware version
of the firmware installed in the huddly camera, that of a binary blob file.
