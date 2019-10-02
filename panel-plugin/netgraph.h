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

#ifndef __NETGRAPH_H__
#define __NETGRAPH_H__

#include <gtk/gtk.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4util/libxfce4util.h>

#include "netdev.h"

G_BEGIN_DECLS

typedef struct {
	XfcePanelPlugin *plugin;

	guint size;
	gboolean has_frame;
	gboolean has_border;
	GdkRGBA bg_color;
	GdkRGBA rx_color;
	GdkRGBA tx_color;
	guint update_interval;
	guint64 min_scale;
	gchar *dev_names;  /* NULL when monitoring all interfaces. */

	GtkWidget *ebox;
	GtkWidget *box;
	GtkWidget *frame;
	GtkWidget *draw_area;
	guint timeout_id;

	GObject *dev_names_entry;
	guint dev_names_timeout_id;

	GPtrArray *devs;
	gsize hist_len;
	guint64 scale;
} NetgraphPlugin;

void netgraph_redraw(NetgraphPlugin *this);
void netgraph_save(XfcePanelPlugin *plugin, NetgraphPlugin *this);

void netgraph_set_size(NetgraphPlugin *this, guint size);
void netgraph_set_has_frame(NetgraphPlugin *this, gboolean has_frame);
void netgraph_set_has_border(NetgraphPlugin *this, gboolean has_border);
void netgraph_set_update_interval(NetgraphPlugin *this, guint update_interval);
void netgraph_set_min_scale(NetgraphPlugin *this, guint64 min_scale);
void netgraph_set_dev_names(NetgraphPlugin *this, const gchar *dev_names);


/* TODO: This should be moved to xfce-rc.h */
#if GLIB_CHECK_VERSION(2, 44, 0)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(XfceRc, xfce_rc_close)
#endif

G_END_DECLS

#endif  /* __NETGRAPH_H__ */
