/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2026 Bardia Moshiri <bardia@furilabs.com>
 */

#include "device_node.h"
#include "utils.h"

#include <errno.h>

static void
dn_add_governor(DeviceNodeContext *dn, const char *path)
{
    DeviceGovernorNode *it;

    if (dn == NULL)
        return;
    if (path == NULL)
        return;
    if (dn->governor_count >= (int)(sizeof(dn->governors) / sizeof(dn->governors[0])))
        return;
    if (!exists(path))
        return;

    it = &dn->governors[dn->governor_count];
    memset(it, 0, sizeof(*it));

    it->present = TRUE;
    g_strlcpy(it->path, path, sizeof(it->path));

    if (!read_str(path, it->default_value, sizeof(it->default_value)))
        it->default_value[0] = '\0';

    dn->governor_count++;
}

static void dn_tweak_orig_capture(DeviceNodeContext *dn);

static void
dn_write_str_if_exists(const char *path, const char *value)
{
    if (exists(path))
        write_str(path, value);
}

void
device_node_init(DeviceNodeContext *dn)
{
    if (dn == NULL)
        return;

    memset(dn, 0, sizeof(*dn));

    /* QCOM */
    dn_add_governor(dn, "/sys/class/devfreq/aa00000.qcom,vidc1:arm9_bus_ddr/governor");
    dn_add_governor(dn, "/sys/class/devfreq/aa00000.qcom,vidc:arm9_bus_ddr/governor");
    dn_add_governor(dn, "/sys/class/devfreq/aa00000.qcom,vidc1:bus_cnoc/governor");
    dn_add_governor(dn, "/sys/class/devfreq/aa00000.qcom,vidc:bus_cnoc/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cci/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpubw/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,gpubw/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,kgsl-busmon/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,mincpubw/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,l3-cdsp/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,memlat-cpu0/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,memlat-cpu4/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,memlat-cpu6/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,mincpu0bw/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,mincpu6bw/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:devfreq_spdm_cpu/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cdsp-cdsp-l3-lat/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpu0-cpu-ddr-latfloor/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpu0-cpu-l3-lat/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpu0-cpu-llcc-lat/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpu0-llcc-ddr-lat/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpu6-cpu-ddr-latfloor/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpu6-cpu-l3-lat/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpu6-cpu-llcc-lat/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpu6-llcc-ddr-lat/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpu-cpu-llcc-bw/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,cpu-llcc-ddr-bw/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,npudsp-npu-ddr-bw/governor");
    dn_add_governor(dn, "/sys/class/devfreq/soc:qcom,npu-npu-ddr-bw/governor");

    /* Exynos/Tensor */
    dn_add_governor(dn, "/sys/class/devfreq/17000010.devfreq_mif/governor");
    dn_add_governor(dn, "/sys/class/devfreq/17000020.devfreq_int/governor");
    dn_add_governor(dn, "/sys/class/devfreq/17000030.devfreq_intcam/governor");
    dn_add_governor(dn, "/sys/class/devfreq/17000040.devfreq_disp/governor");
    dn_add_governor(dn, "/sys/class/devfreq/17000050.devfreq_cam/governor");
    dn_add_governor(dn, "/sys/class/devfreq/17000060.devfreq_tnr/governor");
    dn_add_governor(dn, "/sys/class/devfreq/17000070.devfreq_mfc/governor");
    dn_add_governor(dn, "/sys/class/devfreq/17000080.devfreq_bo/governor");

    /* Google/GS */
    dn_add_governor(dn, "/sys/class/devfreq/gs_memlat_devfreq:devfreq_mif_cpu0_memlat@17000010/governor");
    dn_add_governor(dn, "/sys/class/devfreq/gs_memlat_devfreq:devfreq_mif_cpu1_memlat@17000010/governor");
    dn_add_governor(dn, "/sys/class/devfreq/gs_memlat_devfreq:devfreq_mif_cpu2_memlat@17000010/governor");
    dn_add_governor(dn, "/sys/class/devfreq/gs_memlat_devfreq:devfreq_mif_cpu3_memlat@17000010/governor");
    dn_add_governor(dn, "/sys/class/devfreq/gs_memlat_devfreq:devfreq_mif_cpu4_memlat@17000010/governor");
    dn_add_governor(dn, "/sys/class/devfreq/gs_memlat_devfreq:devfreq_mif_cpu5_memlat@17000010/governor");
    dn_add_governor(dn, "/sys/class/devfreq/gs_memlat_devfreq:devfreq_mif_cpu6_memlat@17000010/governor");
    dn_add_governor(dn, "/sys/class/devfreq/gs_memlat_devfreq:devfreq_mif_cpu7_memlat@17000010/governor");

    dn_tweak_orig_capture(dn);
}

/*
 * Vendor tweak knobs: the value written at screen off (save) and the one
 * written at screen on (boost). Nodes that do not exist are skipped.
 */
