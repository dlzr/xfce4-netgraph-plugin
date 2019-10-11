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

#include "dialogs.h"
#include "netdev.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN	"netgraph"

#define DEFAULT_SIZE		32	/* pixels */
#define DEFAULT_HAS_FRAME	TRUE
#define DEFAULT_HAS_BORDER	FALSE
#define DEFAULT_BG_COLOR	"rgba(0,0,0,0)"
#define DEFAULT_RX_COLOR	"rgb(16,80,73)"
#define DEFAULT_TX_COLOR	"rgb(170,83,8)"
#define DEFAULT_UPDATE_INTERVAL	1000	/* milliseconds */
#define DEFAULT_MIN_SCALE	5120	/* bytes/second */


static void netgraph_construct(XfcePanelPlugin *plugin);
static NetgraphPlugin *netgraph_new(XfcePanelPlugin *plugin);
static void netgraph_free(XfcePanelPlugin *plugin, NetgraphPlugin *this);
static void netgraph_load(NetgraphPlugin *this);
static void on_draw(GtkWidget *widget, cairo_t *cr, NetgraphPlugin *this);
static gdouble get_rx_fraction(NetgraphPlugin *this, gsize idx);
static gdouble get_tx_fraction(NetgraphPlugin *this, gsize idx);
static gboolean on_size_changed(XfcePanelPlugin *plugin, guint size, NetgraphPlugin *this);
static void on_orientation_changed(XfcePanelPlugin *plugin, GtkOrientation orientation, NetgraphPlugin *this);
static gboolean on_update(NetgraphPlugin *this);
static void update_netdev_list(NetgraphPlugin *this);
static void update_netdev_stats(NetgraphPlugin *this);
static void update_tooltip(NetgraphPlugin *this);
static gchar *format_human_size(guint64 num, gchar *buf, gsize bufsize);


// Allow variable declarations at the first use.
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"


static void netgraph_construct(XfcePanelPlugin *plugin)
{
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	NetgraphPlugin *this = netgraph_new(plugin);

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

	this->ebox = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(this->ebox), FALSE);
	gtk_event_box_set_above_child(GTK_EVENT_BOX(this->ebox), TRUE);
	gtk_container_add(GTK_CONTAINER(plugin), this->ebox);

	GtkOrientation orientation = xfce_panel_plugin_get_orientation(plugin);
	this->box = gtk_box_new(orientation, 0);
	gtk_container_add(GTK_CONTAINER(this->ebox), this->box);

	this->frame = gtk_frame_new(NULL);
	gtk_box_pack_end(GTK_BOX(this->box), this->frame, TRUE, TRUE, 0);

	this->draw_area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(this->frame), this->draw_area);
	g_signal_connect_after(this->draw_area, "draw", G_CALLBACK(on_draw), this);

	this->devs = g_ptr_array_new_with_free_func((GDestroyNotify)netdev_free);

	netgraph_load(this);

	netgraph_set_size(this, this->size);
	netgraph_set_has_frame(this, this->has_frame);
	netgraph_set_has_border(this, this->has_border);
	netgraph_set_dev_names(this, this->dev_names);

	gtk_widget_show_all(this->ebox);

	/* This must be called last, as it starts the update timer. */
	netgraph_set_update_interval(this, this->update_interval);

	return this;
}

static void netgraph_free(XfcePanelPlugin *plugin, NetgraphPlugin *this)
{
	if (this->timeout_id) g_source_remove(this->timeout_id);

	gtk_widget_destroy(this->ebox);

	g_ptr_array_free(this->devs, TRUE);

	g_free(this->dev_names);

	g_slice_free(NetgraphPlugin, this);
}

