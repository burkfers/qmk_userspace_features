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
#pragma once

#include "quantum.h"

#define MACCEL_USE_KEYCODES

report_mouse_t pointing_device_task_maccel(report_mouse_t mouse_report);
bool           process_record_maccel(uint16_t keycode, keyrecord_t *record, uint16_t steepness, uint16_t offset, uint16_t limit);

void maccel_enabled(bool enable);

float maccel_get_steepness(void);
float maccel_get_offset(void);
float maccel_get_limit(void);
void  maccel_set_steepness(float val);
void  maccel_set_offset(float val);
void  maccel_set_limit(float val);
