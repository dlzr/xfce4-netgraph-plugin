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

#include "netgraph.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "configure.h"
#include "netdev.h"


#define DEFAULT_SIZE		32
#define DEFAULT_UPDATE_INTERVAL	1000  /* milliseconds */


static void netgraph_construct(XfcePanelPlugin *plugin);
static NetgraphPlugin *netgraph_new(XfcePanelPlugin *plugin);
static void netgraph_free(XfcePanelPlugin *plugin, NetgraphPlugin *this);
static void netgraph_load(NetgraphPlugin *this);
static void on_draw(GtkWidget *widget, cairo_t *cr, NetgraphPlugin *this);
static gboolean on_size_changed(XfcePanelPlugin *plugin, guint size, NetgraphPlugin *this);
static void on_orientation_changed(XfcePanelPlugin *plugin, GtkOrientation orientation, NetgraphPlugin *this);
static void show_tooltip(NetgraphPlugin *this);
static gboolean on_update(NetgraphPlugin *this);
static void update_netdev_list(NetgraphPlugin *this);
static void update_netdev_stats(NetgraphPlugin *this);

static void netgraph_construct(XfcePanelPlugin *plugin)
{
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	NetgraphPlugin *this = netgraph_new(plugin);
	netgraph_load(this);

	g_signal_connect(plugin, "free-data", G_CALLBACK(netgraph_free), this);
	g_signal_connect(plugin, "save", G_CALLBACK(netgraph_save), this);
	g_signal_connect(plugin, "size-changed", G_CALLBACK(on_size_changed), this);
	g_signal_connect(plugin, "orientation-changed", G_CALLBACK(on_orientation_changed), this);
	g_signal_connect(plugin, "configure-plugin", G_CALLBACK(netgraph_configure), this);
	g_signal_connect(plugin, "about", G_CALLBACK(netgraph_about), this);

	/* Set up the right-click menu. */
	xfce_panel_plugin_add_action_widget(plugin, this->ebox);
	xfce_panel_plugin_menu_show_configure(plugin);
	xfce_panel_plugin_menu_show_about(plugin);
}

static NetgraphPlugin *netgraph_new(XfcePanelPlugin *plugin)
{
	NetgraphPlugin *this = g_slice_new0(NetgraphPlugin);
	this->plugin = plugin;

	this->size = DEFAULT_SIZE;
	this->update_interval = DEFAULT_UPDATE_INTERVAL;

	this->ebox = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(this->ebox), FALSE);
	gtk_event_box_set_above_child(GTK_EVENT_BOX(this->ebox), TRUE);
	gtk_container_add(GTK_CONTAINER(plugin), this->ebox);

	GtkOrientation orientation = xfce_panel_plugin_get_orientation(plugin);
	this->box = gtk_box_new(orientation, 0);
	gtk_container_add(GTK_CONTAINER(this->ebox), this->box);
	gtk_widget_set_has_tooltip(this->box, TRUE);
	g_signal_connect(this->box, "query-tooltip", G_CALLBACK(show_tooltip), this);

	this->frame = gtk_frame_new(NULL);
	gtk_box_pack_end(GTK_BOX(this->box), this->frame, TRUE, TRUE, 0);

	this->draw_area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(this->frame), this->draw_area);
	g_signal_connect_after(this->draw_area, "draw", G_CALLBACK(on_draw), this);

	gtk_widget_show_all(this->ebox);

	this->tooltip_label = gtk_label_new(NULL);
	g_object_ref(this->tooltip_label);

	this->devs = g_ptr_array_new_with_free_func((GDestroyNotify)netdev_free);

	return this;
}

static void netgraph_free(XfcePanelPlugin *plugin, NetgraphPlugin *this)
{
	/* Destroy the configuration dialog if it's still open. */
	GtkWidget *dialog = g_object_get_data(G_OBJECT(plugin), "dialog");
	if (dialog) gtk_widget_destroy(dialog);

	if (this->timeout_id) g_source_remove(this->timeout_id);

	gtk_widget_destroy(this->ebox);
	gtk_widget_destroy(this->tooltip_label);

	g_ptr_array_free(this->devs, TRUE);

	g_free(this->devname);

	g_slice_free(NetgraphPlugin, this);
}

static void netgraph_load(NetgraphPlugin *this)
{
	g_autofree gchar *file =
		xfce_panel_plugin_lookup_rc_file(this->plugin);
	if (!file) return;

	g_autoptr(XfceRc) rc = xfce_rc_simple_open(file, TRUE);
	if (!rc) return;

	guint size = xfce_rc_read_int_entry(rc, "size", DEFAULT_SIZE);
	netgraph_set_size(this, size);

	/* The update_interval setting must be processed last,
	 * as it starts the update timer. */
	guint update_interval = xfce_rc_read_int_entry(
		rc, "update_interval", DEFAULT_UPDATE_INTERVAL);
	netgraph_set_update_interval(this, update_interval);
}

