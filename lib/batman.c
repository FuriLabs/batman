/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2026 Bardia Moshiri <bardia@furilabs.com>
 * Copyright (C) 2026 Herman van Hazendonk <github.com@herrie.org>
 */

#include "batman.h"
#include "cpu.h"
#include "gpu.h"
#include "bluetooth.h"
#include "device_node.h"
#include "mtk.h"
#include "wifi.h"
#include "binder.h"
#include "nice.h"
#include "utils.h"

#define BATMAN_STATE_DIR "/var/lib/batman"
#define BATMAN_SCREEN_STATE_PATH BATMAN_STATE_DIR "/screen"

static gboolean
savings_allowed(const BatmanContext *ctx)
{
    if (ctx == NULL)
        return TRUE;

    if (ctx->cfg->chargesave_enabled)
        return TRUE;

    /* chargesave disabled: only do savings when on battery */
    if (ctx->is_on_battery == NULL)
        return TRUE;

    return ctx->is_on_battery(ctx->userdata);
}

static gboolean
overdrive_active(const BatmanContext *ctx)
{
    if (ctx == NULL || ctx->is_overdrive_active == NULL)
        return FALSE;

    return ctx->is_overdrive_active(ctx->userdata);
}

static void
nice_apply(BatmanContext *ctx, gboolean screen_on)
{
    if (!ctx || !ctx->nice)
        return;

    if (nice_is_enabled(ctx->nice) == screen_on)
        return;

    if (!nice_enable(ctx->nice, screen_on))
        g_warning("nice: failed to %s", screen_on ? "enable" : "disable");
}

static void
cancel_isolate(BatmanContext *ctx)
{
    if (!ctx)
        return;

    if (ctx->isolate_source_id != 0) {
        g_source_remove(ctx->isolate_source_id);
        ctx->isolate_source_id = 0;
    }
}

static void
cancel_deisolate(BatmanContext *ctx)
{
    if (!ctx)
        return;

    if (ctx->deisolate_source_id != 0) {
        g_source_remove(ctx->deisolate_source_id);
        ctx->deisolate_source_id = 0;
    }
}

static gboolean
isolate_cb(gpointer userdata)
{
    BatmanContext *ctx = userdata;

    if (!ctx)
        return G_SOURCE_REMOVE;

    ctx->isolate_source_id = 0;

    if (!ctx->cfg->offline_enabled)
        return G_SOURCE_REMOVE;

    if (!ctx->mtk.isolation_available)
        return G_SOURCE_REMOVE;

    mtk_isolate(&ctx->mtk,
                ctx->cpu.first_pol_core,
                ctx->cpu.last_pol_core);

    return G_SOURCE_REMOVE;
}

static gboolean
deisolate_cb(gpointer userdata)
{
    BatmanContext *ctx = userdata;

    if (!ctx)
        return G_SOURCE_REMOVE;

    ctx->deisolate_source_id = 0;

    if (!ctx->cfg->offline_enabled)
        return G_SOURCE_REMOVE;

    if (!ctx->mtk.isolation_available)
        return G_SOURCE_REMOVE;

    mtk_deisolate(&ctx->mtk,
                  ctx->cpu.first_pol_core,
                  ctx->cpu.last_pol_core);

    return G_SOURCE_REMOVE;
}

static void
schedule_isolate(BatmanContext *ctx)
{
    if (!ctx)
        return;

    cancel_deisolate(ctx);

    if (!ctx->cfg->offline_enabled || !ctx->mtk.isolation_available)
        return;

    if (ctx->isolate_source_id != 0)
        return;

    ctx->isolate_source_id = g_timeout_add(1000, isolate_cb, ctx);
}

static void
schedule_deisolate(BatmanContext *ctx)
{
    if (!ctx)
        return;

    cancel_isolate(ctx);

    if (!ctx->cfg->offline_enabled || !ctx->mtk.isolation_available)
        return;

    if (ctx->deisolate_source_id != 0)
        return;

    ctx->deisolate_source_id = g_timeout_add(1000, deisolate_cb, ctx);
}

static void
irqbalance_apply(BatmanContext *ctx, gboolean screen_on)
{
    if (!ctx || !ctx->irqbalance_available)
        return;

    if (screen_on)
        systemd_service_start_async("irqbalance.service");
    else
        systemd_service_stop_async("irqbalance.service");
}

