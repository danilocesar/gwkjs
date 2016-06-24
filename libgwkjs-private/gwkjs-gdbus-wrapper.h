/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* Copyright 2011 Giovanni Campagna
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __GWKJS_UTIL_DBUS_H__
#define __GWKJS_UTIL_DBUS_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _GwkjsDBusImplementation        GwkjsDBusImplementation;
typedef struct _GwkjsDBusImplementationClass   GwkjsDBusImplementationClass;
typedef struct _GwkjsDBusImplementationPrivate GwkjsDBusImplementationPrivate;

#define GWKJS_TYPE_DBUS_IMPLEMENTATION              (gwkjs_dbus_implementation_get_type ())
#define GWKJS_DBUS_IMPLEMENTATION(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GWKJS_TYPE_DBUS_IMPLEMENTATION, GwkjsDBusImplementation))
#define GWKJS_DBUS_IMPLEMENTATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GWKJS_TYPE_DBUS_IMPLEMENTATION, GwkjsDBusImplementationClass))
#define GWKJS_IS_DBUS_IMPLEMENTATION(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GWKJS_TYPE_DBUS_IMPLEMENTATION))
#define GWKJS_IS_DBUS_IMPLEMENTATION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GWKJS_TYPE_DBUS_IMPLEMENTATION))
#define GWKJS_DBUS_IMPLEMENTATION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GWKJS_TYPE_DBUS_IMPLEMENTATION, GwkjsDBusImplementationClass))

struct _GwkjsDBusImplementation {
    GDBusInterfaceSkeleton parent;

    GwkjsDBusImplementationPrivate *priv;
};

struct _GwkjsDBusImplementationClass {
    GDBusInterfaceSkeletonClass parent_class;
};

GType                  gwkjs_dbus_implementation_get_type (void);

void                   gwkjs_dbus_implementation_emit_property_changed (GwkjsDBusImplementation *self, gchar *property, GVariant *newvalue);
void                   gwkjs_dbus_implementation_emit_signal           (GwkjsDBusImplementation *self, gchar *signal_name, GVariant *parameters);

G_END_DECLS

#endif  /* __GWKJS_UTIL_DBUS_H__ */
