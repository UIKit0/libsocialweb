/*
 * libsocialweb - social data store
 * Copyright (C) 2010 Intel Corporation.
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

#ifndef _SW_VIMEO_ITEM_VIEW
#define _SW_VIMEO_ITEM_VIEW

#include <glib-object.h>
#include <libsocialweb/sw-item-view.h>

G_BEGIN_DECLS

#define SW_TYPE_VIMEO_ITEM_VIEW sw_vimeo_item_view_get_type()

#define SW_VIMEO_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_VIMEO_ITEM_VIEW, SwVimeoItemView))

#define SW_VIMEO_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_VIMEO_ITEM_VIEW, SwVimeoItemViewClass))

#define SW_IS_VIMEO_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_VIMEO_ITEM_VIEW))

#define SW_IS_VIMEO_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_VIMEO_ITEM_VIEW))

#define SW_VIMEO_ITEM_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_VIMEO_ITEM_VIEW, SwVimeoItemViewClass))

typedef struct {
  SwItemView parent;
} SwVimeoItemView;

typedef struct {
  SwItemViewClass parent_class;
} SwVimeoItemViewClass;

GType sw_vimeo_item_view_get_type (void);

G_END_DECLS

#endif /* _SW_VIMEO_ITEM_VIEW */
