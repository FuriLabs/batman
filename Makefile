CC = gcc

CFLAGS_LIB = -fPIC `pkg-config --cflags libgbinder glib-2.0 gio-2.0 libnl-3.0 libnl-genl-3.0 libnl-route-3.0` -Iinclude
LDFLAGS_LIB = -shared -Wl,-soname,$(SONAME) `pkg-config --libs libgbinder glib-2.0 gio-2.0 libnl-3.0 libnl-genl-3.0 libnl-route-3.0`

CFLAGS_BATMAN = `pkg-config --cflags upower-glib libgbinder glib-2.0 libnl-3.0 libnl-genl-3.0 libnl-route-3.0 libpulse libpulse-mainloop-glib` -Iinclude
LDFLAGS_BATMAN = `pkg-config --libs upower-glib libgbinder glib-2.0 libnl-3.0 libnl-genl-3.0 libnl-route-3.0 libpulse libpulse-mainloop-glib` -lwayland-client

CFLAGS_MONITOR = `pkg-config --cflags upower-glib` -Iinclude
LDFLAGS_MONITOR = -lwayland-client `pkg-config --libs upower-glib`

CFLAGS_NFCD = -fPIC -DNFC_PLUGIN_EXTERNAL `pkg-config --cflags nfcd-plugin libglibutil gobject-2.0 glib-2.0`
LDFLAGS_NFCD = -fPIC -shared `pkg-config --libs libglibutil gobject-2.0 glib-2.0`

SOURCES_LIB = lib/batman.c \
              lib/utils.c \
              lib/binder.c \
              lib/wifi.c \
              lib/bluetooth.c \
              lib/config.c \
              lib/cpu.c \
              lib/gpu.c \
              lib/device_node.c \
              lib/mtk.c \
              lib/thermal.c \
              lib/nice.c

SOURCES_BATMAN = src/main.c \
                 src/wlrdisplay.c \
                 src/logind.c \
                 src/ppd.c \
                 src/pulse.c

SOURCES_MONITOR = src/monitor/batman-system-monitor.c \
                  src/wlrdisplay.c \
                  lib/utils.c

SOURCES_NFCD = src/nfcd/nfcd-batman-plugin.c

TARGET_LIB = libbatman.so
SONAME = libbatman.so.1
TARGET_BATMAN = batman
TARGET_MONITOR = batman-system-monitor
TARGET_NFCD = batman.so
TARGET_PC = libbatman.pc

VERSION ?= 1.0
PREFIX ?= /usr
TRIPLET ?= $(shell $(CC) -dumpmachine)
LIBDIR ?= $(PREFIX)/lib/$(TRIPLET)
INCLUDEDIR ?= $(PREFIX)/include

all: lib $(TARGET_BATMAN) $(TARGET_MONITOR) $(TARGET_NFCD)

lib: $(TARGET_LIB) $(TARGET_PC)

$(TARGET_LIB): $(SOURCES_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_LIB) $(SOURCES_LIB) $(LDFLAGS) -o $(TARGET_LIB) $(LDFLAGS_LIB)

$(TARGET_BATMAN): $(TARGET_LIB) $(SOURCES_BATMAN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_BATMAN) $(SOURCES_BATMAN) $(LDFLAGS) -o $(TARGET_BATMAN) -L. -lbatman $(LDFLAGS_BATMAN)

$(TARGET_MONITOR):
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_MONITOR) $(SOURCES_MONITOR) $(LDFLAGS) -o $(TARGET_MONITOR) $(LDFLAGS_MONITOR)

$(TARGET_NFCD):
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_NFCD) $(SOURCES_NFCD) $(LDFLAGS) -o $(TARGET_NFCD) $(LDFLAGS_NFCD)

$(TARGET_PC): data/libbatman.pc.in
	sed -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@LIBDIR@|$(LIBDIR)|g' \
	    -e 's|@INCLUDEDIR@|$(INCLUDEDIR)|g' \
	    -e 's|@VERSION@|$(VERSION)|g' \
	    $< > $@

install-lib: lib
	install -d $(DESTDIR)$(LIBDIR)/pkgconfig
	install -d $(DESTDIR)$(INCLUDEDIR)/batman
	install -m 0755 $(TARGET_LIB) $(DESTDIR)$(LIBDIR)/$(SONAME)
	ln -sf $(SONAME) $(DESTDIR)$(LIBDIR)/$(TARGET_LIB)
	install -m 0644 $(TARGET_PC) $(DESTDIR)$(LIBDIR)/pkgconfig/
	install -m 0644 include/batman.h include/config.h include/cpu.h include/gpu.h include/bluetooth.h include/mtk.h include/device_node.h include/binder.h include/nice.h $(DESTDIR)$(INCLUDEDIR)/batman/

install: all install-lib
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/sbin
	install -d $(DESTDIR)/var
	install -d $(DESTDIR)/var/lib/batman
	install -d $(DESTDIR)$(PREFIX)/share/polkit-1/actions
	install -d $(DESTDIR)$(PREFIX)/share/dbus-1/system.d
	install -d $(DESTDIR)$(PREFIX)/lib/$(TRIPLET)
	install -d $(DESTDIR)$(PREFIX)/lib/nfcd/plugins

	install -m 0755 $(TARGET_BATMAN) $(DESTDIR)$(PREFIX)/sbin/
	install -m 0755 $(TARGET_MONITOR) $(DESTDIR)$(PREFIX)/bin/

	install -m 0644 $(TARGET_NFCD) $(DESTDIR)$(PREFIX)/lib/nfcd/plugins/

	install -m 0644 data/config $(DESTDIR)/var/lib/batman/config

	install -m 0644 data/org.freedesktop.UPower.PowerProfiles.policy $(DESTDIR)$(PREFIX)/share/polkit-1/actions/
	install -m 0644 data/org.freedesktop.UPower.PowerProfiles.conf $(DESTDIR)$(PREFIX)/share/dbus-1/system.d/

ifeq ($(strip $(DESTDIR)),)
	install -d $(DESTDIR)$(PREFIX)/lib/systemd/system
	install -m 0644 data/batman.service $(DESTDIR)$(PREFIX)/lib/systemd/system/
endif

	install -m 0644 data/nice.conf $(DESTDIR)/var/lib/batman/nice.conf

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(TARGET_LIB) $(DESTDIR)$(LIBDIR)/$(SONAME)
	rm -f $(DESTDIR)$(LIBDIR)/pkgconfig/$(TARGET_PC)
	rm -rf $(DESTDIR)$(INCLUDEDIR)/batman
	rm -f $(DESTDIR)$(PREFIX)/sbin/$(TARGET_BATMAN)
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET_MONITOR)

	rm -f $(DESTDIR)$(PREFIX)/lib/nfcd/plugins/$(TARGET_NFCD)

	rm -f $(DESTDIR)/var/lib/batman/config

	rm -f $(DESTDIR)$(PREFIX)/share/polkit-1/actions/org.freedesktop.UPower.PowerProfiles.policy
	rm -f $(DESTDIR)$(PREFIX)/share/dbus-1/system.d/org.freedesktop.UPower.PowerProfiles.conf

	rm -f $(DESTDIR)$(PREFIX)/lib/systemd/system/batman.service

	rm -f $(DESTDIR)/var/lib/batman/nice.conf

clean:
	rm -f $(TARGET_LIB) $(TARGET_BATMAN) $(TARGET_MONITOR) $(TARGET_NFCD) $(TARGET_PC)

.PHONY: all lib clean install install-lib uninstall $(TARGET_PC)
