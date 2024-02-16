/* Copyright 2024 burkfers (@burkfers)
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
#include "maccel.h"
#include "via.h"

_Static_assert(sizeof(maccel_config_t) == EECONFIG_USER_DATA_SIZE, "Mismatch in keyboard EECONFIG stored data");

enum via_maccel_channel {
    // clang-format off
    id_maccel = 24
    // clang-format on
};
enum via_maccel_ids {
    // clang-format off
    id_maccel_a       = 1,
    id_maccel_b       = 2,
    id_maccel_c       = 3,
    id_maccel_enabled = 4
    // clang-format on
};

#define COMBINE_UINT8(one, two) (two | (one << 8))

// Handle the data received by the keyboard from the VIA menus
void maccel_config_set_value(uint8_t *data) {
    // data = [ value_id, value_data ]
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);

    switch (*value_id) {
        case id_maccel_a: {
            uint16_t a = COMBINE_UINT8(value_data[0], value_data[1]);

            // calc uint16 to float: steepness only moves the comma
            g_maccel_config.a = a / 10000.0f;
#ifdef MACCEL_DEBUG
            printf("maccel:via: setting a: %u %u -> %u -> %f\n", value_data[0], value_data[1], a, g_maccel_config.a);
#endif
            break;
        }
        case id_maccel_b: {
            uint16_t b = COMBINE_UINT8(value_data[0], value_data[1]);

            // calc uint16 to float: offset moves comma and shifts by 3, so that -3..3 fits into 0..60k
            g_maccel_config.b = (b / 10000.0f) - 3;
            break;
        }
        case id_maccel_c: {
            uint16_t c = COMBINE_UINT8(value_data[0], value_data[1]);

            // calc uint16 to float: offset moves comma, divides by 2 and shifts by 1, so that 1..14 fits into 0..60k
            g_maccel_config.c = (c / 5000.0f) + 1;
            break;
        }
        case id_maccel_enabled: {
            g_maccel_config.enabled = value_data[0];
            break;
        }
    }
}

// Handle the data sent by the keyboard to the VIA menus
void maccel_config_get_value(uint8_t *data) {
    // data = [ value_id, value_data ]
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);

    switch (*value_id) {
        case id_maccel_a: {
            uint16_t a    = g_maccel_config.a * 10000;
            value_data[0] = a >> 8;
            value_data[1] = a & 0xFF;
#ifdef MACCEL_DEBUG
            printf("maccel:via: getting a: %f -> %u -> %u %u\n", g_maccel_config.a, a, value_data[0], value_data[1]);
#endif
            break;
        }
        case id_maccel_b: {
            uint16_t b    = (g_maccel_config.b + 3) * 10000;
            value_data[0] = b >> 8;
            value_data[1] = b & 0xFF;
            break;
        }
        case id_maccel_c: {
            uint16_t c    = (g_maccel_config.c - 1) * 5000;
            value_data[0] = c >> 8;
            value_data[1] = c & 0xFF;
            break;
        }
        case id_maccel_enabled: {
            value_data[0] = g_maccel_config.enabled;
            break;
        }
    }
}

// Save the data to persistent memory after changes are made
void maccel_config_save(void) {
    eeconfig_update_user_datablock(&g_maccel_config);
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    uint8_t *command_id     = &(data[0]);
    uint8_t *channel_id     = &(data[1]);
    uint8_t *value_and_data = &(data[2]);

    if (*channel_id == id_maccel) {
        switch (*command_id) {
            case id_custom_set_value:
                maccel_config_set_value(value_and_data);
                break;
            case id_custom_get_value:
                maccel_config_get_value(value_and_data);
                break;
            case id_custom_save:
                maccel_config_save();
                break;
            default:
                *command_id = id_unhandled;
                break;
        }
        return;
    }

    *command_id = id_unhandled;
}

void eeconfig_init_user(void) {
    // Write default value to EEPROM now
    eeconfig_update_user_datablock(&g_maccel_config);
}

// On Keyboard startup
void keyboard_post_init_maccel(void) {
    // Read custom menu variables from memory
    eeconfig_read_user_datablock(&g_maccel_config);
}
