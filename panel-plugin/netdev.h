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
	char *name;  /* Interface name. */

	guint *samples_rx;  /* Download traffic. */
	guint *samples_tx;  /* Upload traffic. */
	gsize samples_len;
} NetworkDevice;

NetworkDevice *netdev_new(gchar *name);
void netdev_free(NetworkDevice* this);
void netdev_resize(NetworkDevice *this, gsize len);

G_END_DECLS

#endif  /* __NETDEV_H__ */
