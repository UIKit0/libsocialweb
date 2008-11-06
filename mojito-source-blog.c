#include "mojito-source-blog.h"
#include <rss-glib/rss-glib.h>
#include <libsoup/soup.h>

G_DEFINE_TYPE (MojitoSourceBlog, mojito_source_blog, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_BLOG, MojitoSourceBlogPrivate))

struct _MojitoSourceBlogPrivate {
  MojitoCore *core; /* TODO: move to MojitoSource */
  RssParser *parser;
  SoupURI *uri;
};

static GList *
mojito_source_blog_initialize (MojitoSourceClass *source_class, MojitoCore *core)
{
  /* TODO: replace with configuration file */
  
  MojitoSourceBlog *source;
  source = g_object_new (MOJITO_TYPE_SOURCE_BLOG, NULL);
  GET_PRIVATE (source)->core = core;
  GET_PRIVATE (source)->uri = soup_uri_new ("http://planet.gnome.org/atom.xml");
  
  return g_list_prepend (NULL, source);
}

static void
mojito_source_blog_update (MojitoSource *source)
{
  MojitoSourceBlog *blog = MOJITO_SOURCE_BLOG (source);
  
  g_debug ("Updating %s/%s", blog->priv->uri->host, blog->priv->uri->path);
}

static void
mojito_source_blog_dispose (GObject *object)
{
  MojitoSourceBlogPrivate *priv = MOJITO_SOURCE_BLOG (object)->priv;
  
  /* ->core is a weak reference so we don't unref it here */
  
  if (priv->parser) {
    g_object_unref (priv->parser);
    priv->parser = NULL;
  }
  
  G_OBJECT_CLASS (mojito_source_blog_parent_class)->dispose (object);
}

static void
mojito_source_blog_class_init (MojitoSourceBlogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoSourceClass *source_class = MOJITO_SOURCE_CLASS (klass);
  
  g_type_class_add_private (klass, sizeof (MojitoSourceBlogPrivate));
  
  object_class->dispose = mojito_source_blog_dispose;
  
  source_class->initialize = mojito_source_blog_initialize;  
  source_class->update = mojito_source_blog_update;
}

static void
mojito_source_blog_init (MojitoSourceBlog *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->parser = rss_parser_new ();
}

MojitoSource *
mojito_source_blog_new (MojitoCore *core)
{
  MojitoSourceBlog *source = g_object_new (MOJITO_TYPE_SOURCE_BLOG, NULL);
  GET_PRIVATE (source)->core = core;
  return MOJITO_SOURCE (source);
}