static void
apply_overdrive_runtime(BatmanContext *ctx, gboolean screen_on)
{
    if (!ctx)
        return;

    if (ctx->cfg->offline_enabled) {
        cpu_apply_online(&ctx->cpu);
        schedule_deisolate(ctx);
    }

    cpu_restore_offline_limit(&ctx->cpu, ctx->cfg);

    if (ctx->cfg->powersave_enabled) {
        device_node_apply_default(&ctx->devnodes, ctx->cfg);
        bluetooth_apply_default(&ctx->bt, ctx->cfg);
    } else {
        bluetooth_apply_default(&ctx->bt, ctx->cfg);
    }

    if (ctx->cfg->wifi_enabled && ctx->wifi_initialized) {
        wifi_set_powersave(WIFI_IFACE, FALSE);
        wifi_set_wmtwifi(WIFI_IFACE, WMTWIFI_RESUME_VALUE);
    }

    if (ctx->cfg->binder_enabled && ctx->binder)
        binder_set_performance(ctx->binder);

    irqbalance_apply(ctx, TRUE);

    write_str(BATMAN_SCREEN_STATE_PATH, screen_on ? "yes" : "no");
}

void
batman_refresh_audio_offline_state(BatmanContext *ctx)
{
    if (ctx == NULL)
        return;

    if (!ctx->have_last_state)
        return;

    if (overdrive_active(ctx)) {
        if (ctx->cfg->offline_enabled) {
            cpu_apply_online(&ctx->cpu);
            schedule_deisolate(ctx);
        }

        cpu_restore_offline_limit(&ctx->cpu, ctx->cfg);
        return;
    }

    if (ctx->last_screen_on) {
        cpu_restore_offline_limit(&ctx->cpu, ctx->cfg);
        return;
    }

    if (!savings_allowed(ctx)) {
        cpu_restore_offline_limit(&ctx->cpu, ctx->cfg);
        return;
    }

    if (ctx->audio_playing_cached) {
        if (ctx->cfg->offline_enabled) {
            cpu_apply_online(&ctx->cpu);
            schedule_deisolate(ctx);
        }

        cpu_restore_offline_limit(&ctx->cpu, ctx->cfg);
        return;
    }

    cpu_restore_offline_limit(&ctx->cpu, ctx->cfg);

    if (ctx->cfg->offline_enabled) {
        schedule_isolate(ctx);
        cpu_apply_offline(&ctx->cpu);
    }
}

static void
apply_neutral(BatmanContext *ctx, gboolean screen_on)
{
    if (!ctx)
        return;

    /*
     * bring CPUs online (if we previously offlined)
     * restore default governors
     * restore device node defaults/boost tweaks
     * ensure bluetooth isn't forced off
     * disable wifi powersave knobs if wifi is active
     * deisolate if needed
     */
    if (ctx->cfg->offline_enabled) {
        cpu_apply_online(&ctx->cpu);
        schedule_deisolate(ctx);
    }

    if (ctx->cfg->powersave_enabled) {
        cpu_apply_default(&ctx->cpu);
        gpu_set_default(&ctx->gpu, ctx->cfg);
        device_node_apply_default(&ctx->devnodes, ctx->cfg);
        bluetooth_apply_default(&ctx->bt, ctx->cfg);
    } else {
        bluetooth_apply_default(&ctx->bt, ctx->cfg);
    }

    if (ctx->cfg->wifi_enabled && ctx->wifi_initialized) {
        wifi_set_powersave(WIFI_IFACE, FALSE);
        wifi_set_wmtwifi(WIFI_IFACE, WMTWIFI_RESUME_VALUE);
    }

    if (ctx->cfg->binder_enabled && ctx->binder)
        binder_set_performance(ctx->binder);

    /* clear any audio limit state bookkeeping */
    cpu_restore_offline_limit(&ctx->cpu, ctx->cfg);

    irqbalance_apply(ctx, screen_on);
}

