/* Copyright 2024 burkfers (@burkfers)
 * Copyright 2024 Wimads (@wimads)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "quantum.h"
#include "math.h"

static uint32_t maccel_timer;

/* DEVICE_CPI_PARAM
A device specific parameter required to ensure consistent acceleration behaviour across different devices and user dpi settings.
 * PMW3360: 0.087
 * PMW3389: tbd
 * Cirque: tbd
 * Azoteq: tbd
*///disclaimer: values guesstimated by scientifically questionable emperical testing
#ifndef DEVICE_CPI_PARAM // device specific cpi scaling parameter
#    if POINTING_DEVICE_DRIVER == pmw3360
#        define DEVICE_CPI_PARAM 0.087
#    else
#        warning "Unsupported pointing device driver! Please manually set the scaling parameter DEVICE_CPI_PARAM to achieve a consistent acceleration curve!"
#        define DEVICE_CPI_PARAM 0.087
#    endif
#endif

#ifndef MACCEL_STEEPNESS
#    define MACCEL_STEEPNESS 0.4 // steepness of accel curve
#endif
#ifndef MACCEL_OFFSET
#    define MACCEL_OFFSET 1.1 // start offset of accel curve
#endif
#ifndef MACCEL_LIMIT
#    define MACCEL_LIMIT 4.5 // upper limit of accel curve
#endif
#ifndef MACCEL_CPI_THROTTLE
#    define MACCEL_CPI_THROTTLE 200
#endif

static float maccel_a = MACCEL_STEEPNESS;
static float maccel_b = MACCEL_OFFSET;
static float maccel_c = MACCEL_LIMIT;

float maccel_get_steepness(void) {
    return maccel_a;
}
float maccel_get_offset(void) {
    return maccel_b;
}
float maccel_get_limit(void) {
    return maccel_c;
}
void maccel_set_steepness(float val) {
    maccel_a = val;
}
void maccel_set_offset(float val) {
    maccel_b = val;
}
void maccel_set_limit(float val) {
    maccel_c = val;
}

// Clamp a value to the maximum report size to prevent over- and underflows
static inline mouse_xy_report_t clamp_to_report(float val) {
    if (val < XY_REPORT_MIN) {
        return XY_REPORT_MIN;
    } else if (val > XY_REPORT_MAX) {
        return XY_REPORT_MAX;
    } else {
        return val;
    }
}

uint16_t maccel_get_cpi(void) {
    static uint16_t device_cpi      = 100;
    static uint32_t fetch_cpi_timer = 0;

    if (timer_elapsed32(fetch_cpi_timer) > MACCEL_CPI_THROTTLE) {
#if POINTING_DEVICE_DRIVER == azoteq_iqs5xx
        wait_ms(1);
#endif
        device_cpi      = pointing_device_get_cpi();
        fetch_cpi_timer = timer_read32();
    }
    return device_cpi;
}

report_mouse_t pointing_device_task_maccel(report_mouse_t mouse_report) {
    if (mouse_report.x != 0 || mouse_report.y != 0) {
        // time since last mouse report:
        uint16_t delta_time = timer_elapsed32(maccel_timer);
        maccel_timer        = timer_read32();
        // get device cpi setting, only call when mouse hasn't moved since more than 200ms
        static uint16_t device_cpi = 200;
        if (delta_time > 200) {
            device_cpi = pointing_device_get_cpi();
        }
        // calculate dpi correction factor (for normalizing velocity range across different user dpi settings)
        const uint16_t device_cpi     = maccel_get_cpi();
        const float    dpi_correction = (float)100.0f / (DEVICE_CPI_PARAM * device_cpi);
        // calculate delta velocity: dv = dpi_correction * sqrt(dx^2 + dy^2)/dt
        const float velocity = dpi_correction * (sqrtf(mouse_report.x * mouse_report.x + mouse_report.y * mouse_report.y)) / delta_time;
        // calculate mouse acceleration factor: f(dv) = c - (c - 1) * e^(-(dv - b) * a)
        float maccel_factor = maccel_c - (maccel_c - 1) * expf(-1 * (velocity - maccel_b) * maccel_a);
        if (maccel_factor <= 1) { // cut-off acceleration curve below maccel_factor = 1
            maccel_factor = 1;
        }
        // calculate accelerated delta X and Y values:
        const float x = mouse_report.x * maccel_factor;
        const float y = mouse_report.y * maccel_factor;
        // set (clamped) mouse reports for X and Y
        mouse_report.x = clamp_to_report(x);
        mouse_report.y = clamp_to_report(y);

// console output for debugging (enable/disable in maccel.h)
#ifdef MACCEL_DEBUG
        float accelerated = velocity * maccel_factor; // resulting velocity after acceleration; unneccesary for calculation, but nice for debug console
        printf("DPI = %4i, factor = %4f, velocity = %4f, accelerated = %4f\n", device_cpi, maccel_factor, velocity, accelerated);
#endif // MACCEL_DEBUG
    }
    return mouse_report;
}

#define MACCEL_USE_KEYCODES // REMOVEME
bool process_record_maccel(uint16_t keycode, keyrecord_t *record, uint16_t steepness, uint16_t offset, uint16_t limit) {
#ifdef MACCEL_USE_KEYCODES
    if (record->event.pressed) {
        if (keycode == steepness) {
            maccel_set_steepness(maccel_get_steepness() + 0.1);
            printf("maccel: steepness: %f, offset: %f, limit: %f\n", maccel_a, maccel_b, maccel_c);
            return false;
        }
    }
    return true;
}
#else
    // provide a do-nothing keyrecord function so a user doesn't need to unshim when disabling the keycodes
    return true;
}
#endif
