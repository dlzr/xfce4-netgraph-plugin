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

static gboolean device_is_up(const gchar *devname);
static guint64 read_u64_from_file(const gchar *filename);
static int strptrcmp(gconstpointer a, gconstpointer b);


// Allow variable declarations at the first use.
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"


GPtrArray *netdev_enumerate(void)
{
	g_autoptr(GDir) dir = g_dir_open("/sys/class/net", 0, NULL);
	if (!dir) return NULL;

	GPtrArray *files = g_ptr_array_new_with_free_func(g_free);

	const gchar *file = NULL;
	while ((file = g_dir_read_name(dir)) != NULL) {
		if (g_strcmp0(file, "lo") == 0) continue;

		if (!device_is_up(file)) continue;

		g_ptr_array_add(files, g_strdup(file));
	}

	g_ptr_array_sort(files, strptrcmp);

	return files;
}

static void netdev_os_init(NetworkDevice *this)
{
	this->rx_bytes_file = g_strdup_printf("/sys/class/net/%s/statistics/rx_bytes", this->name);
	this->tx_bytes_file = g_strdup_printf("/sys/class/net/%s/statistics/tx_bytes", this->name);
}

static void netdev_os_free(NetworkDevice *this)
{
	g_free(this->rx_bytes_file);
	g_free(this->tx_bytes_file);
}

static void netdev_os_read_stats(NetworkDevice *this, DeviceStats* stats)
{
	stats->is_up = device_is_up(this->name);
	stats->rx_bytes = read_u64_from_file(this->rx_bytes_file);
	stats->tx_bytes = read_u64_from_file(this->tx_bytes_file);
}

static gboolean device_is_up(const gchar *devname)
{
	g_autofree gchar *state_file = g_strdup_printf("/sys/class/net/%s/operstate", devname);

	g_autofree gchar *contents = NULL;
	gsize len;
	if (!g_file_get_contents(state_file, &contents, &len, NULL))
		return FALSE;

	return (g_strcmp0(contents, "up\n") == 0);
}

static guint64 read_u64_from_file(const gchar *filename)
{
	g_autofree gchar *contents = NULL;
	gsize len;
	if (!g_file_get_contents(filename, &contents, &len, NULL))
		return G_MAXUINT64;

	return g_ascii_strtoull(contents, NULL, 10);
}

static int strptrcmp(gconstpointer a, gconstpointer b)
{
	return g_strcmp0(*(const gchar **)a, *(const gchar **)b);
}
