#include <sqlite3.h>
#include <libsoup/soup.h>
#include "mojito-core.h"
#include "mojito-utils.h"
#include "generic.h"

#include "mojito-source-blog.h"

G_DEFINE_TYPE (MojitoCore, mojito_core, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_CORE, MojitoCorePrivate))

struct _MojitoCorePrivate {
  /* Hash of interned source identifiers to GTypes */
  GHashTable *source_hash;
  GList *sources;
  sqlite3 *db;
  SoupSession *session;
};

static void
mojito_core_dispose (GObject *object)
{
  MojitoCorePrivate *priv = MOJITO_CORE (object)->priv;

  if (priv->session) {
    g_object_unref (priv->session);
    priv->session = NULL;
  }
  
  while (priv->sources) {
    g_object_unref (priv->sources->data);
    priv->sources = g_list_delete_link (priv->sources, priv->sources);
  }
  
  G_OBJECT_CLASS (mojito_core_parent_class)->dispose (object);
}

static void
mojito_core_finalize (GObject *object)
{
  MojitoCorePrivate *priv = MOJITO_CORE (object)->priv;

  sqlite3_close (priv->db);
  
  G_OBJECT_CLASS (mojito_core_parent_class)->finalize (object);
}

static void
mojito_core_class_init (MojitoCoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoCorePrivate));

  object_class->dispose = mojito_core_dispose;
  object_class->finalize = mojito_core_finalize;
}

static const char sql_get_sources[] = "SELECT rowid, type, url FROM sources;";

static void
populate_sources (MojitoCore *core)
{
  MojitoCorePrivate *priv = core->priv;
  MojitoSourceClass *source_class;

  source_class = g_type_class_ref (MOJITO_TYPE_SOURCE_BLOG);
  
  priv->sources = g_list_concat
    (priv->sources, mojito_source_initialize (source_class, core));

  g_printerr ("Got %d sources\n", g_list_length (priv->sources));
}

static void
mojito_core_init (MojitoCore *self)
{
  const char sql[] = 
    "CREATE TABLE IF NOT EXISTS items ("
    "'rowid' INTEGER PRIMARY KEY,"
    "'source' INTEGER NOT NULL,"
    "'link' TEXT NOT NULL,"
    "'date' INTEGER NOT NULL,"
    "'title' TEXT"
    ");";
  MojitoCorePrivate *priv = GET_PRIVATE (self);
  
  self->priv = priv;

  g_assert (sqlite3_threadsafe ());
  
  priv->source_hash = g_hash_table_new (g_direct_hash, g_direct_equal);

  priv->sources = NULL;

  /* Make async when we use DBus etc */
  priv->session = soup_session_sync_new ();

  /* TODO: move here onwards into a separate function so we can return errors */
  
  /* TODO: load these at runtime */
  g_hash_table_insert (priv->source_hash,
                       (gpointer)g_intern_static_string ("blog"),
                       GINT_TO_POINTER (MOJITO_TYPE_SOURCE_BLOG));
  
  if (sqlite3_open ("test.db", &priv->db) != SQLITE_OK) {
    g_error (sqlite3_errmsg (priv->db));
    return;
  }
  
  if (!mojito_create_tables (priv->db, sql)) {
    g_error (sqlite3_errmsg (priv->db));
    return;
  }

  populate_sources (self);
}

MojitoCore*
mojito_core_new (void)
{
  return g_object_new (MOJITO_TYPE_CORE, NULL);
}

sqlite3 *
mojito_core_get_db (MojitoCore *core)
{
  return core->priv->db;
}

SoupSession *
mojito_core_get_session (MojitoCore *core)
{
  return core->priv->session;
}

void
mojito_core_run (MojitoCore *core)
{
  g_list_foreach (core->priv->sources, (GFunc)mojito_source_update, NULL);
}