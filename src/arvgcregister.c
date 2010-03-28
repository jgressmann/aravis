/* Aravis - Digital camera library
 *
 * Copyright © 2009-2010 Emmanuel Pacaud
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Emmanuel Pacaud <emmanuel@gnome.org>
 */

#include <arvgcregister.h>
#include <arvgcinteger.h>
#include <arvgcport.h>
#include <arvtools.h>
#include <arvdebug.h>
#include <stdlib.h>
#include <string.h>

static GObjectClass *parent_class = NULL;

/* ArvGcNode implementation */

static void
arv_gc_register_add_element (ArvGcNode *node, const char *name, const char *content, const char **attributes)
{
	ArvGcRegister *gc_register = ARV_GC_REGISTER (node);

	if (strcmp (name, "Address") == 0) {
		gc_register->addresses = g_list_prepend (gc_register->addresses,
							 arv_new_g_value_int64 (g_ascii_strtoull (content, NULL, 0)));
	} else if (strcmp (name, "pAddress") == 0) {
		gc_register->addresses = g_list_prepend (gc_register->addresses, arv_new_g_value_string (content));
	} else if (strcmp (name, "Length") == 0) {
		arv_force_g_value_to_int64 (&gc_register->length, g_ascii_strtoull (content, NULL, 0));
	} else if (strcmp (name, "pLength") == 0) {
		arv_force_g_value_to_string (&gc_register->length, content);
	} else if (strcmp (name, "AccessMode") == 0) {
		if (g_strcmp0 (content, "RW") == 0)
			gc_register->access_mode = ARV_GC_ACCESS_MODE_RW;
		else if (g_strcmp0 (content, "RO") == 0)
			gc_register->access_mode = ARV_GC_ACCESS_MODE_RO;
		else if (g_strcmp0 (content, "WO") == 0)
			gc_register->access_mode = ARV_GC_ACCESS_MODE_WO;
	} else if (strcmp (name, "Cacheable") == 0) {
		if (g_strcmp0 (content, "NoCache") == 0)
			gc_register->cacheable = ARV_GC_CACHEABLE_NO_CACHE;
		else if (g_strcmp0 (content, "WriteAround") == 0)
			gc_register->cacheable = ARV_GC_CACHEABLE_WRITE_AROUND;
		else if (g_strcmp0 (content, "WriteThrough") == 0)
			gc_register->cacheable = ARV_GC_CACHEABLE_WRITE_TRHOUGH;
	} else if (strcmp (name, "pPort") == 0) {
		g_free (gc_register->port_name);
		gc_register->port_name = g_strdup (content);
	} else if (strcmp (name, "PollingTime") == 0) {
		gc_register->polling_time = g_ascii_strtoull (content, NULL, 0);
	} else if (strcmp (name, "Endianess") == 0) {
		if (g_strcmp0 (content, "BigEndian") == 0)
			gc_register->endianess = G_BIG_ENDIAN;
		else
			gc_register->endianess = G_LITTLE_ENDIAN;
	} else if (strcmp (name, "Sign") == 0) {
		if (g_strcmp0 (content, "Unsigned") == 0)
			gc_register->sign = ARV_GC_SIGN_UNSIGNED;
		else
			gc_register->sign = ARV_GC_SIGN_SIGNED;
	} else if (strcmp (name, "LSB") == 0) {
		gc_register->lsb = content != NULL ? atoi (content) : 0;
	} else if (strcmp (name, "MSB") == 0) {
		gc_register->msb = content != NULL ? atoi (content) : 0;
	} else if (strcmp (name, "Bit") == 0) {
		gc_register->msb = content != NULL ? atoi (content) : 0;
		gc_register->lsb = content != NULL ? atoi (content) : 0;
	} else
		ARV_GC_NODE_CLASS (parent_class)->add_element (node, name, content, attributes);
}

/* ArvGcRegister implementation */

static void
_update_cache_size (ArvGcRegister *gc_register, ArvGc *genicam)
{
	gint64 length;

	length = arv_gc_get_int64_from_value (genicam, &gc_register->length);
	if (length != gc_register->cache_size) {
		g_free (gc_register->cache);
		gc_register->cache = g_malloc (length);
		gc_register->cache_size = length;
	}

}

static void
_update_cache (ArvGcRegister *gc_register)
{
	ArvGc *genicam;
	ArvGcNode *port;

	genicam = arv_gc_node_get_genicam (ARV_GC_NODE (gc_register));
	g_return_if_fail (ARV_IS_GC (genicam));

	port = arv_gc_get_node (genicam, gc_register->port_name);
	if (!ARV_IS_GC_PORT (port))
		return;

	_update_cache_size (gc_register, genicam);

	arv_gc_port_read (ARV_GC_PORT (port),
			  gc_register->cache,
			  arv_gc_register_get_address (gc_register),
			  gc_register->cache_size);
}

void
arv_gc_register_get (ArvGcRegister *gc_register, void *buffer, guint64 length)
{
	g_return_if_fail (ARV_IS_GC_REGISTER (gc_register));

	_update_cache (gc_register);

	if (length > gc_register->cache_size) {
		memcpy (buffer, gc_register->cache, gc_register->cache_size);
		memset (buffer + gc_register->cache_size, 0, length - gc_register->cache_size);
	} else
		memcpy (buffer, gc_register->cache, length);

	arv_debug (ARV_DEBUG_LEVEL_STANDARD, "[GcRegister::get] 0x%Lx,%Ld",
		   arv_gc_register_get_address (gc_register), length);
}

