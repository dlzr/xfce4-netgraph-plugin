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

#include "dialogs.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "netgraph.h"
#include "prefs-dialog.glade.h"


#define PLUGIN_WEBSITE "https://TODO"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN	"netgraph"

/* Hook to make sure GtkBuilder knows about the XfceTitledDialog object. */
#define PANEL_UTILS_LINK_4UI \
  if (xfce_titled_dialog_get_type () == 0) return;

#define EDIT_TIMEOUT	1000	/* milliseconds */


static void netgraph_configure_response(GtkWidget *dialog, gint response, NetgraphPlugin *netgraph);
static void on_size_changed(GtkWidget *widget, NetgraphPlugin *this);
static void on_has_frame_changed(GtkWidget *widget, NetgraphPlugin *this);
static void on_has_border_changed(GtkWidget *widget, NetgraphPlugin *this);
static void on_bg_color_changed(GtkWidget *widget, NetgraphPlugin *this);
static void on_rx_color_changed(GtkWidget *widget, NetgraphPlugin *this);
static void on_tx_color_changed(GtkWidget *widget, NetgraphPlugin *this);
static void on_update_interval_changed(GtkWidget *widget, NetgraphPlugin *this);
static void on_min_scale_changed(GtkWidget *widget, NetgraphPlugin *this);
static void on_monitor_devs_changed(GtkWidget *widget, NetgraphPlugin *this);
static void on_dev_names_changed(GtkWidget *widget, NetgraphPlugin *this);
static gboolean on_dev_names_timeout(NetgraphPlugin *this);


void netgraph_configure(XfcePanelPlugin *plugin, NetgraphPlugin *this)
{
	g_autoptr(GError) err = NULL;
	g_autoptr(GtkBuilder) builder = gtk_builder_new();

	PANEL_UTILS_LINK_4UI
	if (!gtk_builder_add_from_string(builder,
			prefs_dialog_glade, prefs_dialog_glade_length, &err)) {
		g_error("Could not construct preferences dialog: %s.", err->message);
		return;
	}

	GObject *dialog = gtk_builder_get_object(builder, "dialog");
	if (!dialog) {
		g_error("Invalid preferences dialog.");
		return;
	}

	xfce_panel_plugin_take_window(plugin, GTK_WINDOW(dialog));

#pragma GCC diagnostic push
	/* GCC complains about the (GWeakNotify) cast, but in this case it's fine. */
#pragma GCC diagnostic ignored "-Wcast-function-type"
	xfce_panel_plugin_block_menu(plugin);
	g_object_weak_ref(dialog, (GWeakNotify)xfce_panel_plugin_unblock_menu, plugin);
#pragma GCC diagnostic pop

	GObject *button = gtk_builder_get_object(builder, "close-button");
	if (button) {
		g_signal_connect_swapped(button, "clicked",
			G_CALLBACK(gtk_widget_destroy), dialog);
	}
	g_signal_connect(dialog, "response",
		G_CALLBACK(netgraph_configure_response), this);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "xfce4-netgraph-plugin");

	/* Populate the dialog and connect the change handlers. */
	GObject *object = NULL;

	GtkOrientation orientation = xfce_panel_plugin_get_orientation(plugin);
	if (orientation == GTK_ORIENTATION_VERTICAL) {
		object = gtk_builder_get_object(builder, "size-label");
		gtk_label_set_text(GTK_LABEL(object), _("Height (px):"));
	}

	object = gtk_builder_get_object(builder, "size");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(object), this->size);
	g_signal_connect(object, "value-changed", G_CALLBACK(on_size_changed), this);

	object = gtk_builder_get_object(builder, "has-frame");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(object), this->has_frame);
	g_signal_connect(object, "toggled", G_CALLBACK(on_has_frame_changed), this);

	object = gtk_builder_get_object(builder, "has-border");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(object), this->has_border);
	g_signal_connect(object, "toggled", G_CALLBACK(on_has_border_changed), this);

	object = gtk_builder_get_object(builder, "bg-color");
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(object), &this->bg_color);
	g_signal_connect(object, "color-set", G_CALLBACK(on_bg_color_changed), this);

	object = gtk_builder_get_object(builder, "rx-color");
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(object), &this->rx_color);
	g_signal_connect(object, "color-set", G_CALLBACK(on_rx_color_changed), this);

	object = gtk_builder_get_object(builder, "tx-color");
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(object), &this->tx_color);
	g_signal_connect(object, "color-set", G_CALLBACK(on_tx_color_changed), this);

	object = gtk_builder_get_object(builder, "update-interval");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(object), this->update_interval);
	g_signal_connect(object, "value-changed",
		G_CALLBACK(on_update_interval_changed), this);

	object = gtk_builder_get_object(builder, "min-scale");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(object), this->min_scale / 1024);
	g_signal_connect(object, "value-changed",
		G_CALLBACK(on_min_scale_changed), this);

	this->dev_names_entry = gtk_builder_get_object(builder, "dev-names");
	object = gtk_builder_get_object(builder, "monitor-devs");
	gtk_combo_box_set_active(GTK_COMBO_BOX(object), (this->dev_names != NULL));
	g_signal_connect(object, "changed", G_CALLBACK(on_monitor_devs_changed), this);

	on_monitor_devs_changed(GTK_WIDGET(object), this);
	g_signal_connect(this->dev_names_entry, "changed",
		G_CALLBACK(on_dev_names_changed), this);

	gtk_widget_show(GTK_WIDGET(dialog));
}

