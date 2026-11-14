/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2026 Bardia Moshiri <bardia@furilabs.com>
 * Copyright (C) 2026 Herman van Hazendonk <github.com@herrie.org>
 */

#ifndef BATMAN_H
#define BATMAN_H

#include <glib.h>
#include "config.h"
#include "cpu.h"
#include "gpu.h"
#include "bluetooth.h"
#include "mtk.h"
#include "device_node.h"
#include "binder.h"
#include "nice.h"

/**
 * libbatman - platform-agnostic power tuning engine
 *
 * BatmanContext owns and drives the hardware subsystems (CPU governors,
 * GPU OPP, devfreq, Bluetooth, WiFi, binder power hints, process niceness).
 * The consumer calls batman_apply_state() when the display turns on or off.
 * To re-apply after a battery or config change, it clears have_last_state
 * first. When audio playback starts or stops, it updates
 * audio_playing_cached and calls batman_refresh_audio_offline_state().
 */

/**
 * Asks the consumer for a live value while state is applied.
 *
 * @param userdata user pointer passed to batman_context_new()
 * @return TRUE or FALSE
 */
typedef gboolean (*BatmanQueryFunc)(void *userdata);

/**
 * batman context
 */
typedef struct {
    BatmanConfig *cfg;                 /** Not owned */

    CpuContext cpu;
    GpuContext gpu;
    BluetoothContext bt;
    MtkContext mtk;
    DeviceNodeContext devnodes;

    Binder *binder;
    NiceContext *nice;

    gboolean wifi_initialized;
    gboolean last_screen_on;
    gboolean have_last_state;
    gboolean irqbalance_available;
    gboolean audio_playing_cached;

    guint isolate_source_id;
    guint deisolate_source_id;

    BatmanQueryFunc is_on_battery;
    BatmanQueryFunc is_overdrive_active;
    void *userdata;
} BatmanContext;

/**
 * Create a BatmanContext and initialize all hardware subsystems.
 *
 * @param cfg loaded configuration, must outlive the context
 * @param is_on_battery returns whether the device runs on battery, NULL means always
 * @param is_overdrive_active returns whether overdrive is on, NULL means never
 * @param userdata user pointer passed to the query functions
 * @return BatmanContext or NULL if cfg is NULL
 */
BatmanContext *
batman_context_new(BatmanConfig *cfg,
                   BatmanQueryFunc is_on_battery,
                   BatmanQueryFunc is_overdrive_active,
                   void *userdata);

/**
 * Tear down all subsystems and free the context.
 *
 * @param ctx BatmanContext
 */
void
batman_context_free(BatmanContext *ctx);

/**
 * Apply the power state for the given screen state.
 *
 * @param ctx BatmanContext
 * @param screen_on TRUE if the display is on, otherwise FALSE
 */
void
batman_apply_state(BatmanContext *ctx,
                   gboolean screen_on);

/**
 * Bring CPUs online or offline after audio_playing_cached changed while
 * the screen is off.
 *
 * @param ctx BatmanContext
 */
void
batman_refresh_audio_offline_state(BatmanContext *ctx);

/**
 * Start or stop the WiFi subsystem to match the WIFI config flag.
 *
 * @param ctx BatmanContext
 */
void
batman_wifi_config_changed(BatmanContext *ctx);

#endif /* BATMAN_H */
