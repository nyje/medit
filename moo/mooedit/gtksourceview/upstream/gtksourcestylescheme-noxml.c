/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcestylescheme.c
 *
 *  Copyright (C) 2003 - Paolo Maggi <paolo.maggi@polito.it>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.

 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "gtksourceview-i18n.h"
#include "gtksourcestyleschememanager.h"
#include "gtksourceview.h"
#include "gtksourcelanguage-private.h"
#include "gtksourcestyle-private.h"
#include <string.h>

#define STYLE_TEXT		"text"
#define STYLE_SELECTED		"text-selected"
#define STYLE_BRACKET_MATCH	"bracket-match"
#define STYLE_BRACKET_MISMATCH	"bracket-mismatch"
#define STYLE_CURSOR		"cursor"
#define STYLE_SECONDARY_CURSOR	"secondary-cursor"
#define STYLE_CURRENT_LINE	"current-line"
#define STYLE_LINE_NUMBERS	"line-numbers"

enum {
	PROP_0,
	PROP_ID,
	PROP_NAME
};

struct _GtkSourceStyleSchemePrivate
{
	gchar *id;
	gchar *name;
	gchar *author;
	gchar *description;
	GtkSourceStyleScheme *parent;
	gchar *parent_id;
	GHashTable *styles;
};

G_DEFINE_TYPE (GtkSourceStyleScheme, gtk_source_style_scheme, G_TYPE_OBJECT)

static void
gtk_source_style_scheme_finalize (GObject *object)
{
	GtkSourceStyleScheme *scheme = GTK_SOURCE_STYLE_SCHEME (object);

	g_hash_table_destroy (scheme->priv->styles);
	g_free (scheme->priv->author);
	g_free (scheme->priv->description);
	g_free (scheme->priv->id);
	g_free (scheme->priv->name);
	g_free (scheme->priv->parent_id);

	if (scheme->priv->parent != NULL)
		g_object_unref (scheme->priv->parent);

	G_OBJECT_CLASS (gtk_source_style_scheme_parent_class)->finalize (object);
}

