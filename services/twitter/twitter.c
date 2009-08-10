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

#include <config.h>
#include <time.h>
#include <string.h>
#include "twitter.h"
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-online.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <mojito-keyfob/mojito-keyfob.h>
#include <mojito-keystore/mojito-keystore.h>
#include <rest/oauth-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

G_DEFINE_TYPE (MojitoServiceTwitter, mojito_service_twitter, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_TWITTER, MojitoServiceTwitterPrivate))

struct _MojitoServiceTwitterPrivate {
  enum {
    OWN,
    FRIENDS
  } type;
  gboolean running;
  RestProxy *proxy;
  char *user_id;
  char *image_url;
};

RestXmlNode *
node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *root;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message ("Error from Twitter: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  g_object_unref (call);

  if (root == NULL) {
    g_message ("Error from Twitter: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  return root;
}

static char *
make_date (const char *s)
{
  struct tm tm;
  strptime (s, "%a %b %d %T %z %Y", &tm);
  return mojito_time_t_to_string (timegm (&tm));
}

static MojitoItem *
make_item (MojitoServiceTwitter *twitter, RestXmlNode *node)
{
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  MojitoItem *item;
  RestXmlNode *u_node, *n;
  const char *post_id, *user_id, *user_name;
  char *url;

  u_node = rest_xml_node_find (node, "user");

  user_id = rest_xml_node_find (u_node, "screen_name")->content;

  /* For friend feeds, ignore our own tweets */
  if (priv->type == FRIENDS && g_str_equal (user_id, priv->user_id))
    return NULL;

  item = mojito_item_new ();
  mojito_item_set_service (item, (MojitoService *)twitter);

  post_id = rest_xml_node_find (node, "id")->content;
  mojito_item_put (item, "authorid", user_id);

  url = g_strdup_printf ("http://twitter.com/%s/statuses/%s", user_id, post_id);
  mojito_item_put (item, "id", url);
  mojito_item_take (item, "url", url);

  user_name = rest_xml_node_find (node, "name")->content;
  mojito_item_put (item, "author", user_name);

  mojito_item_put (item, "content", rest_xml_node_find (node, "text")->content);

  const char *date;
  date = rest_xml_node_find (node, "created_at")->content;
  mojito_item_take (item, "date", make_date (date));

  n = rest_xml_node_find (u_node, "location");
  if (n && n->content)
    mojito_item_put (item, "location", n->content);

  n = rest_xml_node_find (u_node, "profile_image_url");
  if (n && n->content)
    mojito_item_take (item, "authoricon", mojito_web_download_image (n->content));

  return item;
}

static void
tweets_cb (RestProxyCall *call,
           GError        *error,
           GObject       *weak_object,
           gpointer       userdata)
{
  MojitoServiceTwitter *service = MOJITO_SERVICE_TWITTER (weak_object);
  RestXmlNode *root, *node;
  MojitoSet *set;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  root = node_from_call (call);
  if (!root)
    return;

  set = mojito_item_set_new ();

  for (node = rest_xml_node_find (root, "status"); node; node = node->next) {
    MojitoItem *item;
    /* TODO: skip the user's own tweets */

    item = make_item (service, node);
    if (item)
      mojito_set_add (set, (GObject *)item);
  }

  mojito_service_emit_refreshed ((MojitoService *)service, set);

  /* TODO cleanup */

  rest_xml_node_unref (root);
}

static void
get_status_updates (MojitoServiceTwitter *twitter)
{
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  RestProxyCall *call;

  if (!priv->user_id || !priv->running)
    return;

  call = rest_proxy_new_call (priv->proxy);
  switch (priv->type) {
  case OWN:
    rest_proxy_call_set_function (call, "statuses/user_timeline.xml");
    break;
  case FRIENDS:
    rest_proxy_call_set_function (call, "statuses/friends_timeline.xml");
    break;
  }
  rest_proxy_call_async (call, tweets_cb, (GObject*)twitter, NULL, NULL);
}

static void
verify_cb (RestProxyCall *call,
           GError        *error,
           GObject       *weak_object,
           gpointer       userdata)
{
  MojitoServiceTwitter *service = MOJITO_SERVICE_TWITTER (weak_object);
  RestXmlNode *node;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  node = node_from_call (call);
  if (!node)
    return;

  service->priv->user_id = g_strdup (rest_xml_node_find (node, "id")->content);
  service->priv->image_url = g_strdup (rest_xml_node_find (node, "profile_image_url")->content);

  rest_xml_node_unref (node);

  if (service->priv->running)
    get_status_updates (service);
}

static void
got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data)
{
  MojitoServiceTwitter *twitter = MOJITO_SERVICE_TWITTER (user_data);
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  RestProxyCall *call;

  if (authorised) {
    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "account/verify_credentials.xml");
    rest_proxy_call_async (call, verify_cb, (GObject*)twitter, NULL, NULL);
  } else {
    mojito_service_emit_refreshed ((MojitoService *)twitter, NULL);
  }
}

