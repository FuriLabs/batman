/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2026 Bardia Moshiri <bardia@furilabs.com>
 */

#ifndef PPD_H
#define PPD_H

#include "cpu.h"

typedef struct PpdContext PpdContext;

/**
 * Initialize and export org.freedesktop.UPower.PowerProfiles on the system bus.
 *
 * @param cpu CpuContext
 * @return PpdContext or NULL on failure
 */
PpdContext *
ppd_init(CpuContext *cpu);

/**
 * Unexport the service and free resources.
 *
 * @param ppd PpdContext
 */
void
ppd_cleanup(PpdContext *ppd);

/**
 * Check whether overdrive mode is currently enabled.
 *
 * @param ppd PpdContext
 * @return TRUE if overdrive is enabled, otherwise FALSE
 */
gboolean
ppd_is_overdrive_enabled(const PpdContext *ppd);

/**
 * Set GPU to minimum OPP on screen off and restore it on screen on.
 *
 * @param ppd PpdContext
 * @param screen_off TRUE if screen is off, otherwise FALSE
 */
void
ppd_set_screen_off(PpdContext *ppd, gboolean screen_off);

#endif /* PPD_H */