static void
gtk_source_style_scheme_set_property (GObject 	   *object,
				      guint 	    prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
	char *tmp;
	GtkSourceStyleScheme *scheme = GTK_SOURCE_STYLE_SCHEME (object);

	switch (prop_id)
	{
	    case PROP_ID:
		tmp = scheme->priv->id;
		scheme->priv->id = g_strdup (g_value_get_string (value));
		g_free (tmp);
		break;

	    case PROP_NAME:
		tmp = scheme->priv->name;
		scheme->priv->name = g_strdup (g_value_get_string (value));
		g_free (tmp);
		g_object_notify (object, "name");
		break;

	    default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_style_scheme_get_property (GObject 	 *object,
				      guint 	  prop_id,
				      GValue 	 *value,
				      GParamSpec *pspec)
{
	GtkSourceStyleScheme *scheme = GTK_SOURCE_STYLE_SCHEME (object);

	switch (prop_id)
	{
	    case PROP_ID:
		    g_value_set_string (value, scheme->priv->id);
		    break;

	    case PROP_NAME:
		    g_value_set_string (value, scheme->priv->name);
		    break;

	    default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_style_scheme_class_init (GtkSourceStyleSchemeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_style_scheme_finalize;
	object_class->set_property = gtk_source_style_scheme_set_property;
	object_class->get_property = gtk_source_style_scheme_get_property;

	/**
	 * GtkSourceStyleScheme:id:
	 *
	 * Style scheme id, a unique string used to identify the style scheme
	 * in #GtkSourceStyleSchemeManager.
	 *
	 * Since: 2.0
	 */
	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
						 	      "Style scheme id",
							      "Style scheme id",
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GtkSourceStyleScheme:name:
	 *
	 * Style scheme name, a translatable string to present to user.
	 *
	 * Since: 2.0
	 */
	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
						 	      "Style scheme name",
							      "Style scheme name",
							      NULL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GtkSourceStyleSchemePrivate));
}

static void
gtk_source_style_scheme_init (GtkSourceStyleScheme *scheme)
{
	scheme->priv = G_TYPE_INSTANCE_GET_PRIVATE (scheme, GTK_TYPE_SOURCE_STYLE_SCHEME,
						    GtkSourceStyleSchemePrivate);
	scheme->priv->styles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
						      (GDestroyNotify) g_object_unref);
}

/**
 * gtk_source_style_scheme_get_id:
 * @scheme: a #GtkSourceStyleScheme.
 *
 * Returns: @scheme id.
 *
 * Since: 2.0
 */
const gchar *
gtk_source_style_scheme_get_id (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	g_return_val_if_fail (scheme->priv->id != NULL, "");
	return scheme->priv->id;
}

/**
 * gtk_source_style_scheme_get_name:
 * @scheme: a #GtkSourceStyleScheme.
 *
 * Returns: @scheme name.
 *
 * Since: 2.0
 */
const gchar *
gtk_source_style_scheme_get_name (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	g_return_val_if_fail (scheme->priv->name != NULL, "");
	return scheme->priv->name;
}

/**
 * _gtk_source_style_scheme_new:
 * @id: scheme id.
 * @name: scheme name.
 *
 * Returns: new empty #GtkSourceStyleScheme.
 *
 * Since: 2.0
 */
GtkSourceStyleScheme *
_gtk_source_style_scheme_new (const gchar *id,
			      const gchar *name)
{
	GtkSourceStyleScheme *scheme;

	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	scheme = g_object_new (GTK_TYPE_SOURCE_STYLE_SCHEME,
			       "id", id, "name", name, NULL);

	return scheme;
}

/**
 * gtk_source_style_scheme_get_style:
 * @scheme: a #GtkSourceStyleScheme.
 * @style_name: style name to find.
 *
 * Returns: style which corresponds to @style_name in the @scheme,
 * or %NULL when no style with this name found. Call g_object_unref()
 * when you are done with it.
 *
 * Since: 2.0
 */
GtkSourceStyle *
gtk_source_style_scheme_get_style (GtkSourceStyleScheme *scheme,
				   const gchar          *style_name)
{
	GtkSourceStyle *style = NULL;

	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	g_return_val_if_fail (style_name != NULL, NULL);

	style = g_hash_table_lookup (scheme->priv->styles, style_name);

	if (style != NULL)
		return gtk_source_style_copy (style);

	if (scheme->priv->parent != NULL)
		return gtk_source_style_scheme_get_style (scheme->priv->parent,
							  style_name);
	else
		return NULL;
}

/**
 * gtk_source_style_scheme_set_style:
 * @scheme: a #GtkSourceStyleScheme.
 * @name: style name.
 * @style: style to set or %NULL.
 *
 * Since: 2.0
 */
void
gtk_source_style_scheme_set_style (GtkSourceStyleScheme *scheme,
				   const gchar          *name,
				   const GtkSourceStyle *style)
{
	g_return_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme));
	g_return_if_fail (name != NULL);

	if (style != NULL)
		g_hash_table_insert (scheme->priv->styles, g_strdup (name),
				     gtk_source_style_copy (style));
	else
		g_hash_table_remove (scheme->priv->styles, name);
}

/**
 * gtk_source_style_scheme_get_matching_brackets_style:
 * @scheme: a #GtkSourceStyleScheme.
 *
 * Returns: style which corresponds to "bracket-match" name, to use
 * in an editor. Call g_object_unref() when you are done with it.
 *
 * Since: 2.0
 */
GtkSourceStyle *
gtk_source_style_scheme_get_matching_brackets_style (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	return gtk_source_style_scheme_get_style (scheme, STYLE_BRACKET_MATCH);
}