static void netgraph_load(NetgraphPlugin *this)
{
	this->size = DEFAULT_SIZE;
	this->has_frame = DEFAULT_HAS_FRAME;
	this->has_border = DEFAULT_HAS_BORDER;
	gdk_rgba_parse(&this->bg_color, DEFAULT_BG_COLOR);
	gdk_rgba_parse(&this->rx_color, DEFAULT_RX_COLOR);
	gdk_rgba_parse(&this->tx_color, DEFAULT_TX_COLOR);
	this->update_interval = DEFAULT_UPDATE_INTERVAL;
	this->min_scale = DEFAULT_MIN_SCALE;
	g_free(this->dev_names);
	this->dev_names = NULL;

	g_autofree gchar *file =
		xfce_panel_plugin_lookup_rc_file(this->plugin);
	if (!file) return;

	g_autoptr(XfceRc) rc = xfce_rc_simple_open(file, TRUE);
	if (!rc) return;

	this->size = xfce_rc_read_int_entry(rc, "size", DEFAULT_SIZE);
	this->has_frame = !!xfce_rc_read_int_entry(rc, "has_frame", DEFAULT_HAS_FRAME);
	this->has_border = !!xfce_rc_read_int_entry(rc, "has_border", DEFAULT_HAS_BORDER);
	gdk_rgba_parse(&this->bg_color, xfce_rc_read_entry(rc, "bg_color", DEFAULT_BG_COLOR));
	gdk_rgba_parse(&this->rx_color, xfce_rc_read_entry(rc, "rx_color", DEFAULT_RX_COLOR));
	gdk_rgba_parse(&this->tx_color, xfce_rc_read_entry(rc, "tx_color", DEFAULT_TX_COLOR));
	this->update_interval = xfce_rc_read_int_entry(rc, "update_interval", DEFAULT_UPDATE_INTERVAL);
	this->min_scale = xfce_rc_read_int_entry(rc, "min_scale", DEFAULT_MIN_SCALE);
	const gchar *dev_names = xfce_rc_read_entry(rc, "dev_names", "");
	if (*dev_names) this->dev_names = g_strdup(dev_names);
}

void netgraph_save(XfcePanelPlugin *plugin, NetgraphPlugin *this)
{
	g_autofree gchar *file =
		xfce_panel_plugin_save_location(plugin, TRUE);
	if (!file) return;

	g_autoptr(XfceRc) rc = xfce_rc_simple_open(file, FALSE);
	if (!rc) return;

	xfce_rc_write_int_entry(rc, "update_interval", this->update_interval);
	xfce_rc_write_int_entry(rc, "size", this->size);
	xfce_rc_write_int_entry(rc, "has_frame", !!this->has_frame);
	xfce_rc_write_int_entry(rc, "has_border", !!this->has_border);

	g_autofree gchar *bg_color = gdk_rgba_to_string(&this->bg_color);
	xfce_rc_write_entry(rc, "bg_color", bg_color);
	g_autofree gchar *rx_color = gdk_rgba_to_string(&this->rx_color);
	xfce_rc_write_entry(rc, "rx_color", rx_color);
	g_autofree gchar *tx_color = gdk_rgba_to_string(&this->tx_color);
	xfce_rc_write_entry(rc, "tx_color", tx_color);

	if (this->dev_names) {
		xfce_rc_write_entry(rc, "dev_names", this->dev_names);
	} else {
		xfce_rc_write_entry(rc, "dev_names", "");
	}
}

void netgraph_set_size(NetgraphPlugin *this, guint size)
{
	this->size = size;
	on_size_changed(this->plugin, xfce_panel_plugin_get_size(this->plugin), this);
}

void netgraph_set_has_frame(NetgraphPlugin *this, gboolean has_frame)
{
	this->has_frame = has_frame;
	gtk_frame_set_shadow_type(GTK_FRAME(this->frame),
		this->has_frame ? GTK_SHADOW_IN : GTK_SHADOW_NONE);
}

void netgraph_set_has_border(NetgraphPlugin *this, gboolean has_border)
{
	this->has_border = has_border;

	int border_width = 2;
	if (xfce_panel_plugin_get_size(this->plugin) <= 26) border_width = 1;
	if (!this->has_border) border_width = 0;

	gtk_container_set_border_width(GTK_CONTAINER(this->box), border_width);
}

void netgraph_set_update_interval(NetgraphPlugin *this, guint update_interval)
{
	this->update_interval = update_interval;

	if (this->timeout_id) g_source_remove(this->timeout_id);
	this->timeout_id = g_timeout_add(this->update_interval, (GSourceFunc)on_update, this);
}

void netgraph_set_min_scale(NetgraphPlugin *this, guint64 min_scale)
{
	this->min_scale = min_scale;
	netgraph_redraw(this);
}

void netgraph_set_dev_names(NetgraphPlugin *this, const gchar *list)
{
	if (!list || !*list) {
		g_free(this->dev_names);
		this->dev_names = NULL;
		g_ptr_array_remove_range(this->devs, 0, this->devs->len);
		update_netdev_list(this);
		return;
	}

	gsize orig_len = this->devs->len;
	g_autoptr(GString) sanitized = g_string_new("");
	g_auto(GStrv) parts = g_strsplit_set(list, ", \t\r\n", -1);
	for (gsize i = 0; parts[i] != NULL; i++) {
		if (*parts[i] == '\0') continue;

		if (sanitized->len != 0) g_string_append(sanitized, ", ");
		g_string_append(sanitized, parts[i]);

		g_ptr_array_add(this->devs, netdev_new(parts[i], this->hist_len));
	}

	if (this->devs->len == orig_len) {
		/* No new devices were added. */
		return;
	}

	if (orig_len != 0) {
		/* Clear the old devices. */
		g_ptr_array_remove_range(this->devs, 0, orig_len);
	}

	g_free(this->dev_names);
	this->dev_names = g_string_free(sanitized, FALSE);
	sanitized = NULL;  /* Prevent a double-free from g_autoptr. */
}

