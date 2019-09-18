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
	guint update_interval;
	gboolean has_frame;
	gboolean has_border;

	guint timeout_id;

	GtkWidget *ebox;
	GtkWidget *box;
	GtkWidget *frame;
	GtkWidget *draw_area;
	GtkWidget *tooltip_label;

	NetworkDevice **devs;
	gsize devs_len;
} NetgraphPlugin;

void netgraph_save(XfcePanelPlugin *plugin, NetgraphPlugin *this);

void netgraph_set_size(NetgraphPlugin *this, guint size);
void netgraph_set_update_interval(NetgraphPlugin *this, guint update_interval);

G_END_DECLS

#endif  /* __NETGRAPH_H__ */
