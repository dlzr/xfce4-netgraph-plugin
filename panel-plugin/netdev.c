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

#include "netdev.h"

#include <glib.h>


NetworkDevice *netdev_new(gchar *name)
{
	NetworkDevice *this = g_slice_new0(NetworkDevice);
	this->name = g_strdup(name);

	return this;
}

void netdev_free(NetworkDevice* this)
{
	g_free(this->name);

	g_free(this->samples_tx);
	g_free(this->samples_rx);

	g_slice_free(NetworkDevice, this);
}

void netdev_resize(NetworkDevice *this, gsize len)
{
	if (len == this->samples_len) return;

	this->samples_rx = g_realloc(this->samples_rx, len * sizeof(guint));
	this->samples_tx = g_realloc(this->samples_tx, len * sizeof(guint));
	if (len > this->samples_len) {
		memset(this->samples_rx + this->samples_len, 0,
		       (len - this->samples_len) * sizeof(guint));
		memset(this->samples_tx + this->samples_len, 0,
		       (len - this->samples_len) * sizeof(guint));
	}
	this->samples_len = len;
}