static gboolean
get_color (GtkSourceStyle *style,
	   gboolean        foreground,
	   GdkColor       *dest)
{
	const gchar *color;
	guint mask;

	if (style == NULL)
		return FALSE;

	if (foreground)
	{
		color = style->foreground;
		mask = GTK_SOURCE_STYLE_USE_FOREGROUND;
	}
	else
	{
		color = style->background;
		mask = GTK_SOURCE_STYLE_USE_BACKGROUND;
	}

	if (style->mask & mask)
	{
		if (color == NULL || !gdk_color_parse (color, dest))
		{
			g_warning ("%s: invalid color '%s'", G_STRLOC,
				   color != NULL ? color : "(null)");
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

/**
 * gtk_source_style_scheme_get_current_line_color:
 * @scheme: a #GtkSourceStyleScheme.
 * @color: a #GdkColor structure to fill.
 *
 * Returns: %TRUE if @scheme has style for current line set, or %FALSE
 * otherwise.
 *
 * Since: 2.0
 */
gboolean
gtk_source_style_scheme_get_current_line_color (GtkSourceStyleScheme *scheme,
						GdkColor             *color)
{
	GtkSourceStyle *style;
	gboolean ret = FALSE;

	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), FALSE);
	g_return_val_if_fail (color != NULL, FALSE);

	style = gtk_source_style_scheme_get_style (scheme, STYLE_CURRENT_LINE);

	ret = get_color (style, FALSE, color);

	if (style != NULL)
		g_object_unref (style);

	return ret;
}

static void
set_text_style (GtkWidget      *widget,
		GtkSourceStyle *style,
		GtkStateType    state)
{
	GdkColor color;
	GdkColor *color_ptr;

	if (get_color (style, FALSE, &color))
		color_ptr = &color;
	else
		color_ptr = NULL;

	gtk_widget_modify_base (widget, state, color_ptr);

	if (get_color (style, TRUE, &color))
		color_ptr = &color;
	else
		color_ptr = NULL;

	gtk_widget_modify_text (widget, state, color_ptr);
}

static void
set_line_numbers_style (GtkWidget      *widget,
			GtkSourceStyle *style)
{
	gint i;
	GdkColor *fg_ptr = NULL;
	GdkColor *bg_ptr = NULL;
	GdkColor fg;
	GdkColor bg;

	if (get_color (style, TRUE, &fg))
		fg_ptr = &fg;
	if (get_color (style, FALSE, &bg))
		bg_ptr = &bg;

	for (i = 0; i < 5; ++i)
	{
		gtk_widget_modify_fg (widget, i, fg_ptr);
		gtk_widget_modify_bg (widget, i, bg_ptr);
	}
}

static void
set_cursor_colors (GtkWidget      *widget,
		   const GdkColor *primary,
		   const GdkColor *secondary)
{
#if !GTK_CHECK_VERSION(2,11,3)
	char *rc_string;
	char *widget_name;

	widget_name = g_strdup_printf ("gtk-source-view-%p", (gpointer) widget);

	rc_string = g_strdup_printf (
		"style \"%p\"\n"
		"{\n"
		"   GtkWidget::cursor-color = \"#%02x%02x%02x\"\n"
		"   GtkWidget::secondary-cursor-color = \"#%02x%02x%02x\"\n"
		"}\n"
		"widget \"*.%s\" style \"%p\"\n",
		(gpointer) widget,
		primary->red >> 8, primary->green >> 8, primary->blue >> 8,
		secondary->red >> 8, secondary->green >> 8, secondary->blue >> 8,
		widget_name,
		(gpointer) widget
	);

	gtk_rc_parse_string (rc_string);

	if (strcmp (widget_name, gtk_widget_get_name (widget)) != 0)
		gtk_widget_set_name (widget, widget_name);

	g_free (rc_string);
	g_free (widget_name);
#else
	gtk_widget_modify_cursor (widget, primary, secondary);
#endif
}

static void
unset_cursor_colors (GtkWidget *widget)
{
#if !GTK_CHECK_VERSION(2,11,3)
	set_cursor_colors (widget,
			   &widget->style->text[GTK_STATE_NORMAL],
			   &widget->style->text_aa[GTK_STATE_NORMAL]);
#else
	gtk_widget_modify_cursor (widget, NULL, NULL);
#endif
}

static void
update_cursor_colors (GtkWidget      *widget,
		      GtkSourceStyle *style_primary,
		      GtkSourceStyle *style_secondary)
{
	GdkColor primary_color, secondary_color;
	GdkColor *primary = NULL, *secondary = NULL;

	if (get_color (style_primary, TRUE, &primary_color))
		primary = &primary_color;

	if (get_color (style_secondary, TRUE, &secondary_color))
		secondary = &secondary_color;

	if (primary != NULL && secondary == NULL)
	{
		secondary_color = widget->style->base[GTK_STATE_NORMAL];
		secondary_color.red = ((gint) secondary_color.red + primary->red) / 2;
		secondary_color.green = ((gint) secondary_color.green + primary->green) / 2;
		secondary_color.blue = ((gint) secondary_color.blue + primary->blue) / 2;
		secondary = &secondary_color;
	}

	if (primary != NULL)
		set_cursor_colors (widget, primary, secondary);
	else
		unset_cursor_colors (widget);
}

/**
 * _gtk_source_style_scheme_apply:
 * @scheme: a #GtkSourceStyleScheme or NULL.
 * @widget: a #GtkWidget to apply styles to.
 *
 * Sets text colors from @scheme in the @widget.
 *
 * Since: 2.0
 */
void
_gtk_source_style_scheme_apply (GtkSourceStyleScheme *scheme,
				GtkWidget            *widget)
{
	g_return_if_fail (!scheme || GTK_IS_SOURCE_STYLE_SCHEME (scheme));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (scheme != NULL)
	{
		GtkSourceStyle *style, *style2;

		gtk_widget_ensure_style (widget);

		style = gtk_source_style_scheme_get_style (scheme, STYLE_TEXT);
		set_text_style (widget, style, GTK_STATE_NORMAL);
		set_text_style (widget, style, GTK_STATE_ACTIVE);
		set_text_style (widget, style, GTK_STATE_PRELIGHT);
		set_text_style (widget, style, GTK_STATE_INSENSITIVE);
		if (style != NULL)
			g_object_unref (style);

		style = gtk_source_style_scheme_get_style (scheme, STYLE_SELECTED);
		set_text_style (widget, style, GTK_STATE_SELECTED);
		if (style != NULL)
			g_object_unref (style);

		style = gtk_source_style_scheme_get_style (scheme, STYLE_LINE_NUMBERS);
		set_line_numbers_style (widget, style);
		if (style != NULL)
			g_object_unref (style);

		style = gtk_source_style_scheme_get_style (scheme, STYLE_CURSOR);
		style2 = gtk_source_style_scheme_get_style (scheme, STYLE_SECONDARY_CURSOR);
		update_cursor_colors (widget, style, style2);
		if (style2 != NULL)
			g_object_unref (style2);
		if (style != NULL)
			g_object_unref (style);
	}
	else
	{
		set_text_style (widget, NULL, GTK_STATE_NORMAL);
		set_text_style (widget, NULL, GTK_STATE_ACTIVE);
		set_text_style (widget, NULL, GTK_STATE_PRELIGHT);
		set_text_style (widget, NULL, GTK_STATE_INSENSITIVE);
		set_text_style (widget, NULL, GTK_STATE_SELECTED);
		set_line_numbers_style (widget, NULL);
		unset_cursor_colors (widget);
	}
}

/* --- PARSER ---------------------------------------------------------------- */

/**
 * _gtk_source_style_scheme_new_from_file:
 * @filename: file to parse.
 *
 * Returns: new #GtkSourceStyleScheme created from file, or
 * %NULL on error.
 *
 * Since: 2.0
 */
GtkSourceStyleScheme *
_gtk_source_style_scheme_new_from_file (G_GNUC_UNUSED const gchar *filename)
{
	return NULL;
}

/**
 * _gtk_source_style_scheme_get_parent_id:
 * @scheme: a #GtkSourceStyleScheme.
 *
 * Returns: parent style scheme id or %NULL.
 *
 * Since: 2.0
 */
const gchar *
_gtk_source_style_scheme_get_parent_id (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	return scheme->priv->parent_id;
}

/**
 * _gtk_source_style_scheme_set_parent:
 * @scheme: a #GtkSourceStyleScheme.
 * @parent_scheme: parent #GtkSourceStyleScheme for @scheme.
 *
 * Sets @parent_scheme as parent scheme for @scheme, @scheme will
 * look for styles in @parent_scheme if it doesn't have style set
 * for given name.
 *
 * Since: 2.0
 */
void
_gtk_source_style_scheme_set_parent (GtkSourceStyleScheme *scheme,
				     GtkSourceStyleScheme *parent_scheme)
{
	g_return_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme));
	g_return_if_fail (!parent_scheme || GTK_IS_SOURCE_STYLE_SCHEME (parent_scheme));

	if (scheme->priv->parent != NULL)
		g_object_unref (scheme->priv->parent);
	if (parent_scheme)
		g_object_ref (parent_scheme);
	scheme->priv->parent = parent_scheme;
}

/**
 * _gtk_source_style_scheme_get_default:
 *
 * Returns: default style scheme to be used when user didn't set
 * style scheme explicitly.
 *
 * Since: 2.0
 */
GtkSourceStyleScheme *
_gtk_source_style_scheme_get_default (void)
{
	GtkSourceStyleSchemeManager *manager;

	manager = gtk_source_style_scheme_manager_get_default ();

	return gtk_source_style_scheme_manager_get_scheme (manager, "kate");
}
