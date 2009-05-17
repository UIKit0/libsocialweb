/*
 * Mojito - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <mojito-client/mojito-client.h>

static void
get_caps_cb (MojitoClientService *service,
             guint32 caps, const GError *error, gpointer user_data)
{
  g_print ("[%p] Caps %u\n", service, caps);
}

static void
on_caps_changed (MojitoClientService *service, guint32 caps, gpointer user_data)
{
  g_print ("[%p] New Caps %u\n", service, caps);
}

static void
client_get_services_cb (MojitoClient *client,
                       const GList        *services,
                       gpointer      userdata)
{
  const GList *l;
  MojitoClientService *service;

  for (l = services; l; l = l->next)
  {
    g_print ("Told about service: %s\n", (char*)l->data);
    service = mojito_client_get_service (client, (char*)l->data);
    g_signal_connect (service, "capabilities-changed", G_CALLBACK (on_caps_changed), NULL);
    mojito_client_service_get_capabilities (service, get_caps_cb, NULL);
  }
}

int
main (int    argc,
      char **argv)
{
  MojitoClient *client;
  GMainLoop *loop;

  g_type_init ();

  client = mojito_client_new ();
  mojito_client_get_services (client, client_get_services_cb, NULL);

  loop = g_main_loop_new (NULL, FALSE);

  g_main_loop_run (loop);
}
