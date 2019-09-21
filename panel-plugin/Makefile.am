AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-netgraph-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

#
# netgraph plugin
#
plugin_LTLIBRARIES = \
	libnetgraph.la

plugindir = \
	$(libdir)/xfce4/panel/plugins

libnetgraph_la_SOURCES = \
	dialogs.c \
	dialogs.h \
	netdev.c \
	netdev.h \
	netgraph.c \
	netgraph.h

libnetgraph_la_CFLAGS = \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(PLATFORM_CFLAGS)

libnetgraph_la_LDFLAGS = \
       -avoid-version \
       -module \
       -no-undefined \
       -export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
       $(PLATFORM_LDFLAGS)

libnetgraph_la_LIBADD = \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4PANEL_LIBS)

#
# Desktop file
#
desktopdir =								\
	$(datadir)/xfce4/panel/plugins

desktop_DATA =								\
	netgraph.desktop

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	netgraph.desktop.in

CLEANFILES =								\
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
