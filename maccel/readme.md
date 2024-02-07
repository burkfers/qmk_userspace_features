# Mouse acceleration

This feature was born from the frustration of not having a tweakable acceleration curve that could work between OSes and hosts and be specific to one device, catalysed by multiple users expressing interest.

## Installation

To use this feature, copy `maccel.c` and `maccel.h` into your userspace or keymap.

You have several options of how to build it.

1. Simple include

You may directly `#include "maccel.c` at the top of your `keymap.c`. This is an easy option if you are not maintaining a userspace.

2. Build system

You can add the `.c` to your build process by naming it in `rules.mk`: 
```
SRC += "maccel.c"
```

---

Next, add the acceleration shim to your `pointing_device_task_user`:
```c
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    // ...
    return pointing_device_task_maccel(mouse_report);
}
```
If you have chosen to use the build system, you will also need to `#include "maccel.h`.

You may call it at the beginning if you wish to use the accelerated mouse report for your other code.

If you have not previously implemented a `pointing_device_task_user` (ie. your keymap has no such function), add one to `keymap.c`:
```c
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    return pointing_device_task_maccel(mouse_report);
}
```

## Configuration

Several characteristics of the acceleration curve can be tweaked by adding relevant defines to `config.h`:
```c
#define MACCEL_STEEPNESS 0.6 // steepness of accel curve
#define MACCEL_OFFSET 0.8    // X-offset of accel curve
#define MACCEL_LIMIT 3.5     // maximum scale factor
```

[![](accel_curve.png)](https://www.wolframalpha.com/input?i=plot+c-%28c-1%29%28e%5E%28-%28x-b%29*a%29%29+with+a%3D0.6+with+b%3D0.8+with+c%3D3.5+from+x%3D-0.1+to+10+from+y%3D-0.1+to+4.5)

To aid in dialing in the settings just right, a debug mode exists to print mathy details to the console. Refer to the QMK documentation on how to enable the console and debugging, then enable mouse acceleration debugging in `config.h`:
```c
#define MACCEL_DEBUG
/*
 * Requires enabling float support for printf!
 */
#undef PRINTF_SUPPORT_DECIMAL_SPECIFIERS
#define PRINTF_SUPPORT_DECIMAL_SPECIFIERS 1
```

## Limitations

Since this implementation only scales pointer movement upwards, you may need to lower your DPI to end up with acceptable speeds. Please see the documentation for your device to learn more.

With an unfavorable combination of `POINTING_DEVICE_THROTTLE_MS` and higher DPI, you may also run into issues of peaking the maximum movement. Enable extended mouse reports by adding the following define in `config.h` to make this much less likely:
```c
#define MOUSE_EXTENDED_REPORT
```

## Release history

2024 February 07 - Experimental new DPI correction to achieve a consistent curve across varying DPIs.
2024 February 06 - First release candidate. Feedback welcome!

## Credits
Thanks to everyone who helped!
Including:
- Wimads (@wimads) and burkfers (@burkfers) wrote most of the code
- Quentin (@balanstik) for insightful commentary on the math, and testing
- ouglop (@ouglop) for insightful commentary on the math
- Drashna Jael're (@drashna) for coding tips and their invaluable bag of magic C tricks
