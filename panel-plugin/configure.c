/*
 *  Copyright (C) 2019 David Lazar <dlazar@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "netgraph.h"
#include "configure.h"

#define PLUGIN_WEBSITE "https://TODO"


static void netgraph_configure_response(GtkWidget *dialog,
					gint response,
					NetgraphPlugin *netgraph)
{
	gboolean result;

	if (response == GTK_RESPONSE_HELP) {
		/* Show help. */
		result = g_spawn_command_line_async("exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);

		if (result == FALSE) {
			g_warning(_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
		}
	} else {
		/* Remove the dialog data from the plugin. */
		g_object_set_data(G_OBJECT(netgraph->plugin), "dialog", NULL);

		/* Unlock the panel menu. */
		xfce_panel_plugin_unblock_menu(netgraph->plugin);

		/* Save the plugin. */
		netgraph_save(netgraph->plugin, netgraph);

		/* Destroy the properties dialog. */
		gtk_widget_destroy(dialog);
	}
}

void netgraph_configure(XfcePanelPlugin *plugin, NetgraphPlugin *netgraph)
{
	/* Block the plugin menu. */
	xfce_panel_plugin_block_menu(plugin);

	/* Create the dialog. */
	GtkWidget *dialog = xfce_titled_dialog_new_with_buttons(
			_("netgraph Plugin"),
			GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin))),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			"gtk-help", GTK_RESPONSE_HELP,
			"gtk-close", GTK_RESPONSE_OK,
			NULL);

	/* Link the dialog to the plugin, so we can destroy it when the plugin
	 * is closed, but the dialog is still open. */
	g_object_set_data(G_OBJECT(plugin), "dialog", dialog);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "xfce4-netgraph");
	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(netgraph_configure_response), netgraph);
	gtk_widget_show(dialog);
}

void netgraph_about(XfcePanelPlugin *plugin)
{
	const gchar *auth[] = {
		"David Lazar <dlazar@gmail.com>",
		NULL
	};

	GdkPixbuf *icon = xfce_panel_pixbuf_from_source("xfce4-netgraph-plugin", NULL, 32);

	gtk_show_about_dialog(
			NULL,
			"logo",         icon,
			"license",      xfce_get_license_text(XFCE_LICENSE_TEXT_GPL),
			"version",      PACKAGE_VERSION,
			"program-name", PACKAGE_NAME,
			"comments",     _("Graphical representation of the network traffic"),
			"website",      PLUGIN_WEBSITE,
			"copyright",    _("Copyright \xc2\xa9 2019\n"),
			"authors",      auth,
			NULL);

	if (icon) {
		g_object_unref(G_OBJECT(icon));
	}
}