void
batman_apply_state(BatmanContext *ctx,
                   gboolean screen_on)
{
    if (!ctx)
        return;

    nice_apply(ctx, screen_on);

    /* if charging and CHARGESAVE=false, neutralize and do nothing */
    if (!savings_allowed(ctx)) {
        ctx->have_last_state = TRUE;
        ctx->last_screen_on = screen_on;

        apply_neutral(ctx, screen_on);
        write_str(BATMAN_SCREEN_STATE_PATH, screen_on ? "yes" : "no");
        return;
    }

    if (ctx->have_last_state && ctx->last_screen_on == screen_on)
        return;

    ctx->have_last_state = TRUE;
    ctx->last_screen_on = screen_on;

    /* Overdrive explicitly overrides normal screen off mode */
    if (overdrive_active(ctx)) {
        apply_overdrive_runtime(ctx, screen_on);
        return;
    }

    if (!screen_on) {
        if (ctx->cfg->powersave_enabled) {
            cpu_apply_powersave(&ctx->cpu);
            gpu_set_powersave(&ctx->gpu, ctx->cfg);
            device_node_apply_powersave(&ctx->devnodes, ctx->cfg);
            bluetooth_apply_powersave(&ctx->bt, ctx->cfg);

            if (ctx->cfg->binder_enabled && ctx->binder)
                binder_set_power_saver(ctx->binder);
        }

        if (ctx->cfg->offline_enabled) {
            if (ctx->audio_playing_cached) {
                cpu_apply_online(&ctx->cpu);
                schedule_deisolate(ctx);
                cpu_restore_offline_limit(&ctx->cpu, ctx->cfg);
            } else {
                cpu_restore_offline_limit(&ctx->cpu, ctx->cfg);
                schedule_isolate(ctx);
                cpu_apply_offline(&ctx->cpu);
            }
        } else {
            cpu_restore_offline_limit(&ctx->cpu, ctx->cfg);
        }

        if (ctx->cfg->wifi_enabled && ctx->wifi_initialized) {
            wifi_set_powersave(WIFI_IFACE, TRUE);
            wifi_set_wmtwifi(WIFI_IFACE, WMTWIFI_SUSPEND_VALUE);
        }
    } else {
        if (ctx->cfg->wifi_enabled && ctx->wifi_initialized) {
            wifi_set_powersave(WIFI_IFACE, FALSE);
            wifi_set_wmtwifi(WIFI_IFACE, WMTWIFI_RESUME_VALUE);
        }

        if (ctx->cfg->offline_enabled) {
            cpu_apply_online(&ctx->cpu);
            schedule_deisolate(ctx);
        }

        if (ctx->cfg->powersave_enabled) {
            cpu_apply_default(&ctx->cpu);
            gpu_set_default(&ctx->gpu, ctx->cfg);
            device_node_apply_default(&ctx->devnodes, ctx->cfg);
            bluetooth_apply_default(&ctx->bt, ctx->cfg);

            if (ctx->cfg->binder_enabled && ctx->binder)
                binder_set_performance(ctx->binder);
        }
    }

    irqbalance_apply(ctx, screen_on);

    write_str(BATMAN_SCREEN_STATE_PATH, screen_on ? "yes" : "no");
}

BatmanContext *
batman_context_new(BatmanConfig *cfg,
                   BatmanQueryFunc is_on_battery,
                   BatmanQueryFunc is_overdrive_active,
                   void *userdata)
{
    BatmanContext *ctx;

    if (!cfg)
        return NULL;

    ctx = g_new0(BatmanContext, 1);
    ctx->cfg = cfg;
    ctx->is_on_battery = is_on_battery;
    ctx->is_overdrive_active = is_overdrive_active;
    ctx->userdata = userdata;

    cpu_init(&ctx->cpu);
    gpu_init(&ctx->gpu, cfg);
    device_node_init(&ctx->devnodes);
    mtk_init(&ctx->mtk);
    bluetooth_init(&ctx->bt);

    ctx->binder = binder_init();

    ctx->wifi_initialized = FALSE;
    if (cfg->wifi_enabled) {
        if (wifi_init() == 0)
            ctx->wifi_initialized = TRUE;
    }

    ctx->nice = nice_init();
    if (!ctx->nice)
        g_warning("nice_init failed");

    ctx->irqbalance_available = systemd_service_exists("irqbalance.service");
    g_debug("irqbalance available: %s", ctx->irqbalance_available ? "True" : "False");

    return ctx;
}

void
batman_context_free(BatmanContext *ctx)
{
    if (!ctx)
        return;

    cancel_isolate(ctx);
    cancel_deisolate(ctx);

    if (ctx->nice)
        nice_cleanup(ctx->nice);

    if (ctx->wifi_initialized)
        wifi_cleanup();

    bluetooth_cleanup(&ctx->bt);

    if (ctx->binder)
        binder_cleanup(ctx->binder);

    g_free(ctx);
}

void
batman_wifi_config_changed(BatmanContext *ctx)
{
    if (!ctx)
        return;

    if (ctx->cfg->wifi_enabled) {
        if (!ctx->wifi_initialized) {
            if (wifi_init() == 0)
                ctx->wifi_initialized = TRUE;
        }
    } else {
        if (ctx->wifi_initialized) {
            wifi_cleanup();
            ctx->wifi_initialized = FALSE;
        }
    }
}