static void
start (MojitoService *service)
{
  MojitoServiceTwitter *twitter = (MojitoServiceTwitter*)service;

  twitter->priv->running = TRUE;
}

static void
refresh (MojitoService *service)
{
  MojitoServiceTwitter *twitter = (MojitoServiceTwitter*)service;
  MojitoServiceTwitterPrivate *priv = twitter->priv;

  if (!priv->running)
    return;

  if (priv->user_id) {
    get_status_updates (twitter);
  } else {
    mojito_keyfob_oauth ((OAuthProxy*)priv->proxy, got_tokens_cb, service);
  }
}

static const char **
get_static_caps (MojitoService *service)
{
  static const char * caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (MojitoService *service)
{
  MojitoServiceTwitterPrivate *priv = GET_PRIVATE (service);
  static const char * caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };
  static const char * no_caps[] = { NULL };

  if (priv->user_id)
    return caps;
  else
    return no_caps;
}

static void
_status_updated_cb (RestProxyCall *call,
                    GError        *error,
                    GObject       *weak_object,
                    gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);

  mojito_service_emit_status_updated (service, error == NULL);
}

static void
update_status (MojitoService *service, const char *msg)
{
  MojitoServiceTwitter *twitter = MOJITO_SERVICE_TWITTER (service);
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  RestProxyCall *call;

  if (!priv->user_id)
    return;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "statuses/update.xml");

  rest_proxy_call_add_params (call,
                              "status", msg,
                              NULL);

  rest_proxy_call_async (call, _status_updated_cb, (GObject *)service, NULL, NULL);
}

static void
avatar_downloaded_cb (const gchar *uri,
                       gchar       *local_path,
                       gpointer     userdata)
{
  MojitoService *service = MOJITO_SERVICE (userdata);

  mojito_service_emit_avatar_retrieved (service, local_path);
  g_free (local_path);
}

static void
request_avatar (MojitoService *service)
{
  MojitoServiceTwitterPrivate *priv = GET_PRIVATE (service);

  if (priv->image_url) {
    mojito_web_download_image_async (priv->image_url,
                                     avatar_downloaded_cb,
                                     service);
  }
}

static void
online_notify (gboolean online, gpointer user_data)
{
  MojitoServiceTwitter *service = (MojitoServiceTwitter *) user_data;

  if (online) {
    mojito_keyfob_oauth ((OAuthProxy*)service->priv->proxy, got_tokens_cb, service);
  } else {
    mojito_service_emit_capabilities_changed ((MojitoService *)service, NULL);
  }
}


static const char *
mojito_service_twitter_get_name (MojitoService *service)
{
  return "twitter";
}

static void
mojito_service_twitter_constructed (GObject *object)
{
  MojitoServiceTwitter *twitter = MOJITO_SERVICE_TWITTER (object);
  MojitoServiceTwitterPrivate *priv;
  const char *key = NULL, *secret = NULL;

  priv = twitter->priv = GET_PRIVATE (twitter);

  if (mojito_service_get_param ((MojitoService*)twitter, "own")) {
    priv->type = OWN;
  } else {
    priv->type = FRIENDS;
  }

  mojito_keystore_get_key_secret ("twitter", &key, &secret);

  priv->proxy = oauth_proxy_new (key, secret, "http://twitter.com/", FALSE);

  mojito_online_add_notify (online_notify, twitter);
  if (mojito_is_online ()) {
    online_notify (TRUE, twitter);
  }
}

static void
mojito_service_twitter_dispose (GObject *object)
{
  MojitoServiceTwitterPrivate *priv = MOJITO_SERVICE_TWITTER (object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (mojito_service_twitter_parent_class)->dispose (object);
}

static void
mojito_service_twitter_finalize (GObject *object)
{
  MojitoServiceTwitterPrivate *priv = MOJITO_SERVICE_TWITTER (object)->priv;

  g_free (priv->user_id);
  g_free (priv->image_url);

  G_OBJECT_CLASS (mojito_service_twitter_parent_class)->finalize (object);
}

static void
mojito_service_twitter_class_init (MojitoServiceTwitterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceTwitterPrivate));

  object_class->constructed = mojito_service_twitter_constructed;
  object_class->dispose = mojito_service_twitter_dispose;
  object_class->finalize = mojito_service_twitter_finalize;

  service_class->get_name = mojito_service_twitter_get_name;
  service_class->start = start;
  service_class->refresh = refresh;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->update_status = update_status;
  service_class->request_avatar = request_avatar;
}

static void
mojito_service_twitter_init (MojitoServiceTwitter *self)
{
}
