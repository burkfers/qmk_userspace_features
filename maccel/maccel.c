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

#ifdef MACCEL_USE_KEYCODES
#    ifndef MACCEL_STEEPNESS_STEP
#        define MACCEL_STEEPNESS_STEP 0.01f
#    endif
#    ifndef MACCEL_OFFSET_STEP
#        define MACCEL_OFFSET_STEP 0.1f
#    endif
#    ifndef MACCEL_LIMIT_STEP
#        define MACCEL_LIMIT_STEP 0.1f
#    endif
#endif

typedef struct {
    float a;
    float b;
    float c;
} maccel_config_t;

static maccel_config_t g_maccel_config = {.a = MACCEL_STEEPNESS, .b = MACCEL_OFFSET, .c = MACCEL_LIMIT};

static bool _maccel_enabled = true;

float maccel_get_steepness(void) {
    return g_maccel_config.a;
}
float maccel_get_offset(void) {
    return g_maccel_config.b;
}
float maccel_get_limit(void) {
    return g_maccel_config.c;
}
void maccel_set_steepness(float val) {
    if (val >= 0) { // value less 0 leads to nonsensical results
        g_maccel_config.a = val;
    }
}
void maccel_set_offset(float val) {
    g_maccel_config.b = val;
}
void maccel_set_limit(float val) {
    if (val >= 1) { // limit less than 1 leads to nonsensical results
        g_maccel_config.c = val;
    }
}

void maccel_enabled(bool enable) {
    _maccel_enabled = enable;
#ifdef MACCEL_DEBUG
    printf("maccel: enabled: %b\n", _maccel_enabled);
#endif
}
bool maccel_get_enabled(void) {
    return _maccel_enabled;
}
void maccel_toggle_enabled(void) {
    maccel_enabled(!_maccel_enabled);
}

#define _CONSTRAIN(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define CONSTRAIN_REPORT(val) _CONSTRAIN(val, XY_REPORT_MIN, XY_REPORT_MAX)

report_mouse_t pointing_device_task_maccel(report_mouse_t mouse_report) {
    if (mouse_report.x != 0 || mouse_report.y != 0) {
        if (!_maccel_enabled) { // do nothing if not enabled
            return mouse_report;
        }
        // time since last mouse report:
        uint16_t delta_time = timer_elapsed32(maccel_timer);
        maccel_timer        = timer_read32();
        // get device cpi setting, only call when mouse hasn't moved since more than 200ms
        static uint16_t device_cpi = 300;
        if (delta_time > 200) {
            device_cpi = pointing_device_get_cpi();
        }
        // calculate dpi correction factor (for normalizing velocity range across different user dpi settings)
        const float dpi_correction = (float)100.0f / (DEVICE_CPI_PARAM * device_cpi);
        // calculate delta velocity: dv = dpi_correction * sqrt(dx^2 + dy^2)/dt
        const float velocity = dpi_correction * (sqrtf(mouse_report.x * mouse_report.x + mouse_report.y * mouse_report.y)) / delta_time;
        // calculate mouse acceleration factor: f(dv) = c - (c - 1) * e^(-(dv - b) * a)
        float maccel_factor = g_maccel_config.c - (g_maccel_config.c - 1) * expf(-1 * (velocity - g_maccel_config.b) * g_maccel_config.a);
        if (maccel_factor <= 1) { // cut-off acceleration curve below maccel_factor = 1
            maccel_factor = 1;
        }
        // calculate accelerated delta X and Y values and clamp:
        const mouse_xy_report_t x = CONSTRAIN_REPORT(mouse_report.x * maccel_factor);
        const mouse_xy_report_t y = CONSTRAIN_REPORT(mouse_report.y * maccel_factor);

// console output for debugging (enable/disable in config.h)
#ifdef MACCEL_DEBUG
        float accelerated = velocity * maccel_factor; // resulting velocity after acceleration; unneccesary for calculation, but nice for debug console
        printf("DPI = %4i, factor = %4f, velocity = %4f, accelerated = %4f\n", device_cpi, maccel_factor, velocity, accelerated);
        // printf("x %4i, y %4i --%4f--> x' %4i, y' %4i\n", mouse_report.x, mouse_report.y, maccel_factor, x, y);
#endif // MACCEL_DEBUG

        // report back accelerated values
        mouse_report.x = x;
        mouse_report.y = y;
    }
    return mouse_report;
}

#ifdef MACCEL_USE_KEYCODES
static inline float get_mod_step(float step) {
    const uint8_t mod_mask = get_mods();
    if (mod_mask & MOD_MASK_CTRL) {
        step *= 10; // control increases by factor 10
    }
    if (mod_mask & MOD_MASK_SHIFT) {
        step *= -1; // shift inverts
    }
    return step;
}

bool process_record_maccel(uint16_t keycode, keyrecord_t *record, uint16_t steepness, uint16_t offset, uint16_t limit) {
    if (record->event.pressed) {
        if (keycode == steepness) {
            maccel_set_steepness(maccel_get_steepness() + get_mod_step(MACCEL_STEEPNESS_STEP));
            printf("maccel: steepness: %f, offset: %f, limit: %f\n", g_maccel_config.a, g_maccel_config.b, g_maccel_config.c);
            return false;
        }
        if (keycode == offset) {
            maccel_set_offset(maccel_get_offset() + get_mod_step(MACCEL_OFFSET_STEP));
            printf("maccel: steepness: %f, offset: %f, limit: %f\n", g_maccel_config.a, g_maccel_config.b, g_maccel_config.c);
            return false;
        }
        if (keycode == limit) {
            maccel_set_limit(maccel_get_limit() + get_mod_step(MACCEL_LIMIT_STEP));
            printf("maccel: steepness: %f, offset: %f, limit: %f\n", g_maccel_config.a, g_maccel_config.b, g_maccel_config.c);
            return false;
        }
    }
    return true;
}
#else
bool process_record_maccel(uint16_t keycode, keyrecord_t *record, uint16_t steepness, uint16_t offset, uint16_t limit) {
    // provide a do-nothing keyrecord function so a user doesn't need to unshim when disabling the keycodes
    return true;
}
#endif
