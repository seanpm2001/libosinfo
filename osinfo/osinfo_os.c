/*
 * libosinfo:
 *
 * Copyright (C) 2009-2010 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 * Authors:
 *   Arjun Roy <arroy@redhat.com>
 *   Daniel P. Berrange <berrange@redhat.com>
 */

#include <osinfo/osinfo.h>

G_DEFINE_TYPE (OsinfoOs, osinfo_os, OSINFO_TYPE_PRODUCT);

#define OSINFO_OS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OSINFO_TYPE_OS, OsinfoOsPrivate))

/**
 * SECTION:osinfo_os
 * @short_description: An operating system
 * @see_also: #OsinfoOs, #OsinfoPlatform
 *
 * #OsinfoOs is an entity representing an operating system.
 * Operating systems have a list of supported devices. 
 * There are relationships amongst operating systems to
 * declare which are newest releases, which are clones
 * and which are derived from a common ancestry.
 */

struct _OsinfoOsPrivate
{
    // OS-Platform specific information
    // Key: gchar* (platform id)
    // Value: GList: Element Value: List of device_link structs
    GHashTable *platforms;

    // Value: List of device_link structs
    GList *deviceLinks;
};

struct _OsinfoOsDeviceLink {
    OsinfoDevice *dev;
    gchar *driver;
};


static void osinfo_os_finalize (GObject *object);

static void osinfo_device_link_free(gpointer data, gpointer opaque G_GNUC_UNUSED)
{
    g_object_unref(OSINFO_DEVICELINK(data));
}


static void
osinfo_os_finalize (GObject *object)
{
    OsinfoOs *os = OSINFO_OS (object);

    g_list_foreach(os->priv->deviceLinks, osinfo_device_link_free, NULL);
    g_list_free(os->priv->deviceLinks);
    g_hash_table_unref(os->priv->platforms);

    /* Chain up to the parent class */
    G_OBJECT_CLASS (osinfo_os_parent_class)->finalize (object);
}

/* Init functions */
static void
osinfo_os_class_init (OsinfoOsClass *klass)
{
    GObjectClass *g_klass = G_OBJECT_CLASS (klass);

    g_klass->finalize = osinfo_os_finalize;
    g_type_class_add_private (klass, sizeof (OsinfoOsPrivate));
}

static void
osinfo_os_platform_devices_free(gpointer opaque)
{
    GList *deviceLinks = opaque;

    g_list_foreach(deviceLinks, osinfo_device_link_free, NULL);
    g_list_free(deviceLinks);
}

static void
osinfo_os_init (OsinfoOs *os)
{
    OsinfoOsPrivate *priv;
    os->priv = priv = OSINFO_OS_GET_PRIVATE(os);

    os->priv->deviceLinks = NULL;
    os->priv->platforms = g_hash_table_new_full(g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  osinfo_os_platform_devices_free);
}


/**
 * osinfo_os_new:
 * @id: a unique identifier
 * 
 * Create a new operating system entity
 *
 * Returns: (transfer full): a new operating system entity
 */
OsinfoOs *osinfo_os_new(const gchar *id)
{
    return g_object_new(OSINFO_TYPE_OS,
			"id", id,
			NULL);
}


/**
 * osinfo_os_get_preferred_device:
 * @os: the operating system entity
 * @platform: (transfer none)(allow-none): an optional platform to restrict to
 * @filter: (transfer none)(allow-none): a device metadata filter
 *
 * Get the preferred device matching a given filter and platform
 *
 * Returns: (transfer none): a device, or NULL
 */
OsinfoDevice *osinfo_os_get_preferred_device(OsinfoOs *os, OsinfoPlatform *platform, OsinfoFilter *filter)
{
    g_return_val_if_fail(OSINFO_IS_OS(os), NULL);
    g_return_val_if_fail(!platform || OSINFO_IS_PLATFORM(platform), NULL);
    g_return_val_if_fail(OSINFO_IS_FILTER(filter), NULL);

    OsinfoDeviceLink *link = osinfo_os_get_preferred_device_link(os, platform, filter);
    if (link)
        return osinfo_devicelink_get_target(link);
    return NULL;
}


/**
 * osinfo_os_get_preferred_device_link:
 * @os: the operating system entity
 * @platform: (transfer none)(allow-none): an optional platform to restrict to
 * @filter: (transfer none)(allow-none): a device metadata filter
 *
 * Get the preferred device link matching a given filter and platform.
 * The filter matches against attributes on the device, not the link.
 *
 * Returns: (transfer none): a device, or NULL
 */
OsinfoDeviceLink *osinfo_os_get_preferred_device_link(OsinfoOs *os, OsinfoPlatform *platform, OsinfoFilter *filter)
{
    g_return_val_if_fail(OSINFO_IS_OS(os), NULL);
    g_return_val_if_fail(!platform || OSINFO_IS_PLATFORM(platform), NULL);
    g_return_val_if_fail(OSINFO_IS_FILTER(filter), NULL);
    // Check if device type info present for <os,platform>, else return NULL.

    GList *tmp;
    if (platform)
        tmp = g_hash_table_lookup(os->priv->platforms,
				  osinfo_entity_get_id(OSINFO_ENTITY(platform)));
    else
        tmp = os->priv->deviceLinks;

    // For each device in section list, apply filter. If filter passes, return device.
    while (tmp) {
        OsinfoDeviceLink *link = OSINFO_DEVICELINK(tmp->data);
        OsinfoDevice *dev = osinfo_devicelink_get_target(link);

        if (!filter || osinfo_filter_matches(filter, OSINFO_ENTITY(dev))) {
	    return link;
	}

	tmp = tmp->next;
    }

    // If no devices pass filter, return NULL.
    return NULL;
}


