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

#ifndef __NETDEV_H__
#define __NETDEV_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	gchar *name;  /* Interface name. */

	guint64 rx_bytes;
	guint64 tx_bytes;

	guint64 *hist_rx;  /* Download traffic. */
	guint64 *hist_tx;  /* Upload traffic. */
	guint64 max_rx;
	guint64 max_tx;

	guint down;  /* Number of updates when the interface was down. */

#ifdef __linux__
	gchar *rx_bytes_file;
	gchar *tx_bytes_file;
#endif
} NetworkDevice;


/* Returns the list of network device names that are currently up. */
GPtrArray *netdev_enumerate(void);

NetworkDevice *netdev_new(gchar *name, gsize hist_len);
void netdev_free(NetworkDevice* this);
void netdev_resize(NetworkDevice *this, gsize oldlen, gsize newlen);
void netdev_update(NetworkDevice *this, gsize hist_len, guint interval);

G_END_DECLS

#endif  /* __NETDEV_H__ */
