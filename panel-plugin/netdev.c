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

typedef struct {
	gboolean is_up;
	guint64 rx_bytes;
	guint64 tx_bytes;
} DeviceStats;

/* Functions defined in the OS-specific files. */
static void netdev_os_init(NetworkDevice *this);
static void netdev_os_free(NetworkDevice *this);
static void netdev_os_read_stats(NetworkDevice *this, DeviceStats* stats);

#ifdef __linux__
#include "os_linux.c"
#endif


static void shift_get_max(guint64 *hist, gsize len, guint64 *max);


NetworkDevice *netdev_new(gchar *name, gsize hist_len)
{
	NetworkDevice *this = g_slice_new0(NetworkDevice);
	this->name = g_strdup(name);
	this->hist_rx = g_new0(guint64, hist_len);
	this->hist_tx = g_new0(guint64, hist_len);

	netdev_os_init(this);

	DeviceStats stats;
	netdev_os_read_stats(this, &stats);
	this->rx_bytes = stats.rx_bytes;
	this->tx_bytes = stats.tx_bytes;

	return this;
}

void netdev_free(NetworkDevice* this)
{
	netdev_os_free(this);

	g_free(this->name);
	g_free(this->hist_tx);
	g_free(this->hist_rx);

	g_slice_free(NetworkDevice, this);
}

void netdev_resize(NetworkDevice *this, gsize oldlen, gsize newlen)
{
	this->hist_rx = g_realloc(this->hist_rx, newlen * sizeof(guint64));
	this->hist_tx = g_realloc(this->hist_tx, newlen * sizeof(guint64));
	if (newlen > oldlen) {
		memset(this->hist_rx + oldlen, 0, (newlen - oldlen) * sizeof(guint64));
		memset(this->hist_tx + oldlen, 0, (newlen - oldlen) * sizeof(guint64));
	}
}

void netdev_update(NetworkDevice *this, gsize hist_len)
{
	/* Read the new sample. */
	DeviceStats stats;
	netdev_os_read_stats(this, &stats);

	/* Make room for the new sample. */
	shift_get_max(this->hist_rx, hist_len, &this->max_rx);
	shift_get_max(this->hist_tx, hist_len, &this->max_tx);

	if (!stats.is_up) {
		/* Add zeroes if the interface is down. */
		this->down++;
		this->hist_rx[0] = 0;
		this->hist_tx[0] = 0;
		return;
	}

	this->down = 0;

	/* Insert the new sample. */
	if (stats.rx_bytes >= this->rx_bytes) {
		this->hist_rx[0] = stats.rx_bytes - this->rx_bytes;
	} else {
		/* The rx_bytes counter is only supposed to go up.  If it went
		 * down, we assume a wrap-around happened, and the counter
		 * restarted from 0. */
		this->hist_rx[0] = stats.rx_bytes;
	}
	if (stats.tx_bytes >= this->tx_bytes) {
		this->hist_tx[0] = stats.tx_bytes - this->tx_bytes;
	} else {
		this->hist_tx[0] = stats.tx_bytes;
	}

	/* Recompute the max values. */
	if (this->hist_rx[0] > this->max_rx) {
		this->max_rx = this->hist_rx[0];
	}
	if (this->hist_tx[0] > this->max_tx) {
		this->max_tx = this->hist_tx[0];
	}

	/* Update the current stats. */
	this->rx_bytes = stats.rx_bytes;
	this->tx_bytes = stats.tx_bytes;
}

static void shift_get_max(guint64 *hist, gsize len, guint64 *max)
{
	if (len == 0) return;

	*max = hist[0];
	for (gssize i = len - 1; i > 0; i--) {
		hist[i] = hist[i - 1];
		if (hist[i] > *max) {
			*max = hist[i];
		}
	}
}
