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
#include "maccel.h"

static uint32_t maccel_timer;


#ifndef MACCEL_STEEPNESS
#    define MACCEL_STEEPNESS 1.0 // steepness of accel sigmoid-curve
#endif
#ifndef MACCEL_OFFSET
#    define MACCEL_OFFSET 5.0 // midpoint velocity of accel sigmoid-curve
#endif
#ifndef MACCEL_LIMIT
#    define MACCEL_LIMIT 6.0 // upper limit of accel sigmoid-curve
#endif


maccel_config_t g_maccel_config = {
    // clang-format off
    .a = MACCEL_STEEPNESS,
    .b = MACCEL_OFFSET,
    .c = MACCEL_LIMIT,
    .enabled = true
    // clang-format on,e
};

/* DEVICE_CPI_PARAM
A device specific parameter required to ensure consistent acceleration behaviour across different devices and user dpi settings.
 * PMW3360: 0.087
 * PMW3389: tbd
 * Cirque: tbd
 * Azoteq: tbd
*///disclaimer: values guesstimated by scientifically questionable emperical testing
// Slightly hacky method of detecting which driver is loaded
#if !defined(DEVICE_CPI_PARAM)
// #    if defined(PMW33XX_FIRMWARE_LENGTH)
#    if defined(POINTING_DEVICE_DRIVER_pmw3360)
#        define DEVICE_CPI_PARAM 0.087
#    elif defined(POINTING_DEVICE_DRIVER_cirque_pinnacle_spi)
#        define DEVICE_CPI_PARAM 0.087
#    else
#        warning "Unsupported pointing device driver! Please manually set the scaling parameter DEVICE_CPI_PARAM to achieve a consistent acceleration curve!"
#        define DEVICE_CPI_PARAM 0.087
#    endif
#endif


#ifdef MACCEL_USE_KEYCODES
#    ifndef MACCEL_STEEPNESS_STEP
#        define MACCEL_STEEPNESS_STEP 0.1f
#    endif
#    ifndef MACCEL_OFFSET_STEP
#        define MACCEL_OFFSET_STEP 0.5f
#    endif
#    ifndef MACCEL_LIMIT_STEP
#        define MACCEL_LIMIT_STEP 1.0f
#    endif
#endif

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
    g_maccel_config.enabled = enable;
#ifdef MACCEL_DEBUG
    printf("maccel: enabled: %d\n", g_maccel_config.enabled);
#endif
}
bool maccel_get_enabled(void) {
    return g_maccel_config.enabled;
}
void maccel_toggle_enabled(void) {
    maccel_enabled(!maccel_get_enabled());
}

#define _CONSTRAIN(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define CONSTRAIN_REPORT(val) _CONSTRAIN(val, XY_REPORT_MIN, XY_REPORT_MAX)

report_mouse_t pointing_device_task_maccel(report_mouse_t mouse_report) {
    if (mouse_report.x != 0 || mouse_report.y != 0) {
        if (!g_maccel_config.enabled) { // do nothing if not enabled
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
        // calculate mouse acceleration factor: f(dv) = 1 + (c - 1) / (1 + e^(-(dv - b) * a))
        const float steepness = g_maccel_config.a;
        const float velocity_midpoint = g_maccel_config.b;
        const float factor_max = g_maccel_config.c;
        const float factor_min = 1;  //  Maybe turn it into proper param?
        const float e = expf( - steepness * (velocity - velocity_midpoint));
        const float maccel_factor = factor_min + (factor_max - factor_min) / (1 + e);
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

// provide weak do-nothing shims so users do not need to unshim when diabling via
__attribute__((weak)) void keyboard_post_init_maccel(void) {
    return;
}