void
arv_gc_register_set (ArvGcRegister *gc_register, void *buffer, guint64 length)
{
	ArvGc *genicam;
	ArvGcNode *port;

	g_return_if_fail (ARV_IS_GC_REGISTER (gc_register));
	genicam = arv_gc_node_get_genicam (ARV_GC_NODE (gc_register));
	g_return_if_fail (ARV_IS_GC (genicam));

	port = arv_gc_get_node (genicam, gc_register->port_name);
	if (!ARV_IS_GC_PORT (port))
		return;

	arv_gc_port_write (ARV_GC_PORT (port), buffer, arv_gc_register_get_address (gc_register), length);
}

guint64
arv_gc_register_get_address (ArvGcRegister *gc_register)
{
	ArvGc *genicam;
	GList *iter;
	guint64 value = 0;

	g_return_val_if_fail (ARV_IS_GC_REGISTER (gc_register), 0);
	genicam = arv_gc_node_get_genicam (ARV_GC_NODE (gc_register));
	g_return_val_if_fail (ARV_IS_GC (genicam), 0);

	for (iter = gc_register->addresses; iter != NULL; iter = iter->next)
		value += arv_gc_get_int64_from_value (genicam, iter->data);

	return value;
}

guint64
arv_gc_register_get_length (ArvGcRegister *gc_register)
{
	ArvGc *genicam;

	g_return_val_if_fail (ARV_IS_GC_REGISTER (gc_register), 0);
	genicam = arv_gc_node_get_genicam (ARV_GC_NODE (gc_register));
	g_return_val_if_fail (ARV_IS_GC (genicam), 0);

	return arv_gc_get_int64_from_value (genicam, &gc_register->length);
}

ArvGcNode *
arv_gc_register_new (void)
{
	ArvGcNode *node;

	node = g_object_new (ARV_TYPE_GC_REGISTER, NULL);

	return node;
}

static void
arv_gc_register_init (ArvGcRegister *gc_register)
{
	/* Set default to read only 32 bits little endian integer register */
	g_value_init (&gc_register->length, G_TYPE_INT64);
	g_value_set_int64 (&gc_register->length, 4);
	gc_register->access_mode = ARV_GC_ACCESS_MODE_RO;
	gc_register->cacheable = ARV_GC_CACHEABLE_NO_CACHE;
	gc_register->cache = g_malloc0(4);
	gc_register->cache_size = 4;
	gc_register->endianess = G_LITTLE_ENDIAN;
	gc_register->msb = 31;
	gc_register->lsb = 0;
}

static void
arv_gc_register_finalize (GObject *object)
{
	ArvGcRegister *gc_register = ARV_GC_REGISTER (object);
	GList *iter;

	for (iter = gc_register->addresses; iter != NULL; iter = iter->next)
		arv_free_g_value (iter->data);
	g_list_free (gc_register->addresses);
	g_value_unset (&gc_register->length);
	g_free (gc_register->port_name);
	g_free (gc_register->cache);

	parent_class->finalize (object);
}

static void
arv_gc_register_class_init (ArvGcRegisterClass *register_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (register_class);
	ArvGcNodeClass *node_class = ARV_GC_NODE_CLASS (register_class);

	parent_class = g_type_class_peek_parent (register_class);

	object_class->finalize = arv_gc_register_finalize;

	node_class->add_element = arv_gc_register_add_element;
}

/* ArvGcInteger interface implementation */

static gint64
arv_gc_register_get_integer_value (ArvGcInteger *gc_integer)
{
	ArvGcRegister *gc_register = ARV_GC_REGISTER (gc_integer);
	gint64 value;
	guint lsb;
	guint msb;

	_update_cache (gc_register);

	arv_copy_memory_with_endianess (&value, sizeof (value), G_BYTE_ORDER,
					gc_register->cache, gc_register->cache_size, gc_register->endianess);

	if (gc_register->endianess == G_BYTE_ORDER) {
		lsb = gc_register->lsb;
		msb = gc_register->msb;
	} else {
		msb = gc_register->lsb;
		lsb = gc_register->msb;
	}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	value = (value >> lsb);
	if (msb - lsb < 63)
		value = value & ((1 << (msb - lsb + 1)) - 1);
#else
	value = (value << lsb);
	if (lsb - msb < 63)
		value = value & ((1 >> (lsb - msb + 1)) - 1);
#endif

	return value;
}

static void
arv_gc_register_set_integer_value (ArvGcInteger *gc_integer, gint64 value)
{
	ArvGcRegister *gc_register = ARV_GC_REGISTER (gc_integer);

	arv_gc_register_set (gc_register, &value, sizeof (value));
}


static void
arv_gc_register_integer_interface_init (ArvGcIntegerInterface *interface)
{
	interface->get_value = arv_gc_register_get_integer_value;
	interface->set_value = arv_gc_register_set_integer_value;
}

G_DEFINE_TYPE_WITH_CODE (ArvGcRegister, arv_gc_register, ARV_TYPE_GC_NODE,
			 G_IMPLEMENT_INTERFACE (ARV_TYPE_GC_INTEGER, arv_gc_register_integer_interface_init))