/**
 * osinfo_os_get_devices:
 * @os: an operating system
 * @platform: (allow-none)(transfer none): an optional platform to restrict to
 * @filter: (allow-none)(transfer none): an optional device property filter
 *
 * Get all devices matching a given filter and platform
 *
 * Returns: (transfer full): A list of devices
 */
OsinfoDeviceList *osinfo_os_get_devices(OsinfoOs *os, OsinfoPlatform *platform, OsinfoFilter *filter)
{
    g_return_val_if_fail(OSINFO_IS_OS(os), NULL);
    g_return_val_if_fail(!platform || OSINFO_IS_PLATFORM(platform), NULL);
    g_return_val_if_fail(!filter || OSINFO_IS_FILTER(filter), NULL);

    OsinfoDeviceList *newList = osinfo_devicelist_new();
    GList *tmp = NULL;

    if (platform)
        tmp = g_hash_table_lookup(os->priv->platforms,
				  osinfo_entity_get_id(OSINFO_ENTITY(platform)));
    else
        tmp = os->priv->deviceLinks;

    while (tmp) {
        OsinfoDeviceLink *link = OSINFO_DEVICELINK(tmp->data);
        OsinfoDevice *dev = osinfo_devicelink_get_target(link);
        if (!filter || osinfo_filter_matches(filter, OSINFO_ENTITY(dev)))
	    osinfo_list_add(OSINFO_LIST(newList), OSINFO_ENTITY(dev));

	tmp = tmp->next;
    }

    return newList;
}



/**
 * osinfo_os_get_device_links:
 * @os: an operating system
 * @platform: (allow-none)(transfer none): an optional platform to restrict to
 * @filter: (allow-none)(transfer none): an optional device property filter
 *
 * Get all devices matching a given filter and platform. The filter
 * matches against the links, not the devices.
 *
 * Returns: (transfer full): A list of device links
 */
OsinfoDeviceLinkList *osinfo_os_get_device_links(OsinfoOs *os, OsinfoPlatform *platform, OsinfoFilter *filter)
{
    g_return_val_if_fail(OSINFO_IS_OS(os), NULL);
    g_return_val_if_fail(!platform || OSINFO_IS_PLATFORM(platform), NULL);
    g_return_val_if_fail(!filter || OSINFO_IS_FILTER(filter), NULL);

    OsinfoDeviceLinkList *newList = osinfo_devicelinklist_new();
    GList *tmp = NULL;

    if (platform)
        tmp = g_hash_table_lookup(os->priv->platforms,
				  osinfo_entity_get_id(OSINFO_ENTITY(platform)));
    else
        tmp = os->priv->deviceLinks;

    while (tmp) {
        OsinfoDeviceLink *link = OSINFO_DEVICELINK(tmp->data);

        if (!filter || osinfo_filter_matches(filter, OSINFO_ENTITY(link)))
	    osinfo_list_add(OSINFO_LIST(newList), OSINFO_ENTITY(link));

	tmp = tmp->next;
    }

    return newList;
}


/**
 * osinfo_os_add_device:
 * @os: an operating system
 * @platform: (transfer none): an optional platform to associated with
 * @dev: (transfer none): the device to associate with
 *
 * Associated a device with an operating system, and optionally
 * a platform.  The returned #OsinfoDeviceLink
 * can be used to record extra metadata against the link
 *
 * Returns: (transfer none): the device association
 */
OsinfoDeviceLink *osinfo_os_add_device(OsinfoOs *os, OsinfoPlatform *platform, OsinfoDevice *dev)
{
    g_return_val_if_fail(OSINFO_IS_OS(os), NULL);
    g_return_val_if_fail(!platform || OSINFO_IS_PLATFORM(platform), NULL);
    g_return_val_if_fail(OSINFO_IS_DEVICE(dev), NULL);

    OsinfoDeviceLink *link = osinfo_devicelink_new(dev);

    if (platform) {
        GList *tmp = NULL;
        gpointer origKey, origValue;
	if (g_hash_table_lookup_extended(os->priv->platforms,
					 osinfo_entity_get_id(OSINFO_ENTITY(platform)),
					 &origKey, &origValue)) {
	    g_hash_table_steal(os->priv->platforms,
			       osinfo_entity_get_id(OSINFO_ENTITY(platform)));
	    g_free(origKey);
	    tmp = origValue;
	}
	tmp = g_list_append(tmp, link);
	g_hash_table_insert(os->priv->platforms,
			    g_strdup(osinfo_entity_get_id(OSINFO_ENTITY(platform))), tmp);
    } else {
        os->priv->deviceLinks = g_list_append(os->priv->deviceLinks, link);
    }

    return link;
}
/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 */