void netgraph_redraw(NetgraphPlugin *this)
{
	gtk_widget_queue_draw(this->draw_area);
}

static void on_draw(GtkWidget *widget, cairo_t *cr, NetgraphPlugin *this)
{
	GtkAllocation alloc;
	gtk_widget_get_allocation(this->draw_area, &alloc);
	guint w = alloc.width;
	guint h = alloc.height;

	if (w > this->hist_len) w = this->hist_len;

	gdk_cairo_set_source_rgba(cr, &this->bg_color);
	cairo_rectangle(cr, 0, 0, w, h);
	cairo_fill(cr);

	cairo_set_line_width(cr, 1.0);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);

	gdk_cairo_set_source_rgba(cr, &this->rx_color);
	for (guint x = 0; x < w; x++) {
		guint seg = (guint)(h * get_rx_fraction(this, w - 1 - x));
		if (!seg) continue;

		cairo_move_to(cr, x + 0.5, h - seg - 0.5);
		cairo_line_to(cr, x + 0.5, h - 0.5);
		cairo_stroke(cr);
	}

	gdk_cairo_set_source_rgba(cr, &this->tx_color);
	for (guint x = 0; x < w; x++) {
		guint seg = (guint)(h * get_tx_fraction(this, w - 1 - x));
		if (!seg) continue;

		cairo_move_to(cr, x + 0.5, 0.5);
		cairo_line_to(cr, x + 0.5, seg - 0.5);
		cairo_stroke(cr);
	}
}

static gdouble get_rx_fraction(NetgraphPlugin *this, gsize idx)
{
	guint64 rx = 0;
	for (gsize i = 0; i < this->devs->len; i++) {
		NetworkDevice *dev = g_ptr_array_index(this->devs, i);
		rx += dev->hist_rx[idx];
	}
	return (gdouble)rx / (gdouble)this->scale;
}

static gdouble get_tx_fraction(NetgraphPlugin *this, gsize idx)
{
	guint64 tx = 0;
	for (gsize i = 0; i < this->devs->len; i++) {
		NetworkDevice *dev = g_ptr_array_index(this->devs, i);
		tx += dev->hist_tx[idx];
	}
	return (gdouble)tx / (gdouble)this->scale;
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

	/* Update the border since it depends on the plugin size. */
	netgraph_set_has_border(this, this->has_border);

	return TRUE;
}

static void on_orientation_changed(XfcePanelPlugin *plugin,
				   GtkOrientation orientation,
				   NetgraphPlugin *this)
{
	on_size_changed(this->plugin, xfce_panel_plugin_get_size(this->plugin), this);
}

static gboolean on_update(NetgraphPlugin *this)
{
	if (this->dev_names == NULL) update_netdev_list(this);

	update_netdev_stats(this);
	update_tooltip(this);
	netgraph_redraw(this);

	return TRUE;  /* Keep the timeout active. */
}

static void update_netdev_list(NetgraphPlugin *this)
{
	g_autoptr(GPtrArray) dev_names = netdev_enumerate();
	if (!dev_names) return;

	gsize i, j;
	for (i = j = 0; i < dev_names->len && j < this->devs->len; ) {
		gchar *dev_name = g_ptr_array_index(dev_names, i);
		NetworkDevice *dev = g_ptr_array_index(this->devs, j);

		gint cmp = g_strcmp0(dev_name, dev->name);
		if (cmp == 0) {
			i++;
			j++;
		} else if (cmp < 0) {
			/* A new netdev appeared, need to add it to devs. */
			g_debug("Found new netdev %s.", dev_name);
			g_ptr_array_insert(this->devs, j,
					   netdev_new(dev_name, this->hist_len));
			i++;
			j++;
		} else {
			/* The current element in devs seems to have
			 * disappeared.  It will get cleaned up later, once
			 * it's been down long enough. */
			j++;
		}
	}
	for (; i < dev_names->len; i++) {
		gchar *dev_name = g_ptr_array_index(dev_names, i);
		g_ptr_array_add(this->devs, netdev_new(dev_name, this->hist_len));
	}
}