void netgraph_save(XfcePanelPlugin *plugin, NetgraphPlugin *this)
{
	g_autofree gchar *file =
		xfce_panel_plugin_save_location(plugin, TRUE);
	if (!file) return;

	g_autoptr(XfceRc) rc = xfce_rc_simple_open(file, FALSE);
	if (!rc) return;

	xfce_rc_write_int_entry (rc, "size", this->size);
	xfce_rc_write_int_entry (rc, "update_interval", this->update_interval);
}

void netgraph_set_size(NetgraphPlugin *this, guint size)
{
	this->size = size;
	on_size_changed(this->plugin, xfce_panel_plugin_get_size(this->plugin), this);
}

void netgraph_set_update_interval(NetgraphPlugin *this, guint update_interval)
{
	this->update_interval = update_interval;

	if (this->timeout_id) g_source_remove(this->timeout_id);
	this->timeout_id = g_timeout_add(this->update_interval, (GSourceFunc)on_update, this);
}

static void on_draw(GtkWidget *widget, cairo_t *cr, NetgraphPlugin *this)
{
	GtkAllocation alloc;
	gtk_widget_get_allocation(this->draw_area, &alloc);

	GdkRGBA color;
	gdk_rgba_parse(&color, "#884444");

	gdk_cairo_set_source_rgba(cr, &color);
	cairo_rectangle(cr, 0, 0, alloc.width, alloc.height);
	cairo_fill(cr);
}

static gboolean on_size_changed(XfcePanelPlugin *plugin,
				guint size,
				NetgraphPlugin *this)
{
	guint width = size;
	guint height = size;

	GtkOrientation orientation = xfce_panel_plugin_get_orientation(plugin);
	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		width = this->size;
	} else {
		height = this->size;
	}

	gtk_widget_set_size_request(GTK_WIDGET(this->frame), width, height);

	if (width != this->hist_len) {
		for (gsize i = 0; i < this->devs->len; i++) {
			NetworkDevice *dev = g_ptr_array_index(this->devs, i);
			netdev_resize(dev, this->hist_len, width);
		}
	}
	this->hist_len = width;

	return TRUE;
}

static void on_orientation_changed(XfcePanelPlugin *plugin,
				   GtkOrientation orientation,
				   NetgraphPlugin *this)
{
	// TODO
}

static void show_tooltip(NetgraphPlugin *this)
{
	// TODO
}

static gboolean on_update(NetgraphPlugin *this)
{
	if (this->devname == NULL) update_netdev_list(this);

	update_netdev_stats(this);

	gtk_widget_queue_draw(this->draw_area);

	return TRUE;  /* Keep the timeout active. */
}

static void update_netdev_list(NetgraphPlugin *this)
{
	g_autoptr(GPtrArray) devnames = netdev_enumerate();
	if (!devnames) return;

	gsize i, j;
	for (i = j = 0; i < devnames->len && j < this->devs->len; ) {
		gchar *devname = g_ptr_array_index(devnames, i);
		NetworkDevice *dev = g_ptr_array_index(this->devs, j);

		gint cmp = g_strcmp0(devname, dev->name);
		if (cmp == 0) {
			i++;
			j++;
		} else if (cmp < 0) {
			/* A new netdev appeared, need to add it to devs. */
			g_ptr_array_insert(this->devs, j,
					   netdev_new(devname, this->hist_len));
			i++;
			j++;
		} else {
			/* The current element in devs seems to have
			 * disappeared.  It will get cleaned up later, once
			 * it's been down long enough. */
			j++;
		}
	}
	for (; i < devnames->len; i++) {
		gchar *devname = g_ptr_array_index(devnames, i);
		g_ptr_array_add(this->devs, netdev_new(devname, this->hist_len));
	}
}

static void update_netdev_stats(NetgraphPlugin *this)
{
	for (gsize i = 0; i < this->devs->len; i++) {
		NetworkDevice *dev = g_ptr_array_index(this->devs, i);
		netdev_update(dev, this->hist_len);

		/* Don't clean up devs if we're monitoring a specific interface. */
		if (this->devname != NULL) continue;

		if (dev->down >= this->hist_len) {
			/* The device has been down for too long (it no longer
			 * has data in the history), so we stop tracking it. */
			g_ptr_array_remove_index(this->devs, i);
			i--;
		}
	}
}

XFCE_PANEL_PLUGIN_REGISTER(netgraph_construct);
