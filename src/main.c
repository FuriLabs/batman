/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2026 Bardia Moshiri <bardia@furilabs.com>
 */

#include <stdio.h>
#include <upower.h>

#include "batman.h"
#include "config.h"
#include "utils.h"
#include "logind.h"
#include "wlrdisplay.h"
#include "ppd.h"
#include "pulse.h"

#define BATMAN_STATE_DIR "/var/lib/batman"
#define BATMAN_CUSTOM_UID_PATH BATMAN_STATE_DIR "/CUSTOM_UID"

typedef struct {
    GMainLoop *loop;
    UpClient *upower;

    BatmanConfig cfg;
    BatmanContext *batman;
    PulseContext pulse;

    LogindMonitor *logind;
    PpdContext *ppd;
} BatmanApp;

static gboolean
read_uid_file(const char *path, uid_t *out_uid)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *contents = NULL;

    if (out_uid == NULL || path == NULL)
        return FALSE;

    if (!g_file_get_contents(path, &contents, NULL, &error))
        return FALSE;

    g_strstrip(contents);
    if (contents[0] == '\0')
        return FALSE;

    char *endp = NULL;
    unsigned long v = strtoul(contents, &endp, 10);
    if (endp == contents || *endp != '\0')
        return FALSE;

    *out_uid = (uid_t)v;
    return TRUE;
}

static gboolean
pick_user_id(uid_t *out_uid)
{
    if (out_uid == NULL)
        return FALSE;

    GDir *d = g_dir_open("/run/user", 0, NULL);
    if (d == NULL)
        return FALSE;

    const char *name = NULL;

    while ((name = g_dir_read_name(d)) != NULL) {
        /* only numeric entries */
        gboolean all_digits = TRUE;
        for (const char *p = name; *p; p++) {
            if (!g_ascii_isdigit((guchar)*p)) {
                all_digits = FALSE;
                break;
            }
        }

        if (!all_digits)
            continue;

        char *endp = NULL;
        unsigned long v = strtoul(name, &endp, 10);
        if (endp == name || *endp != '\0')
            continue;

        *out_uid = (uid_t)v;
        g_dir_close(d);
        return TRUE;
    }

    g_dir_close(d);
    return FALSE;
}

static gboolean
find_batman_uid(uid_t *out_uid)
{
    if (out_uid == NULL)
        return FALSE;

    uid_t uid = getuid();

    /*
     * find the user session uid:
     *   /var/lib/batman/CUSTOM_UID if present
     *   first numeric directory under /run/user
     */
    if (uid == 0) {
        uid_t custom = 0;
        if (read_uid_file(BATMAN_CUSTOM_UID_PATH, &custom)) {
            uid = custom;
        } else {
            uid_t picked = 0;
            if (pick_user_id(&picked))
                uid = picked;
            else
                return FALSE;
        }
    }

    *out_uid = uid;
    return TRUE;
}

static void
ensure_xdg_runtime_dir(void)
{
    const char *xdg = g_getenv("XDG_RUNTIME_DIR");

    /* keep if already valid */
    if (xdg != NULL && xdg[0] != '\0' && dir_exists(xdg))
        return;

    uid_t target_uid = 0;
    if (!find_batman_uid(&target_uid))
        target_uid = getuid();

    char runtime_path[256];
    snprintf(runtime_path, sizeof(runtime_path), "/run/user/%d", (int)target_uid);

    if (!dir_exists(runtime_path))
        return;

    g_setenv("XDG_RUNTIME_DIR", runtime_path, TRUE);

    /*
     * we use a dedicated extra socket for batman, so root does not need
     * to use the default per-user native socket or pulse cookie logic.
     */
    char pulse_server[256];
    snprintf(pulse_server, sizeof(pulse_server),
             "unix:%s/pulse/batman", runtime_path);

    g_setenv("PULSE_SERVER", pulse_server, TRUE);
}

static void
ensure_batman_state_dir(void)
{
    if (!dir_exists(BATMAN_STATE_DIR))
        ensure_dir(BATMAN_STATE_DIR, 0755);

    uid_t target_uid = 0;
    if (!find_batman_uid(&target_uid))
        return;

    chown_recursive(BATMAN_STATE_DIR, target_uid, (gid_t)target_uid);
}

static gboolean
app_is_on_battery(BatmanApp *app)
{
    if (app == NULL || app->upower == NULL)
        return TRUE;

    return up_client_get_on_battery(app->upower) ? TRUE : FALSE;
}

static gboolean
get_current_screen_on(BatmanApp *app)
{
    if (app && app->logind) {
        LogindScreenState s = logind_monitor_get_screen_state(app->logind);
        if (s == LOGIND_SCREEN_ON)
            return TRUE;
        if (s == LOGIND_SCREEN_OFF)
            return FALSE;
    }

    int wlr = get_wlroots_screen_status();
    return (wlr != 0) ? TRUE : FALSE;
}

static gboolean
app_overdrive_active(const BatmanApp *app)
{
    if (app == NULL || app->ppd == NULL)
        return FALSE;

    return ppd_is_overdrive_enabled(app->ppd) ? TRUE : FALSE;
}

static gboolean
app_is_on_battery_cb(void *userdata)
{
    return app_is_on_battery(userdata);
}

static gboolean
app_overdrive_active_cb(void *userdata)
{
    return app_overdrive_active(userdata);
}