static void update_netdev_stats(NetgraphPlugin *this)
{
	this->scale = 0;

	for (gsize i = 0; i < this->devs->len; i++) {
		NetworkDevice *dev = g_ptr_array_index(this->devs, i);
		netdev_update(dev, this->hist_len, this->update_interval);

		/* Don't clean up devs if we're monitoring specific interfaces. */
		if (this->dev_names == NULL) {
			if (dev->down >= this->hist_len) {
				g_debug("Removing netdev %s, was down for %d intervals.", dev->name, dev->down);
				g_ptr_array_remove_index(this->devs, i);
				i--;
				continue;
			}
		}

		this->scale += dev->max_rx + dev->max_tx;
	}

	if (this->scale < this->min_scale) this->scale = this->min_scale;
}

static void update_tooltip(NetgraphPlugin *this)
{
	g_autoptr(GString) label = g_string_new("");

#define BUFSIZE	32
	gchar rx_buf[BUFSIZE], tx_buf[BUFSIZE];
	for (gsize i = 0; i < this->devs->len; i++) {
		NetworkDevice *dev = g_ptr_array_index(this->devs, i);
		format_human_size(dev->hist_rx[0], rx_buf, BUFSIZE);
		format_human_size(dev->hist_tx[0], tx_buf, BUFSIZE);
		g_autofree gchar *dev_name_esc =
			g_markup_escape_text(dev->name, -1);
		g_string_append_printf(
			label, _("<b>%s</b>: %sB/s down; %sB/s up\n"),
			dev_name_esc, rx_buf, tx_buf);
	}

	format_human_size(this->scale, rx_buf, BUFSIZE);
	g_string_append_printf(label, _("current scale: %sB/s"), rx_buf);

	gtk_widget_set_tooltip_markup(this->box, label->str);
#undef BUFSIZE
}

static gchar *format_human_size(guint64 num, gchar *buf, gsize bufsize)
{
	if (num < 1024) {
		g_snprintf(buf, bufsize, "%ld ", num);
	} else if (num < 10240) {
		g_snprintf(buf, bufsize, "%.2f K", (gdouble)num / 1024UL);
	} else if (num < 102400) {
		g_snprintf(buf, bufsize, "%.1f K", (gdouble)num / 1024UL);
	} else if (num < 1024UL * 1024) {
		g_snprintf(buf, bufsize, "%.0f K", (gdouble)num / 1024UL);
	} else if (num < 1024UL * 10240) {
		g_snprintf(buf, bufsize, "%.2f M", (gdouble)num / (1024UL * 1024));
	} else if (num < 1024UL * 102400) {
		g_snprintf(buf, bufsize, "%.1f M", (gdouble)num / (1024UL * 1024));
	} else if (num < 1024UL * 1024 * 1024) {
		g_snprintf(buf, bufsize, "%.0f M", (gdouble)num / (1024UL * 1024));
	} else if (num < 1024UL * 1024 * 10240) {
		g_snprintf(buf, bufsize, "%.2f G", (gdouble)num / (1024UL * 1024 * 1024));
	} else if (num < 1024UL * 1024 * 102400) {
		g_snprintf(buf, bufsize, "%.1f G", (gdouble)num / (1024UL * 1024 * 1024));
	} else if (num < 1024UL * 1024 * 1024 * 1024) {
		g_snprintf(buf, bufsize, "%.0f G", (gdouble)num / (1024UL * 1024 * 1024));
	} else if (num < 1024UL * 1024 * 1024 * 10240) {
		g_snprintf(buf, bufsize, "%.2f T", (gdouble)num / (1024UL * 1024 * 1024 * 1024));
	} else if (num < 1024UL * 1024 * 1024 * 102400) {
		g_snprintf(buf, bufsize, "%.1f T", (gdouble)num / (1024UL * 1024 * 1024 * 1024));
	} else if (num < 1024UL * 1024 * 1024 * 1024 * 1024) {
		g_snprintf(buf, bufsize, "%.0f T", (gdouble)num / (1024UL * 1024 * 1024 * 1024));
	} else if (num < 1024UL * 1024 * 1024 * 1024 * 10240) {
		g_snprintf(buf, bufsize, "%.2f P", (gdouble)num / (1024UL * 1024 * 1024 * 1024 * 1024));
	} else if (num < 1024UL * 1024 * 1024 * 1024 * 102400) {
		g_snprintf(buf, bufsize, "%.1f P", (gdouble)num / (1024UL * 1024 * 1024 * 1024 * 1024));
	} else if (num < 1024UL * 1024 * 1024 * 1024 * 1024 * 1024) {
		g_snprintf(buf, bufsize, "%.0f P", (gdouble)num / (1024UL * 1024 * 1024 * 1024 * 1024));
	}
	buf[bufsize - 1] = '\0';

	return buf;
}

XFCE_PANEL_PLUGIN_REGISTER(netgraph_construct);