typedef struct {
    const char *path;
    const char *save;
    const char *boost;
} DeviceTweak;

static const DeviceTweak dn_tweaks[] = {
    /* MediaTek CPUFreq */
    { "/proc/cpufreq/cpufreq_power_mode", "1", "3" },
    { "/proc/cpufreq/cpufreq_cci_mode", "0", "1" },
    { "/proc/cpufreq/cpufreq_sched_disable", "1", "0" },
    { "/proc/cpufreq/MT_CPU_DVFS_CCI/cpufreq_turbo_mode", "0", "1" },
    { "/proc/cpufreq/MT_CPU_DVFS_L/cpufreq_turbo_mode", "0", "1" },
    { "/proc/cpufreq/MT_CPU_DVFS_LL/cpufreq_turbo_mode", "0", "1" },

    /* MediaTek */
    { "/sys/devices/system/cpu/perf/enable", "0", "1" },
    { "/sys/devices/system/cpu/sched/sched_boost", "0", "1" },

    /* MediaTek GED */
    { "/sys/module/ged/parameters/boost_gpu_enable", "0", "1" },
    { "/sys/module/ged/parameters/boost_extra", "0", "1" },
    { "/sys/module/ged/parameters/gx_game_mode", "0", "1" },
    { "/sys/module/ged/parameters/gx_boost_on", "0", "1" },
    { "/sys/module/ged/parameters/ged_force_mdp_enable", "1", "1" },
    { "/sys/module/ged/parameters/boost_amp", "0", "1" },
    { "/sys/module/ged/parameters/ged_boost_enable", "0", "1" },
    { "/sys/module/ged/parameters/enable_gpu_boost", "0", "1" },
    { "/sys/module/ged/parameters/enable_cpu_boost", "0", "1" },

    /* MediaTek GBE */
    { "/sys/kernel/gbe/gbe_enable1", "0", "1" },
    { "/sys/kernel/gbe/gbe_enable2", "0", "1" },
    { "/sys/kernel/gbe/gbe2_max_boost_cnt", "2", "2" },
    { "/sys/kernel/gbe/gbe2_loading_th", "5", "20" },

    /* QCOM */
    { "/sys/module/lpm_levels/parameters/sleep_disabled", "N", "Y" },
    { "/sys/module/lpm_levels/parameters/lpm_prediction", "N", "N" },
    { "/sys/module/lpm_levels/parameters/lpm_ipi_prediction", "N", "N" },
    { "/sys/kernel/debug/msm_vidc/fw_low_power_mode", "1", "0" },

    /* UFS */
    { "/sys/devices/platform/soc/1d84000.ufshc/hibern8_on_idle_enable", "1", "0" },
    { "/sys/devices/platform/soc/1d84000.ufshc/clkgate_enable", "1", "0" },
    { "/sys/devices/platform/soc/1d84000.ufshc/clkscale_enable", "1", "0" },

    /* MediaTek FPSGO */
    { "/sys/kernel/fpsgo/fstb/adopt_low_fps", "1", "0" },
    { "/sys/kernel/fpsgo/fbt/boost_ta", "0", "1" },
    { "/sys/kernel/fpsgo/common/gpu_block_boost", "0", "101" },

    /* MediaTek apusys */
    { "/sys/kernel/apusys/mnoc_apu_qos_boost", "0", "1" },

    /* MediaTek perfmgr */
    { "/proc/perfmgr/boost_ctrl/dram_ctrl/ddr", "0", "2" },
    { "/proc/perfmgr/boost_ctrl/eas_ctrl/sched_boost", "0", "1" },

    /* DRM */
    { "/sys/module/drm_kms_helper/parameters/poll", "N", "Y" },

    /* schedtune */
    { "/sys/fs/cgroup/schedtune/schedtune.boost", "0", "40" },
    { "/sys/fs/cgroup/schedtune/schedtune.prefer_idle", "0", "1" },
};

G_STATIC_ASSERT(G_N_ELEMENTS(dn_tweaks) <= DEVICE_NODE_TWEAK_MAX);

/*
 * The pre-batman value of each knob is kept here so that BOOST=false can put
 * it back at screen on. It lives in /run so that a restart of batman while the
 * screen is off does not take batman's own save values for the originals; it
 * is gone after a reboot, when the knobs are back at their boot values too.
 */
#define DN_TWEAK_ORIG_DIR  "/run/batman"
#define DN_TWEAK_ORIG_PATH DN_TWEAK_ORIG_DIR "/tweak-defaults"

/*
 * Only values that read back in the form they are written are kept: a plain
 * integer or Y/N. MediaTek's /proc/cpufreq nodes read back as prose
 * ("Low Power mode", "turbo_mode(support, disable, loc_opt) = 0, 1, 0") and
 * cannot be restored, so with BOOST=false they are not touched at all.
 */