static void
on_pulse_audio_state_changed(const PulseContext *pulse,
                             gboolean audio_playing,
                             void *userdata)
{
    BatmanApp *app = userdata;

    (void)pulse;

    if (app == NULL)
        return;

    if (app->batman->audio_playing_cached == audio_playing)
        return;

    app->batman->audio_playing_cached = audio_playing;

    g_debug("pulse: cached audio state changed: %s",
            app->batman->audio_playing_cached ? "YES" : "NO");

    if (!get_current_screen_on(app))
        batman_refresh_audio_offline_state(app->batman);
}

static void
app_apply_state(BatmanApp *app,
                gboolean screen_on)
{
    if (!app)
        return;

    ppd_set_screen_off(app->ppd, !screen_on);
    batman_apply_state(app->batman, screen_on);
}

static void
app_reapply_current_state(BatmanApp *app)
{
    if (!app)
        return;

    gboolean screen_on = get_current_screen_on(app);

    app->batman->have_last_state = FALSE;
    app_apply_state(app, screen_on);
}

static void
on_config_reloaded(const BatmanConfig *cfg, void *userdata)
{
    (void)cfg;
    BatmanApp *app = userdata;
    if (!app)
        return;

    g_info("config reloaded from %s", app->cfg.config_path[0] ? app->cfg.config_path : "(none)");

    batman_wifi_config_changed(app->batman);

    /* apply new config immediately */
    app_reapply_current_state(app);
}

static void
on_upower_on_battery_notify(GObject *obj, GParamSpec *pspec, gpointer userdata)
{
    (void)obj;
    (void)pspec;
    BatmanApp *app = userdata;
    if (!app)
        return;

    gboolean on_batt = app_is_on_battery(app);
    g_debug("upower: on-battery changed: %s", on_batt ? "YES" : "NO");

    /* re-apply state because CHARGESAVE may neutralize or enable savings */
    app_reapply_current_state(app);
}

static void
on_logind_screen_changed(LogindScreenState state,
                         void *userdata)
{
    BatmanApp *app = userdata;

    if (app == NULL)
        return;

    if (state == LOGIND_SCREEN_ON) {
        g_debug("logind: screen state changed to ON");
        app_apply_state(app, TRUE);
        return;
    }

    if (state == LOGIND_SCREEN_OFF) {
        g_debug("logind: screen state changed to OFF");
        app_apply_state(app, FALSE);
        return;
    }

    g_debug("logind: screen state UNKNOWN, falling back to wlroots");

    int wlr = get_wlroots_screen_status();
    g_debug("wlroots fallback status = %d", wlr);
    app_apply_state(app, (wlr != 0) ? TRUE : FALSE);
}

static gboolean
initial_probe_cb(gpointer userdata)
{
    BatmanApp *app = userdata;

    if (app == NULL)
        return G_SOURCE_REMOVE;

    if (app->logind != NULL) {
        LogindScreenState s = logind_monitor_get_screen_state(app->logind);

        if (s == LOGIND_SCREEN_ON) {
            g_debug("initial probe: logind reports screen ON");
            app_apply_state(app, TRUE);
            return G_SOURCE_REMOVE;
        }

        if (s == LOGIND_SCREEN_OFF) {
            g_debug("initial probe: logind reports screen OFF");
            app_apply_state(app, FALSE);
            return G_SOURCE_REMOVE;
        }

        g_debug("initial probe: logind state UNKNOWN");
    }

    int wlr = get_wlroots_screen_status();
    g_debug("initial probe: wlroots fallback status = %d", wlr);
    app_apply_state(app, (wlr != 0) ? TRUE : FALSE);

    return G_SOURCE_REMOVE;
}

int
main(void)
{
    BatmanApp app;

    ensure_xdg_runtime_dir();
    ensure_batman_state_dir();

    memset(&app, 0, sizeof(app));

    app.loop = g_main_loop_new(NULL, FALSE);

    config_set_defaults(&app.cfg);
    config_load(&app.cfg);

    if (!config_monitor_start(&app.cfg, on_config_reloaded, &app))
        g_debug("config live-reload monitor not active");

    app.upower = up_client_new();

    if (app.upower)
        g_signal_connect(app.upower, "notify::on-battery",
                         G_CALLBACK(on_upower_on_battery_notify), &app);

    app.batman = batman_context_new(&app.cfg,
                                    app_is_on_battery_cb,
                                    app_overdrive_active_cb,
                                    &app);
    pulse_init(&app.pulse, on_pulse_audio_state_changed, &app);
    app.batman->audio_playing_cached = pulse_is_audio_playing(&app.pulse);

    app.ppd = ppd_init(&app.batman->cpu);
    if (!app.ppd)
        g_warning("ppd_init failed (PowerProfiles D-Bus service not available)");

    app.logind = logind_monitor_new(on_logind_screen_changed, &app);

    g_idle_add(initial_probe_cb, &app);

    g_main_loop_run(app.loop);

    config_monitor_stop(&app.cfg);

    if (app.logind)
        logind_monitor_free(app.logind);

    if (app.ppd)
        ppd_cleanup(app.ppd);

    pulse_cleanup(&app.pulse);

    batman_context_free(app.batman);

    if (app.upower) {
        g_object_unref(app.upower);
        app.upower = NULL;
    }

    g_main_loop_unref(app.loop);

    return 0;
}