static void netgraph_configure_response(GtkWidget *dialog,
					gint response,
					NetgraphPlugin *this)
{
	if (this->dev_names_timeout_id) {
		/* The user closed the prefs dialog before the changes to
		 * dev_names were applied. */
		g_source_remove(this->dev_names_timeout_id);
		on_dev_names_timeout(this);
		this->dev_names_entry = NULL;
	}

	netgraph_save(this->plugin, this);
}

static void on_size_changed(GtkWidget *widget, NetgraphPlugin *this)
{
	netgraph_set_size(
		this, gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
}

static void on_has_frame_changed(GtkWidget *widget, NetgraphPlugin *this)
{
	netgraph_set_has_frame(
		this, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

static void on_has_border_changed(GtkWidget *widget, NetgraphPlugin *this)
{
	netgraph_set_has_border(
		this, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

static void on_bg_color_changed(GtkWidget *widget, NetgraphPlugin *this)
{
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &this->bg_color);
	netgraph_redraw(this);
}

static void on_rx_color_changed(GtkWidget *widget, NetgraphPlugin *this)
{
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &this->rx_color);
	netgraph_redraw(this);
}

static void on_tx_color_changed(GtkWidget *widget, NetgraphPlugin *this)
{
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &this->tx_color);
	netgraph_redraw(this);
}

static void on_update_interval_changed(GtkWidget *widget, NetgraphPlugin *this)
{
	netgraph_set_update_interval(
		this, gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
}

static void on_min_scale_changed(GtkWidget *widget, NetgraphPlugin *this)
{
	netgraph_set_min_scale(
		this, gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)) * 1024);
}

static void on_monitor_devs_changed(GtkWidget *widget, NetgraphPlugin *this)
{
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widget))) {
		if (this->dev_names) {
			gtk_entry_set_text(GTK_ENTRY(this->dev_names_entry), this->dev_names);
		} else {
			gtk_entry_set_text(GTK_ENTRY(this->dev_names_entry), "");
		}
		gtk_widget_show(GTK_WIDGET(this->dev_names_entry));
	} else {
		gtk_widget_hide(GTK_WIDGET(this->dev_names_entry));
		netgraph_set_dev_names(this, NULL);
	}
}

static void on_dev_names_changed(GtkWidget *widget, NetgraphPlugin *this)
{
	if (this->dev_names_timeout_id) g_source_remove(this->dev_names_timeout_id);
	this->dev_names_timeout_id = g_timeout_add(EDIT_TIMEOUT,
		(GSourceFunc)on_dev_names_timeout, this);
}

static gboolean on_dev_names_timeout(NetgraphPlugin *this)
{
	this->dev_names_timeout_id = 0;

	netgraph_set_dev_names(this, gtk_entry_get_text(GTK_ENTRY(this->dev_names_entry)));

	return FALSE;  /* Do not reactivate the timeout. */
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

	g_object_unref(G_OBJECT(icon));
}