static gboolean
dn_value_restorable(const char *v)
{
    const char *c = v;

    if (v == NULL || v[0] == '\0')
        return FALSE;
    if ((v[0] == 'Y' || v[0] == 'N') && v[1] == '\0')
        return TRUE;
    if (*c == '-')
        c++;
    if (*c == '\0')
        return FALSE;
    for (; *c; c++) {
        if (!g_ascii_isdigit(*c))
            return FALSE;
    }
    return TRUE;
}

static gboolean
dn_tweak_orig_load(DeviceNodeContext *dn)
{
    g_autofree char *buf = NULL;
    g_auto(GStrv) lines = NULL;
    gboolean any = FALSE;
    gsize i, j;

    if (!g_file_get_contents(DN_TWEAK_ORIG_PATH, &buf, NULL, NULL))
        return FALSE;

    lines = g_strsplit(buf, "\n", -1);
    for (i = 0; lines[i]; i++) {
        char *tab = strchr(lines[i], '\t');

        if (tab == NULL)
            continue;
        *tab = '\0';
        if (!dn_value_restorable(tab + 1))
            continue;

        for (j = 0; j < G_N_ELEMENTS(dn_tweaks); j++) {
            if (strcmp(dn_tweaks[j].path, lines[i]) != 0)
                continue;
            g_strlcpy(dn->tweak_orig[j], tab + 1, sizeof(dn->tweak_orig[j]));
            any = TRUE;
            break;
        }
    }

    return any;
}

static void
dn_tweak_orig_capture(DeviceNodeContext *dn)
{
    g_autoptr(GString) out = g_string_new(NULL);
    g_autoptr(GError) err = NULL;
    gsize i;

    if (dn_tweak_orig_load(dn)) {
        g_debug("device_node: tweak defaults loaded from %s", DN_TWEAK_ORIG_PATH);
        return;
    }

    for (i = 0; i < G_N_ELEMENTS(dn_tweaks); i++) {
        char val[sizeof(dn->tweak_orig[0])];

        if (!read_str(dn_tweaks[i].path, val, sizeof(val)))
            continue;
        if (!dn_value_restorable(val)) {
            g_debug("device_node: %s reads back as \"%s\", not restorable",
                    dn_tweaks[i].path, val);
            continue;
        }

        g_strlcpy(dn->tweak_orig[i], val, sizeof(dn->tweak_orig[i]));
        g_string_append_printf(out, "%s\t%s\n", dn_tweaks[i].path, val);
    }

    if (g_mkdir_with_parents(DN_TWEAK_ORIG_DIR, 0755) != 0 ||
        !g_file_set_contents(DN_TWEAK_ORIG_PATH, out->str, (gssize)out->len, &err))
        g_debug("device_node: could not save tweak defaults: %s",
                err ? err->message : g_strerror(errno));
}

static void
dn_apply_tweaks(DeviceNodeContext *dn, const BatmanConfig *cfg, gboolean save)
{
    gsize i;

    if (!cfg->tweaks_enabled)
        return;

    for (i = 0; i < G_N_ELEMENTS(dn_tweaks); i++) {
        const DeviceTweak *t = &dn_tweaks[i];

        if (cfg->boost_enabled) {
            dn_write_str_if_exists(t->path, save ? t->save : t->boost);
            continue;
        }

        /* BOOST=false: only touch knobs whose original value is known,
         * and put that value back instead of boosting */
        if (dn->tweak_orig[i][0] == '\0')
            continue;
        if (save)
            dn_write_str_if_exists(t->path, t->save);
        else if (dn->tweaks_saved)
            dn_write_str_if_exists(t->path, dn->tweak_orig[i]);
    }

    if (!cfg->boost_enabled)
        dn->tweaks_saved = save;
}

void
device_node_apply_powersave(DeviceNodeContext *dn, const BatmanConfig *cfg)
{
    int i;

    if (dn == NULL)
        return;
    if (cfg == NULL)
        return;

    if (cfg->bus_powersave_enabled) {
        for (i = 0; i < dn->governor_count; i++) {
            const DeviceGovernorNode *it = &dn->governors[i];

            if (!it->present)
                continue;

            write_str(it->path, "powersave");
        }
    }

    dn_apply_tweaks(dn, cfg, TRUE);
}

void
device_node_apply_default(DeviceNodeContext *dn, const BatmanConfig *cfg)
{
    int i;

    if (dn == NULL)
        return;
    if (cfg == NULL)
        return;

    if (cfg->bus_powersave_enabled) {
        for (i = 0; i < dn->governor_count; i++) {
            const DeviceGovernorNode *it = &dn->governors[i];

            if (!it->present)
                continue;
            if (it->default_value[0] == '\0')
                continue;

            write_str(it->path, it->default_value);
        }
    }

    dn_apply_tweaks(dn, cfg, FALSE);
}
