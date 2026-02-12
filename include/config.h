/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2026 Bardia Moshiri <bardia@furilabs.com>
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <gio/gio.h>

typedef struct BatmanConfig BatmanConfig;

typedef void (*BatmanConfigChangedCb)(const BatmanConfig *cfg, void *userdata);

/**
 * Batman runtime configuration, loaded from /etc/batman/config or /var/lib/batman/config
 */
struct BatmanConfig {
    gboolean offline_enabled;                 /** Enable CPU offlining logic */
    gboolean powersave_enabled;               /** Enable overall powersave behavior */
    gboolean chargesave_enabled;              /** If true, treat charging state as discharging */
    gboolean gpu_powersave_enabled;           /** Enable GPU governor powersave */
    gboolean bus_powersave_enabled;           /** Enable devfreq bus governor powersave */
    gboolean bluetooth_powersave_enabled;     /** Enable bluetooth auto power off/on */
    gboolean binder_enabled;                  /** Enable binder power control */
    gboolean wifi_enabled;                    /** Enable wifi suspend/resume */

    char devfreq_gpu_path[4096];              /** Optional override for GPU devfreq path */
    char config_path[4096];                   /** Resolved config file path used */

    GFileMonitor *monitor_etc;
    GFileMonitor *monitor_var;
    guint reload_source_id;

    BatmanConfigChangedCb changed_cb;
    void *changed_cb_userdata;
};

/**
 * Set default values for BatmanConfig.
 */
void
config_set_defaults(BatmanConfig *cfg);

/**
 * Load configuration from /etc/batman/config or /var/lib/batman/config.
 */
void
config_load(BatmanConfig *cfg);

/**
 * Start monitoring config paths and auto-reload on changes.
 *
 * Watches both:
 *   /etc/batman/config
 *   /var/lib/batman/config
 *
 * When a change is detected, config_load(cfg) is called,
 * then @cb is invoked (if non-NULL).
 *
 * Returns true if at least one monitor was started.
 */
gboolean
config_monitor_start(BatmanConfig *cfg,
                     BatmanConfigChangedCb cb,
                     void *userdata);

/**
 * Stop monitoring and free monitor resources.
 */
void
config_monitor_stop(BatmanConfig *cfg);

#endif /* CONFIG_H */
