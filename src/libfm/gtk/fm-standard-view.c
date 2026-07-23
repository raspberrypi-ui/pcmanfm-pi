/*
 *      fm-standard-view.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *      Copyright 2012-2015 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
 *      Copyright 2013 Mamoru TASAKA <mtasaka@fedoraproject.org>
 *      Copyright 2024 Ingo Brückl
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/**
 * SECTION:fm-standard-view
 * @short_description: A folder view widget based on libexo.
 * @title: FmStandardView
 *
 * @include: libfm/fm-gtk.h
 *
 * The #FmStandardView represents view of content of a folder with
 * support of drag & drop and other file/directory operations.
 */


#include <stdlib.h>
#include <glib/gi18n-lib.h>

#include "fm.h"
#include "fm-standard-view.h"
#include "fm-gtk-marshal.h"
#include "fm-cell-renderer-text.h"
#include "fm-cell-renderer-pixbuf.h"
#include "fm-gtk-utils.h"


#include "fm-dnd-src.h"
#include "fm-dnd-dest.h"
#include "fm-dnd-auto-scroll.h"

struct _FmStandardView
{
    GtkScrolledWindow parent;

    FmStandardViewMode mode;
    GtkSelectionMode sel_mode;

    gboolean show_hidden;
    gboolean show_thumbs;

    GtkWidget* view; /* either GtkIconView or GtkTreeView */
    FmFolderModel* model; /* FmStandardView doesn't use abstract GtkTreeModel! */
    FmCellRendererPixbuf* renderer_pixbuf;
    FmCellRendererText* renderer_text;
    guint icon_size_changed_handler;
    guint show_full_names_handler;

    /* list view only: the Name column, used to gate right-click
       select-and-activate and the rename-trigger gesture below (GtkTreeView
       has no built-in concept of a single "activable" column) */
    GtkTreeViewColumn* activable_column;

    /* click-selected-item-to-rename gesture, icon view and list view */
    GtkTreePath* pending_rename_path;
    guint pending_rename_timer;

    /* interactive (type-ahead) search, icon view only */
    GtkWidget* search_window;
    GtkWidget* search_entry;
    gulong search_entry_changed_id;
    guint search_timeout_id;
    gboolean search_imcontext_changed;

    FmDndSrc* dnd_src; /* dnd source manager */
    FmDndDest* dnd_dest; /* dnd dest manager */

    /* for very large folder update */
    guint sel_changed_idle;
    gboolean sel_changed_pending;

    FmFileInfoList* cached_selected_files;
    FmPathList* cached_selected_file_paths;

    /* callbacks to creator */
    FmFolderViewUpdatePopup update_popup;
    FmLaunchFolderFunc open_folders;

    /* internal switches */
    void (*set_single_click)(GtkWidget* view, gboolean single_click);
    void (*set_auto_selection_delay)(GtkWidget* view, gint auto_selection_delay);
    void (*set_middle_click)(GtkWidget* view, gboolean middle_click);
    GtkTreePath* (*get_drop_path)(FmStandardView* fv, gint x, gint y);
    void (*set_drag_dest)(FmStandardView* fv, GtkTreePath* tp);
    void (*select_all)(GtkWidget* view);
    void (*unselect_all)(GtkWidget* view);
    void (*select_invert)(FmFolderModel* model, GtkWidget* view);
    void (*select_path)(FmFolderModel* model, GtkWidget* view, GtkTreeIter* it);

    /* for columns width handling */
    gint updated_col;
    gboolean name_updated;

    GtkGesture *igesture;
    GtkGesture *lgesture;
};

struct _FmStandardViewClass
{
    GtkScrolledWindowClass parent_class;

    /* signal handlers */
    /* void (*column_widths_changed)(); */
};

// for gestures
static GtkTreePath *gpath = NULL;
gboolean longpress = FALSE;

static void fm_standard_view_dispose(GObject *object);

static void fm_standard_view_view_init(FmFolderViewInterface* iface);

G_DEFINE_TYPE_WITH_CODE(FmStandardView, fm_standard_view, GTK_TYPE_SCROLLED_WINDOW,
                        G_IMPLEMENT_INTERFACE(FM_TYPE_FOLDER_VIEW, fm_standard_view_view_init))

static GList* fm_standard_view_get_selected_tree_paths(FmStandardView* fv);

static gboolean on_standard_view_focus_in(GtkWidget* widget, GdkEventFocus* evt);

static gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmStandardView* fv);
static void on_sel_changed(GObject* obj, FmStandardView* fv);
static void fm_standard_view_cancel_pending_rename(FmStandardView* fv);
static gboolean on_icon_view_button_release(GtkWidget* view, GdkEventButton* evt, FmStandardView* fv);
static gboolean on_tree_view_button_release(GtkWidget* view, GdkEventButton* evt, FmStandardView* fv);
static gboolean on_icon_view_key_press(GtkWidget* view, GdkEventKey* evt, FmStandardView* fv);
static void fm_standard_view_search_dialog_hide(FmStandardView* fv);

static void on_dnd_src_data_get(FmDndSrc* ds, FmStandardView* fv);

static void on_single_click_changed(FmConfig* cfg, FmStandardView* fv);
static void on_auto_selection_delay_changed(FmConfig* cfg, FmStandardView* fv);
static void on_middle_click_changed(FmConfig* cfg, FmStandardView* fv);
static void on_big_icon_size_changed(FmConfig* cfg, FmStandardView* fv);
static void on_small_icon_size_changed(FmConfig* cfg, FmStandardView* fv);
static void on_thumbnail_size_changed(FmConfig* cfg, FmStandardView* fv);

static FmFolderViewColumnInfo* _sv_column_info_new(FmFolderModelCol col_id)
{
    FmFolderViewColumnInfo* info = g_slice_new0(FmFolderViewColumnInfo);
    info->col_id = col_id;
    return info;
}

static void _sv_column_info_free(gpointer info)
{
    g_slice_free(FmFolderViewColumnInfo, info);
}

/* override for GtkScrolledWindow bug - it ignores modifiers totally */
static gboolean on_standard_view_scroll_event(GtkWidget* w, GdkEventScroll* evt)
{
    if ((evt->state & gtk_accelerator_get_default_mod_mask()) == 0 &&
        GTK_WIDGET_CLASS(fm_standard_view_parent_class)->scroll_event)
        return GTK_WIDGET_CLASS(fm_standard_view_parent_class)->scroll_event(w, evt);
    return FALSE;
}

static void fm_standard_view_class_init(FmStandardViewClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass *widget_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_standard_view_dispose;
    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->focus_in_event = on_standard_view_focus_in;
    widget_class->scroll_event = on_standard_view_scroll_event;

    fm_standard_view_parent_class = (GtkScrolledWindowClass*)g_type_class_peek(GTK_TYPE_SCROLLED_WINDOW);
}

static gboolean on_standard_view_focus_in(GtkWidget* widget, GdkEventFocus* evt)
{
    FmStandardView* fv = FM_STANDARD_VIEW(widget);
    if( fv->view )
    {
        gtk_widget_grab_focus(fv->view);
        return TRUE;
    }
    return FALSE;
}

static void on_single_click_changed(FmConfig* cfg, FmStandardView* fv)
{
    if(fv->set_single_click)
        fv->set_single_click(fv->view, cfg->single_click);
}

static void on_auto_selection_delay_changed(FmConfig* cfg, FmStandardView* fv)
{
    if(fv->set_auto_selection_delay)
        fv->set_auto_selection_delay(fv->view, cfg->auto_selection_delay);
}

static void on_middle_click_changed(FmConfig* cfg, FmStandardView* fv)
{
    if(fv->set_middle_click)
        fv->set_middle_click(fv->view, cfg->middle_click);
}

static void on_icon_view_item_activated(GtkIconView* iv, GtkTreePath* path, FmStandardView* fv)
{
    fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), path, FM_FV_ACTIVATED, 0, -1, -1);
}

static void on_tree_view_row_activated(GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* col, FmStandardView* fv)
{
    fm_standard_view_cancel_pending_rename(fv);
    fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), path, FM_FV_ACTIVATED, 0, -1, -1);
}

static gboolean on_draw (GtkWidget *self, cairo_t *cr, gpointer data)
{
    FmStandardView *fv = (FmStandardView *) self;
    int scale = gtk_widget_get_scale_factor (self);
    if (fm_cell_renderer_pixbuf_get_scale ((FmCellRendererPixbuf *) fv->renderer_pixbuf) != scale)
    {
        fm_folder_model_set_icon_size (fv->model, fm_folder_model_get_icon_size (fv->model));
        fm_cell_renderer_pixbuf_set_scale ((FmCellRendererPixbuf *) fv->renderer_pixbuf, scale);
    }
    return FALSE;
}

static void fm_standard_view_init(FmStandardView *self)
{
    gtk_scrolled_window_set_hadjustment((GtkScrolledWindow*)self, NULL);
    gtk_scrolled_window_set_vadjustment((GtkScrolledWindow*)self, NULL);
    gtk_scrolled_window_set_policy((GtkScrolledWindow*)self, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type((GtkScrolledWindow*)self, GTK_SHADOW_IN);

    /* config change notifications */
    g_signal_connect(fm_config, "changed::single_click", G_CALLBACK(on_single_click_changed), self);
    g_signal_connect(fm_config, "changed::auto_selection_delay", G_CALLBACK(on_auto_selection_delay_changed), self);
    g_signal_connect(fm_config, "changed::middle_click", G_CALLBACK(on_middle_click_changed), self);

    /* dnd support */
    self->dnd_src = fm_dnd_src_new(NULL);
    g_signal_connect(self->dnd_src, "data-get", G_CALLBACK(on_dnd_src_data_get), self);

    self->dnd_dest = fm_dnd_dest_new_with_handlers(NULL);

    self->mode = -1;
    self->updated_col = -1;

    g_signal_connect (self, "draw", G_CALLBACK (on_draw), NULL);
}

/**
 * fm_folder_view_new
 * @mode: initial mode of view
 *
 * Returns: a new #FmFolderView widget.
 *
 * Since: 0.1.0
 * Deprecated: 1.0.1: Use fm_standard_view_new() instead.
 */
FmFolderView* fm_folder_view_new(guint mode)
{
    return (FmFolderView*)fm_standard_view_new(mode, NULL, NULL);
}

/**
 * fm_standard_view_new
 * @mode: initial mode of view
 * @update_popup: (allow-none): callback to update context menu for files
 * @open_folders: (allow-none): callback to open folder on activation
 *
 * Creates new folder view.
 *
 * Returns: a new #FmStandardView widget.
 *
 * Since: 1.0.1
 */
FmStandardView* fm_standard_view_new(FmStandardViewMode mode,
                                     FmFolderViewUpdatePopup update_popup,
                                     FmLaunchFolderFunc open_folders)
{
    FmStandardView* fv = (FmStandardView*)g_object_new(FM_STANDARD_VIEW_TYPE, NULL);
    AtkObject *obj = gtk_widget_get_accessible(GTK_WIDGET(fv));

    fm_standard_view_set_mode(fv, mode);
    fv->update_popup = update_popup;
    fv->open_folders = open_folders;
    atk_object_set_description(obj, _("View of folder contents"));
    return fv;
}

static void _reset_columns_widths(GtkTreeView* view)
{
    GList* cols = gtk_tree_view_get_columns(view);
    GList* l;

    for(l = cols; l; l = l->next)
    {
        FmFolderViewColumnInfo* info = g_object_get_qdata(l->data, fm_qdata_id);
        if(info)
            info->reserved1 = 0;
    }
    g_list_free(cols);
}

static void on_row_changed(GtkTreeModel *tree_model, GtkTreePath *path,
                           GtkTreeIter *iter, FmStandardView* fv)
{
    if(fv->mode == FM_FV_LIST_VIEW)
        _reset_columns_widths(GTK_TREE_VIEW(fv->view));
}

static void on_row_deleted(GtkTreeModel *tree_model, GtkTreePath  *path,
                           FmStandardView* fv)
{
    if(fv->mode == FM_FV_LIST_VIEW)
        _reset_columns_widths(GTK_TREE_VIEW(fv->view));
    /* reset tooltip - it may stick if mouse-over item was deleted,
       see how FmCellRendererText works on that regard */
    g_object_set(G_OBJECT(fv->view), "tooltip-text", NULL, NULL);
}

static void on_row_inserted(GtkTreeModel *tree_model, GtkTreePath *path,
                            GtkTreeIter *iter, FmStandardView* fv)
{
    if(fv->mode == FM_FV_LIST_VIEW)
        _reset_columns_widths(GTK_TREE_VIEW(fv->view));
}

static void unset_model(FmStandardView* fv)
{
    if(fv->model)
    {
        FmFolderModel* model = fv->model;
        /* g_debug("unset_model: %p, n_ref = %d", model, G_OBJECT(model)->ref_count); */
        g_object_unref(model);
        g_signal_handlers_disconnect_by_func(model, on_row_inserted, fv);
        g_signal_handlers_disconnect_by_func(model, on_row_deleted, fv);
        g_signal_handlers_disconnect_by_func(model, on_row_changed, fv);
        fv->model = NULL;
    }
}

static void unset_view(FmStandardView* fv);
static void fm_standard_view_dispose(GObject *object)
{
    FmStandardView *self;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_STANDARD_VIEW(object));
    self = (FmStandardView*)object;
    /* g_debug("fm_standard_view_dispose: %p", self); */

    unset_model(self);

    if(G_LIKELY(self->view))
        unset_view(self);

    fm_standard_view_cancel_pending_rename(self);

    if(self->search_window)
    {
        gtk_widget_destroy(self->search_window);
        self->search_window = NULL;
        self->search_entry = NULL;
    }

    if(self->renderer_pixbuf)
    {
        g_object_unref(self->renderer_pixbuf);
        self->renderer_pixbuf = NULL;
    }

    if(self->renderer_text)
    {
        g_object_unref(self->renderer_text);
        self->renderer_text = NULL;
    }

    if(self->cached_selected_files)
    {
        fm_file_info_list_unref(self->cached_selected_files);
        self->cached_selected_files = NULL;
    }

    if(self->cached_selected_file_paths)
    {
        fm_path_list_unref(self->cached_selected_file_paths);
        self->cached_selected_file_paths = NULL;
    }

    if(self->dnd_src)
    {
        g_signal_handlers_disconnect_by_func(self->dnd_src, on_dnd_src_data_get, self);
        g_object_unref(self->dnd_src);
        self->dnd_src = NULL;
    }
    if(self->dnd_dest)
    {
        g_object_unref(self->dnd_dest);
        self->dnd_dest = NULL;
    }

    g_signal_handlers_disconnect_by_func(fm_config, on_single_click_changed, object);
    g_signal_handlers_disconnect_by_func(fm_config, on_auto_selection_delay_changed, object);
    g_signal_handlers_disconnect_by_func(fm_config, on_middle_click_changed, object);

    if(self->sel_changed_idle)
    {
        g_source_remove(self->sel_changed_idle);
        self->sel_changed_idle = 0;
    }

    if(self->icon_size_changed_handler)
    {
        g_signal_handler_disconnect(fm_config, self->icon_size_changed_handler);
        self->icon_size_changed_handler = 0;
    }
    if(self->show_full_names_handler)
    {
        g_signal_handler_disconnect(fm_config, self->show_full_names_handler);
        self->show_full_names_handler = 0;
    }

    if (self->igesture)
    {
        g_object_unref (self->igesture);
        self->igesture = NULL;
    }
    if (self->lgesture)
    {
        g_object_unref (self->lgesture);
        self->lgesture = NULL;
    }
    (* G_OBJECT_CLASS(fm_standard_view_parent_class)->dispose)(object);
}

static void set_icon_size(FmStandardView* fv, guint icon_size)
{
    FmCellRendererPixbuf* render = fv->renderer_pixbuf;

    fm_cell_renderer_pixbuf_set_fixed_size(render, icon_size, icon_size);

    if(!fv->model)
        return;

    fm_folder_model_set_icon_size(fv->model, icon_size);

    if( fv->mode != FM_FV_LIST_VIEW ) /* this is a GtkIconView */
    {
        /* set row spacing in range 2...12 pixels */
        gint c_size = MIN(12, 2 + icon_size / 8);
        gtk_icon_view_set_row_spacing(GTK_ICON_VIEW(fv->view), c_size);
    }
}

static void on_big_icon_size_changed(FmConfig* cfg, FmStandardView* fv)
{
    guint item_width = cfg->big_icon_size + 40;
    /* reset GtkIconView item text sizes */
    g_object_set((GObject*)fv->renderer_text, "wrap-width", item_width, NULL);
    set_icon_size(fv, cfg->big_icon_size);
}

static void on_small_icon_size_changed(FmConfig* cfg, FmStandardView* fv)
{
    set_icon_size(fv, cfg->small_icon_size);
}

static void on_thumbnail_size_changed(FmConfig* cfg, FmStandardView* fv)
{
    guint item_width = MAX(cfg->thumbnail_size, 96);
    /* reset GtkIconView item text sizes */
    g_object_set((GObject*)fv->renderer_text, "wrap-width", item_width, NULL);
    /* FIXME: thumbnail and icons should have different sizes */
    /* maybe a separate API: fm_folder_model_set_thumbnail_size() */
    set_icon_size(fv, cfg->thumbnail_size);
}

static void on_show_full_names_changed(FmConfig* cfg, FmStandardView* fv)
{
    gint font_height;

    g_return_if_fail(fv->renderer_text);

    if (fm_config->show_full_names)
        font_height = 0;
    else
    {
        /* calculate text row size */
        PangoContext *pc = gtk_widget_get_pango_context((GtkWidget*)fv);
        PangoFontMetrics *metrics = pango_context_get_metrics(pc, NULL, NULL);

        font_height = (pango_font_metrics_get_ascent(metrics)
                       + pango_font_metrics_get_descent(metrics)) / PANGO_SCALE + 1;
        pango_font_metrics_unref(metrics);
    }
    if(fv->mode == FM_FV_ICON_VIEW || fv->mode == FM_FV_ICON_OR_THUMB_VIEW)
        font_height *= 3;
    else /* thumbnail view */
        font_height *= 5;
    g_object_set((GObject*)fv->renderer_text, "max-height", font_height, NULL);
    /* we cannot use gtk_widget_queue_resize() since GtkIconView does not
       recalculate sizes on that, therefore we do a little trick here:
       we reset all attributes we set before enforcing it to relayout */
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(fv->view),
                                   GTK_CELL_RENDERER(fv->renderer_text),
                                   "text", FM_FOLDER_MODEL_COL_NAME, NULL);
}

static void set_drag_dest_list_item(FmStandardView* fv, GtkTreePath* tp)
{
    gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(fv->view), tp,
                                    GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
}

static void set_drag_dest_icon_item(FmStandardView* fv, GtkTreePath* tp)
{
    gtk_icon_view_set_drag_dest_item(GTK_ICON_VIEW(fv->view), tp, GTK_ICON_VIEW_DROP_INTO);
}

static GtkTreePath* get_drop_path_list_view(FmStandardView* fv, gint x, gint y)
{
    GtkTreePath* tp = NULL;
    GtkTreeViewColumn* col;

    gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(fv->view), x, y, &x, &y);
    if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(fv->view), x, y, &tp, &col, NULL, NULL))
    {
        if(gtk_tree_view_column_get_sort_column_id(col)!=FM_FOLDER_MODEL_COL_NAME)
        {
            gtk_tree_path_free(tp);
            tp = NULL;
        }
    }
    return tp;
}

static GtkTreePath* get_drop_path_icon_view(FmStandardView* fv, gint x, gint y)
{
    GtkTreePath* tp;

    tp = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(fv->view), x, y);
    return tp;
}

static gboolean on_drag_motion(GtkWidget *dest_widget,
                                 GdkDragContext *drag_context,
                                 gint x,
                                 gint y,
                                 guint time,
                                 FmStandardView* fv)
{
    gboolean ret;
    GdkDragAction action = 0;
    GdkAtom target = fm_dnd_dest_find_target(fv->dnd_dest, drag_context);

    if(target == GDK_NONE)
        return FALSE;

    ret = FALSE;
    /* files are being dragged */
    if(fm_dnd_dest_is_target_supported(fv->dnd_dest, target))
    {
        GtkTreePath* tp = fv->get_drop_path(fv, x, y);
        if(tp)
        {
            GtkTreeIter it;
            if(gtk_tree_model_get_iter(GTK_TREE_MODEL(fv->model), &it, tp))
            {
                FmFileInfo* fi;
                gtk_tree_model_get(GTK_TREE_MODEL(fv->model), &it, FM_FOLDER_MODEL_COL_INFO, &fi, -1);
                fm_dnd_dest_set_dest_file(fv->dnd_dest, fi);
            }
        }
        else
        {
            FmFolderModel* model = fv->model;
            if (model)
            {
                FmFolder* folder = fm_folder_model_get_folder(model);
                fm_dnd_dest_set_dest_file(fv->dnd_dest, fm_folder_get_info(folder));
            }
            else
                fm_dnd_dest_set_dest_file(fv->dnd_dest, NULL);
        }
        action = fm_dnd_dest_get_default_action(fv->dnd_dest, drag_context, target);
        ret = action != 0;
        fv->set_drag_dest(fv, ret ? tp : NULL);
        if (tp)
            gtk_tree_path_free(tp);
    }
    gdk_drag_status(drag_context, action, time);

    return ret;
}

static void on_fv_gesture_pressed (GtkGestureLongPress *, gdouble x, gdouble y, FmStandardView* fv)
{
    longpress = TRUE;
    gtk_icon_view_get_item_at_pos (GTK_ICON_VIEW (fv->view), x, y, &gpath, NULL);
    fm_standard_view_cancel_pending_rename (fv);
}

static void on_fv_gesture_end (GtkGestureLongPress *, GdkEventSequence *, FmStandardView* fv)
{
    if (longpress) fm_folder_view_item_clicked  ((FmFolderView *) fv, gpath, FM_FV_CONTEXT_MENU, 0, -1, -1);
    if (gpath) gtk_tree_path_free (gpath);
    gpath = NULL;
    longpress = FALSE;
}

static inline void create_icon_view(FmStandardView* fv, GList* sels)
{
    GList *l;
    GtkCellRenderer* render;
    FmFolderModel* model = fv->model;
    int icon_size = 0, item_width, font_height;

    fv->view = gtk_icon_view_new();

    if(fv->renderer_pixbuf)
        g_object_unref(fv->renderer_pixbuf);
    fv->renderer_pixbuf = g_object_ref_sink(fm_cell_renderer_pixbuf_new());
    fm_cell_renderer_pixbuf_set_scale (fv->renderer_pixbuf, gtk_widget_get_scale_factor (GTK_WIDGET (fv)));
    render = (GtkCellRenderer*)fv->renderer_pixbuf;

    g_object_set((GObject*)render, "follow-state", TRUE, NULL );
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(fv->view), render, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render, "pixbuf", FM_FOLDER_MODEL_COL_ICON );
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render, "info", FM_FOLDER_MODEL_COL_INFO );

    if(fv->mode == FM_FV_COMPACT_VIEW) /* compact view */
    {
        fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::small_icon_size", G_CALLBACK(on_small_icon_size_changed), fv);
        icon_size = fm_config->small_icon_size;
        fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
        if(model)
        {
            fm_folder_model_set_icon_size(model, icon_size);
            fm_folder_model_show_thumbnails(model, FALSE);
        }

        render = fm_cell_renderer_text_new();
        g_object_set((GObject*)render,
                     "xalign", 1.0, /* FIXME: why this needs to be 1.0? */
                     "yalign", 0.5,
                     NULL );
        /* GtkIconView has no column-major layout mode equivalent to Exo's
           EXO_ICON_VIEW_LAYOUT_COLS, so compact view flows row-major here */
        gtk_icon_view_set_item_orientation( GTK_ICON_VIEW(fv->view), GTK_ORIENTATION_HORIZONTAL );
    }
    else /* big icon view or thumbnail view */
    {
        if(fv->show_full_names_handler == 0)
            fv->show_full_names_handler = g_signal_connect(fm_config, "changed::show_full_names", G_CALLBACK(on_show_full_names_changed), fv);
        if (fm_config->show_full_names)
            font_height = 0;
        else
        {
            /* calculate text row size */
            PangoContext *pc = gtk_widget_get_pango_context((GtkWidget*)fv);
            PangoFontMetrics *metrics = pango_context_get_metrics(pc, NULL, NULL);

            font_height = (pango_font_metrics_get_ascent(metrics)
                           + pango_font_metrics_get_descent(metrics)) / PANGO_SCALE + 1;
            pango_font_metrics_unref(metrics);
        }
        if(fv->mode == FM_FV_ICON_VIEW || fv->mode == FM_FV_ICON_OR_THUMB_VIEW)
        {
            fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::big_icon_size", G_CALLBACK(on_big_icon_size_changed), fv);
            icon_size = fm_config->big_icon_size;
            fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
            if(model)
            {
                fm_folder_model_set_icon_size(model, icon_size);
                if (fv->mode == FM_FV_ICON_OR_THUMB_VIEW) fm_folder_model_show_thumbnails (model, fv->show_thumbs);
                else fm_folder_model_show_thumbnails(model, FALSE);
            }

            render = fm_cell_renderer_text_new();
            item_width = icon_size + 40;
            g_object_set((GObject*)render,
                         "wrap-mode", PANGO_WRAP_WORD_CHAR,
                         "wrap-width", item_width,
                         "max-height", font_height * 3,
                         "alignment", PANGO_ALIGN_CENTER,
                         "xalign", 0.5,
                         "yalign", 0.0,
                         NULL );
            gtk_icon_view_set_column_spacing( GTK_ICON_VIEW(fv->view), 4 );
        }
        else
        {
            fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::thumbnail_size", G_CALLBACK(on_thumbnail_size_changed), fv);
            icon_size = fm_config->thumbnail_size;
            fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
            if(model)
            {
                fm_folder_model_set_icon_size(model, icon_size);
                fm_folder_model_show_thumbnails(model, TRUE);
            }

            render = fm_cell_renderer_text_new();
            item_width = MAX(icon_size, 96);
            g_object_set((GObject*)render,
                         "wrap-mode", PANGO_WRAP_WORD_CHAR,
                         "wrap-width", item_width,
                         "max-height", font_height * 5,
                         "alignment", PANGO_ALIGN_CENTER,
                         "xalign", 0.5,
                         "yalign", 0.0,
                         NULL );
            gtk_icon_view_set_column_spacing( GTK_ICON_VIEW(fv->view), 8 );
        }
    }
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(fv->view), render, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render,
                                "text", FM_FOLDER_MODEL_COL_NAME );
    if(fv->renderer_text)
        g_object_unref(fv->renderer_text);
    fv->renderer_text = FM_CELL_RENDERER_TEXT(g_object_ref_sink(render));
    g_signal_connect(fv->view, "item-activated", G_CALLBACK(on_icon_view_item_activated), fv);
    g_signal_connect(fv->view, "button-release-event", G_CALLBACK(on_icon_view_button_release), fv);
    /* connect "after" so GtkIconView's own key handling (arrow-key nav,
       Enter-to-activate, Space-to-toggle) gets first refusal; GTK's
       input-event signals stop at the first handler that returns TRUE,
       so this only runs for genuinely unhandled keys */
    g_signal_connect_after(fv->view, "key-press-event", G_CALLBACK(on_icon_view_key_press), fv);

    if (!fv->igesture)
    {
        fv->igesture = gtk_gesture_long_press_new (fv->view);
        gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (fv->igesture), fm_config->gestures_touch_only);
        g_signal_connect (fv->igesture, "pressed", G_CALLBACK (on_fv_gesture_pressed), fv);
        g_signal_connect (fv->igesture, "end", G_CALLBACK (on_fv_gesture_end), fv);
        gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (fv->igesture), GTK_PHASE_CAPTURE);
    }

    g_signal_connect(fv->view, "selection-changed", G_CALLBACK(on_sel_changed), fv);
    gtk_icon_view_set_model(GTK_ICON_VIEW(fv->view), (GtkTreeModel*)fv->model);
    gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(fv->view), fv->sel_mode);
    gtk_icon_view_set_activate_on_single_click(GTK_ICON_VIEW(fv->view), fm_config->single_click);

    for(l = sels;l;l=l->next)
        gtk_icon_view_select_path(GTK_ICON_VIEW(fv->view), l->data);
}

static void _update_width_sizing(GtkTreeViewColumn* col, gint width)
{
    if(width > 0)
    {
        gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_fixed_width(col, width);
    }
    else
    {
        gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_resizable(col, TRUE);
    }
    gtk_tree_view_column_queue_resize(col);
}

/* Each change will generate notify for all columns from first to last.
 * 1) on window resizing only column Name may change - the size may grow to
 *    fill any additional space
 * 2) on manual column resize the resized column will change; last column
 *    will change size too if horizontal scroll bar isn't visible */
static void on_column_width_changed(GtkTreeViewColumn* col, GParamSpec *pspec,
                                    FmStandardView* view)
{
    FmFolderViewColumnInfo *info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
    GList *cols = gtk_tree_view_get_columns(GTK_TREE_VIEW(view->view));
    int width;
    guint pos;

    pos = g_list_index(cols, col);
    width = gtk_tree_view_column_get_width(col);
    /* g_debug("column width changed: [%u] id %u: %d", pos, info->col_id, width); */
    /* use info->reserved1 as 'last width' */
    if(width != info->reserved1)
    {
        if(info->col_id == FM_FOLDER_MODEL_COL_NAME)
            view->name_updated = TRUE;
        else if(info->reserved1 && view->updated_col < 0)
            view->updated_col = pos;
        info->reserved1 = width;
    }
    if(pos == g_list_length(cols) - 1) /* got all columns, decide what we got */
    {
        if(!view->name_updated && view->updated_col >= 0)
        {
            col = g_list_nth_data(cols, view->updated_col);
            info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
            if(info)
            {
                info->width = info->reserved1;
                /* g_debug("column %u changed width to %d", info->col_id, info->width); */
                fm_folder_view_columns_changed(FM_FOLDER_VIEW(view));
            }
        }
        /* FIXME: how to detect manual change of Name mix width reliably? */
        view->updated_col = -1;
        view->name_updated = FALSE;
    }
    g_list_free(cols);
}

static void on_column_hide(GtkMenuItem* menu_item, GtkTreeViewColumn* col)
{
    GtkWidget* view = gtk_tree_view_column_get_tree_view(col);
    gtk_tree_view_remove_column(GTK_TREE_VIEW(view), col);
    fm_folder_view_columns_changed(FM_FOLDER_VIEW(gtk_widget_get_parent(view)));
}

static void on_column_move_left(GtkMenuItem* menu_item, GtkTreeViewColumn* col)
{
    GtkTreeView* view = GTK_TREE_VIEW(gtk_tree_view_column_get_tree_view(col));
    GList* list, *l;

    list = gtk_tree_view_get_columns(view);
    l = g_list_find(list, col);
    if(l && l->prev)
    {
        gtk_tree_view_move_column_after(view, col,
                                        l->prev->prev ? l->prev->prev->data : NULL);
        fm_folder_view_columns_changed(FM_FOLDER_VIEW(gtk_widget_get_parent(GTK_WIDGET(view))));
    }
    g_list_free(list);
}

static void on_column_move_right(GtkMenuItem* menu_item, GtkTreeViewColumn* col)
{
    GtkTreeView* view = GTK_TREE_VIEW(gtk_tree_view_column_get_tree_view(col));
    GList* list, *l;

    list = gtk_tree_view_get_columns(view);
    l = g_list_find(list, col);
    if(l && l->next)
    {
        gtk_tree_view_move_column_after(view, col, l->next->data);
        fm_folder_view_columns_changed(FM_FOLDER_VIEW(gtk_widget_get_parent(GTK_WIDGET(view))));
    }
    g_list_free(list);
}

static GtkTreeViewColumn* create_list_view_column(FmStandardView* fv, FmFolderViewColumnInfo *set);

static void on_column_add(GtkMenuItem* menu_item, GtkTreeViewColumn* col)
{
    GtkWidget *view = gtk_tree_view_column_get_tree_view(col);
    GtkWidget *fv = gtk_widget_get_parent(view);
    GtkTreeViewColumn *new_col;
    FmFolderViewColumnInfo info;
    g_return_if_fail(FM_IS_STANDARD_VIEW(fv));
    memset(&info, 0, sizeof(info));
    info.col_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_item), "col_id"));
    new_col = create_list_view_column((FmStandardView*)fv, &info);
    if(new_col) /* skip it if failed */
    {
        gtk_tree_view_move_column_after(GTK_TREE_VIEW(view), new_col, col);
        fm_folder_view_columns_changed(FM_FOLDER_VIEW(fv));
    }
}

static void on_column_auto_adjust(GtkMenuItem* menu_item, GtkTreeViewColumn* col)
{
    FmFolderViewColumnInfo* info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
    info->width = 0;
    info->reserved1 = 0;
    _update_width_sizing(col, 0);
    /* g_debug("auto sizing column %u", info->col_id); */
    fm_folder_view_columns_changed(FM_FOLDER_VIEW(gtk_widget_get_parent(gtk_tree_view_column_get_tree_view(col))));
}

/* FIXME: add support for 'list_view_size_units' config setting:
   - on FM_FOLDER_MODEL_COL_SIZE add submenu with list of available values
   - on click that submenu item change the setting, emit signal, queue reload */
static gboolean on_column_button_press_event(GtkWidget *button,
                                             GdkEventButton *event,
                                             GtkTreeViewColumn* col)
{
    if(event->button == 3)
    {
        GtkWidget* view = gtk_tree_view_column_get_tree_view(col);
        GtkWidget* fv = gtk_widget_get_parent(view);
        GList *columns, *l;
        GSList *cols_list, *ld;
        GtkMenuShell* menu;
        GtkWidget* menu_item;
        const char* label;
        char* menu_item_label;
        FmFolderViewColumnInfo* info;
        guint i;

        g_return_val_if_fail(FM_IS_STANDARD_VIEW(fv), FALSE);

        columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(view));
        l = g_list_find(columns, col);
        if(l == NULL)
        {
            g_warning("column not found in GtkTreeView");
            g_list_free(columns);
            return FALSE;
        }

        menu = GTK_MENU_SHELL(gtk_menu_new());
        /* destroy the menu when selection is done. */
        g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);

        info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
        label = fm_folder_model_col_get_title(FM_STANDARD_VIEW(fv)->model, info->col_id);
        menu_item_label = g_strdup_printf(_("_Hide %s"), label);
        menu_item = gtk_menu_item_new_with_mnemonic(menu_item_label);
        g_free(menu_item_label);
        gtk_menu_shell_append(menu, menu_item);
        g_signal_connect(menu_item, "activate", G_CALLBACK(on_column_hide), col);
        if(info->col_id == FM_FOLDER_MODEL_COL_NAME) /* Name is immutable */
            gtk_widget_set_sensitive(menu_item, FALSE);

        menu_item = gtk_menu_item_new_with_mnemonic(_("_Move Left"));
        gtk_menu_shell_append(menu, menu_item);
        g_signal_connect(menu_item, "activate", G_CALLBACK(on_column_move_left), col);
        if(NULL == l->prev) /* the left most column */
            gtk_widget_set_sensitive(menu_item, FALSE);

        menu_item = gtk_menu_item_new_with_mnemonic(_("Move _Right"));
        gtk_menu_shell_append(menu, menu_item);
        g_signal_connect(menu_item, "activate", G_CALLBACK(on_column_move_right), col);
        if(NULL == l->next) /* the right most column */
            gtk_widget_set_sensitive(menu_item, FALSE);
        g_list_free(columns);

        /* create list of missing columns for 'Add' submenu */
        cols_list = fm_folder_view_get_columns(FM_FOLDER_VIEW(fv));
        menu_item_label = NULL; /* mark for below */
        for(i = 0; fm_folder_model_col_is_valid(i); i++)
        {
            label = fm_folder_model_col_get_title(FM_STANDARD_VIEW(fv)->model, i);
            if(!label)
                continue;
            for(ld = cols_list; ld; ld = ld->next)
                if(((FmFolderViewColumnInfo*)ld->data)->col_id == i)
                    break;
            /* if the column is already in the folder view, don't add it to the menu */
            if(ld)
                continue;
            if(menu_item_label == NULL)
                gtk_menu_shell_append(menu, gtk_separator_menu_item_new());
            menu_item_label = g_strdup_printf(_("Show %s"), label);
            menu_item = gtk_menu_item_new_with_label(menu_item_label);
            g_object_set_data(G_OBJECT(menu_item), "col_id", GINT_TO_POINTER(i));
            g_signal_connect(menu_item, "activate", G_CALLBACK(on_column_add), col);
            g_free(menu_item_label);
            gtk_menu_shell_append(menu, menu_item);
        }
        g_slist_free(cols_list);

        if(info->width > 0 && info->col_id != FM_FOLDER_MODEL_COL_NAME)
        {
            gtk_menu_shell_append(menu, gtk_separator_menu_item_new());
            menu_item = gtk_menu_item_new_with_mnemonic(_("_Forget Width"));
            gtk_menu_shell_append(menu, menu_item);
            g_signal_connect(menu_item, "activate", G_CALLBACK(on_column_auto_adjust), col);
        }

        gtk_widget_show_all(GTK_WIDGET(menu));
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, event->time);
        return TRUE;
    }
    else if(event->button == 1)
    {
        GtkWidget* view = gtk_tree_view_column_get_tree_view(col);
        GtkWidget* fv = gtk_widget_get_parent(view);
        FmFolderViewColumnInfo* info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
        g_return_val_if_fail(FM_IS_STANDARD_VIEW(fv), FALSE);
        return !fm_folder_model_col_is_sortable(FM_STANDARD_VIEW(fv)->model, info->col_id);
    }
    return FALSE;
}

static gboolean on_column_button_released_event(GtkWidget *button, GdkEventButton *event,
                                        GtkTreeViewColumn* col)
{
    if(event->button == 1)
    {
        GtkWidget* view = gtk_tree_view_column_get_tree_view(col);
        GtkWidget* fv = gtk_widget_get_parent(view);
        FmFolderViewColumnInfo* info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
        g_return_val_if_fail(FM_IS_STANDARD_VIEW(fv), FALSE);
        return !fm_folder_model_col_is_sortable(FM_STANDARD_VIEW(fv)->model, info->col_id);
    }
    return FALSE;
}

static GtkTreeViewColumn* create_list_view_column(FmStandardView* fv,
                                                  FmFolderViewColumnInfo *set)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;
    const char* title;
    FmFolderViewColumnInfo* info;
    GtkWidget *label;
    FmFolderModelCol col_id;

    g_return_val_if_fail(set != NULL, NULL); /* invalid arg */
    col_id = set->col_id;
    title = fm_folder_model_col_get_title(fv->model, col_id);
    g_return_val_if_fail(title != NULL, NULL); /* invalid column */

    /* g_debug("adding column id=%u", col_id); */
    col = gtk_tree_view_column_new();
    render = gtk_cell_renderer_text_new();
    gtk_tree_view_column_set_title(col, title);
    info = _sv_column_info_new(col_id);
    info->width = set->width;
    g_object_set_qdata_full(G_OBJECT(col), fm_qdata_id, info, _sv_column_info_free);

    switch(col_id)
    {
    case FM_FOLDER_MODEL_COL_NAME:
        /* special handling for Name column */
        gtk_tree_view_column_pack_start(col, GTK_CELL_RENDERER(fv->renderer_pixbuf), FALSE);
        gtk_tree_view_column_set_attributes(col, GTK_CELL_RENDERER(fv->renderer_pixbuf),
                                            "pixbuf", FM_FOLDER_MODEL_COL_ICON,
                                            "info", FM_FOLDER_MODEL_COL_INFO, NULL);
        g_object_set(render, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_tree_view_column_set_expand(col, TRUE);
        /* Workaround for column collapse issue when double-clicking separator */
        gtk_tree_view_column_set_min_width(col, 50);
        if(set->width <= 0)
            info->width = 200;
        break;
    case FM_FOLDER_MODEL_COL_SIZE:
        g_object_set(render, "xalign", 1.0, NULL);
    default:
        if(set->width < 0)
            info->width = fm_folder_model_col_get_default_width(fv->model, col_id);
    }
    _update_width_sizing(col, info->width);

    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_set_attributes(col, render, "text", col_id, NULL);
    gtk_tree_view_column_set_resizable(col, TRUE);
    /* Unfortunately if we don't set it sortable we cannot right-click it too
    if(fm_folder_model_col_is_sortable(fv->model, col_id)) */
        gtk_tree_view_column_set_sort_column_id(col, col_id);
    gtk_tree_view_append_column(GTK_TREE_VIEW(fv->view), col);
    if(G_UNLIKELY(col_id == FM_FOLDER_MODEL_COL_NAME))
        fv->activable_column = col;

    g_signal_connect(col, "notify::width", G_CALLBACK(on_column_width_changed), fv);

    label = gtk_tree_view_column_get_button(col);
    if(label)
    {
        /* disable left-click handling for non-sortable columns */
        g_signal_connect(label, "button-press-event",
                         G_CALLBACK(on_column_button_press_event), col);
        /* handle right-click on column header */
        g_signal_connect(label, "button-release-event",
                         G_CALLBACK(on_column_button_released_event), col);
        /* FIXME: how to disconnect it later? */
    }

    return col;
}

static void _check_tree_columns_defaults(FmStandardView* fv)
{
    const FmFolderViewColumnInfo cols[] = {
        {FM_FOLDER_MODEL_COL_NAME},
        {FM_FOLDER_MODEL_COL_DESC},
        {FM_FOLDER_MODEL_COL_SIZE},
        {FM_FOLDER_MODEL_COL_MTIME} };
    GSList* cols_list = NULL;
    GList* tree_columns;
    guint i;

    tree_columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(fv->view));
    if(tree_columns != NULL) /* already set */
    {
        g_list_free(tree_columns);
        return;
    }
    /* Set default columns to show in detailed list mode. */
    for(i = 0; i < G_N_ELEMENTS(cols); i++)
        cols_list = g_slist_append(cols_list, (gpointer)&cols[i]);
    fm_folder_view_set_columns(FM_FOLDER_VIEW(fv), cols_list);
    g_slist_free(cols_list);
}

static void on_lv_gesture_pressed (GtkGestureLongPress *, gdouble x, gdouble y, FmStandardView* fv)
{
    GtkTreeViewColumn* col;
    int bx, by;
    longpress = TRUE;
    gtk_tree_view_convert_widget_to_bin_window_coords (GTK_TREE_VIEW (fv->view), x, y, &bx, &by);
    gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (fv->view), bx, by, &gpath, &col, NULL, NULL);
    fm_standard_view_cancel_pending_rename(fv);
}

static void on_lv_gesture_end (GtkGestureLongPress *, GdkEventSequence *, FmStandardView* fv)
{
    if (longpress) fm_folder_view_item_clicked  ((FmFolderView *) fv, gpath, FM_FV_CONTEXT_MENU, 0, -1, -1);
    if (gpath) gtk_tree_path_free (gpath);
    gpath = NULL;
    longpress = FALSE;
}

static inline void create_list_view(FmStandardView* fv, GList* sels)
{
    GtkTreeSelection* ts;
    GList *l;
    FmFolderModel* model = fv->model;
    int icon_size = 0;

    fv->view = gtk_tree_view_new();

    if(fv->renderer_pixbuf)
        g_object_unref(fv->renderer_pixbuf);
    fv->renderer_pixbuf = g_object_ref_sink(fm_cell_renderer_pixbuf_new());
    fm_cell_renderer_pixbuf_set_scale (fv->renderer_pixbuf, gtk_widget_get_scale_factor (GTK_WIDGET (fv)));
    fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::small_icon_size", G_CALLBACK(on_small_icon_size_changed), fv);
    icon_size = fm_config->small_icon_size;
    fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
    if(model)
    {
        fm_folder_model_set_icon_size(model, icon_size);
        fm_folder_model_show_thumbnails(model, FALSE);
        _check_tree_columns_defaults(fv);
        gtk_tree_view_set_search_column(GTK_TREE_VIEW(fv->view),
                                        FM_FOLDER_MODEL_COL_NAME);
    }

    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(fv->view), TRUE);
    gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(fv->view), TRUE);
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(fv->view), fm_config->single_click);

    ts = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
    g_signal_connect(fv->view, "row-activated", G_CALLBACK(on_tree_view_row_activated), fv);
    g_signal_connect(fv->view, "button-release-event", G_CALLBACK(on_tree_view_button_release), fv);
    g_signal_connect(ts, "changed", G_CALLBACK(on_sel_changed), fv);
    gtk_tree_view_set_model(GTK_TREE_VIEW(fv->view), GTK_TREE_MODEL(model));
    gtk_tree_selection_set_mode(ts, fv->sel_mode);
    for(l = sels;l;l=l->next)
        gtk_tree_selection_select_path(ts, (GtkTreePath*)l->data);

    if (!fv->lgesture)
    {
        fv->lgesture = gtk_gesture_long_press_new (fv->view);
        gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (fv->lgesture), fm_config->gestures_touch_only);
        g_signal_connect (fv->lgesture, "pressed", G_CALLBACK (on_lv_gesture_pressed), fv);
        g_signal_connect (fv->lgesture, "end", G_CALLBACK (on_lv_gesture_end), fv);
        gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (fv->lgesture), GTK_PHASE_CAPTURE);
    }
}

static void unset_view(FmStandardView* fv)
{
    /* these signals connected by view creators */
    if(fv->mode == FM_FV_LIST_VIEW)
    {
        GtkTreeSelection* ts = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
        g_signal_handlers_disconnect_by_func(ts, on_sel_changed, fv);
        g_signal_handlers_disconnect_by_func(fv->view, on_tree_view_row_activated, fv);
        g_signal_handlers_disconnect_by_func(fv->view, on_tree_view_button_release, fv);
        fm_standard_view_cancel_pending_rename(fv);
        fv->activable_column = NULL;
        if (fv->lgesture)
        {
            g_object_unref (fv->lgesture);
            fv->lgesture = NULL;
        }
    }
    else
    {
        g_signal_handlers_disconnect_by_func(fv->view, on_sel_changed, fv);
        g_signal_handlers_disconnect_by_func(fv->view, on_icon_view_item_activated, fv);
        g_signal_handlers_disconnect_by_func(fv->view, on_icon_view_button_release, fv);
        g_signal_handlers_disconnect_by_func(fv->view, on_icon_view_key_press, fv);
        fm_standard_view_cancel_pending_rename(fv);
        fm_standard_view_search_dialog_hide(fv);
        if (fv->igesture)
        {
            g_object_unref (fv->igesture);
            fv->igesture = NULL;
        }
    }
    /* these signals connected by fm_standard_view_set_mode() */
    g_signal_handlers_disconnect_by_func(fv->view, on_drag_motion, fv);
    g_signal_handlers_disconnect_by_func(fv->view, on_btn_pressed, fv);

    fm_dnd_unset_dest_auto_scroll(fv->view);
    gtk_widget_destroy(GTK_WIDGET(fv->view));
    fv->view = NULL;
}

static void select_all_list_view(GtkWidget* view)
{
    GtkTreeSelection * tree_sel;
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_select_all(tree_sel);
}

static void unselect_all_list_view(GtkWidget* view)
{
    GtkTreeSelection * tree_sel;
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_unselect_all(tree_sel);
}

static void select_invert_list_view(FmFolderModel* model, GtkWidget* view)
{
    GtkTreeSelection *tree_sel;
    GtkTreeIter it;
    if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it))
        return;
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    do
    {
        if(gtk_tree_selection_iter_is_selected(tree_sel, &it))
            gtk_tree_selection_unselect_iter(tree_sel, &it);
        else
            gtk_tree_selection_select_iter(tree_sel, &it);
    }while( gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it ));
}

static void select_invert_icon_view(FmFolderModel* model, GtkWidget* view)
{
    GtkTreePath* path;
    int i, n;
    n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model), NULL);
    if(n == 0)
        return;
    path = gtk_tree_path_new_first();
    for( i=0; i<n; ++i, gtk_tree_path_next(path) )
    {
        if ( gtk_icon_view_path_is_selected(GTK_ICON_VIEW(view), path))
            gtk_icon_view_unselect_path(GTK_ICON_VIEW(view), path);
        else
            gtk_icon_view_select_path(GTK_ICON_VIEW(view), path);
    }
    gtk_tree_path_free(path);
}

static void select_path_list_view(FmFolderModel* model, GtkWidget* view, GtkTreeIter* it)
{
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_select_iter(sel, it);
}

static void select_path_icon_view(FmFolderModel* model, GtkWidget* view, GtkTreeIter* it)
{
    GtkTreePath* tp = gtk_tree_model_get_path(GTK_TREE_MODEL(model), it);
    if(tp)
    {
        gtk_icon_view_select_path(GTK_ICON_VIEW(view), tp);
        gtk_tree_path_free(tp);
    }
}

/**
 * fm_folder_view_set_mode
 * @fv: a widget to apply
 * @mode: new mode of view
 *
 * Since: 0.1.0
 * Deprecated: 1.0.1: Use fm_standard_view_set_mode() instead.
 */
void fm_folder_view_set_mode(FmFolderView* fv, guint mode)
{
    g_return_if_fail(FM_IS_STANDARD_VIEW(fv));

    fm_standard_view_set_mode((FmStandardView*)fv, mode);
}

/**
 * fm_standard_view_set_mode
 * @fv: a widget to apply
 * @mode: new mode of view
 *
 * Before 1.0.1 this API had name fm_folder_view_set_mode.
 *
 * Changes current view mode for folder in @fv.
 *
 * Since: 0.1.0
 */
void fm_standard_view_set_mode(FmStandardView* fv, FmStandardViewMode mode)
{
    if( mode != fv->mode )
    {
        GList *sels;
        gboolean has_focus;

        if( G_LIKELY(fv->view) )
        {
            has_focus = gtk_widget_has_focus(fv->view);
            /* preserve old selections */
            sels = fm_standard_view_get_selected_tree_paths(fv);

            unset_view(fv); /* it will destroy the fv->view widget */

            /* FIXME: compact view and icon view actually use the same
             * type of widget, GtkIconView. So it may be better to
             * reuse the widget when available. */
        }
        else
        {
            sels = NULL;
            has_focus = FALSE;
        }

        if(fv->icon_size_changed_handler)
        {
            g_signal_handler_disconnect(fm_config, fv->icon_size_changed_handler);
            fv->icon_size_changed_handler = 0;
        }
        if(fv->show_full_names_handler)
        {
            g_signal_handler_disconnect(fm_config, fv->show_full_names_handler);
            fv->show_full_names_handler = 0;
        }

        fv->mode = mode;
        switch(mode)
        {
        case FM_FV_COMPACT_VIEW:
        case FM_FV_ICON_VIEW:
        case FM_FV_THUMBNAIL_VIEW:
        case FM_FV_ICON_OR_THUMB_VIEW:
            create_icon_view(fv, sels);
            fv->set_single_click = (void(*)(GtkWidget*,gboolean))gtk_icon_view_set_activate_on_single_click;
            /* no hover-preselect or widget-level middle-click equivalent in
               GtkIconView; middle-click is handled directly in on_btn_pressed() */
            fv->set_auto_selection_delay = NULL;
            fv->set_middle_click = NULL;
            fv->get_drop_path = get_drop_path_icon_view;
            fv->set_drag_dest = set_drag_dest_icon_item;
            fv->select_all = (void(*)(GtkWidget*))gtk_icon_view_select_all;
            fv->unselect_all = (void(*)(GtkWidget*))gtk_icon_view_unselect_all;
            fv->select_invert = select_invert_icon_view;
            fv->select_path = select_path_icon_view;
            break;
        case FM_FV_LIST_VIEW: /* detailed list view */
            create_list_view(fv, sels);
            fv->set_single_click = (void(*)(GtkWidget*,gboolean))gtk_tree_view_set_activate_on_single_click;
            /* no hover-preselect or widget-level middle-click equivalent in
               GtkTreeView; middle-click is handled directly in on_btn_pressed() */
            fv->set_auto_selection_delay = NULL;
            fv->set_middle_click = NULL;
            fv->get_drop_path = get_drop_path_list_view;
            fv->set_drag_dest = set_drag_dest_list_item;
            fv->select_all = select_all_list_view;
            fv->unselect_all = unselect_all_list_view;
            fv->select_invert = select_invert_list_view;
            fv->select_path = select_path_list_view;
        }
        g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
        g_list_free(sels);

        fm_dnd_src_set_widget(fv->dnd_src, fv->view);
        fm_dnd_dest_set_widget(fv->dnd_dest, fv->view);
        g_signal_connect_after(fv->view, "drag-motion", G_CALLBACK(on_drag_motion), fv);
        /* connecting it after sometimes conflicts with system configuration
           (bug #3559831) so we just hope here it will be handled in order
           of connecting, i.e. after GtkIconView or GtkTreeView handler */
        g_signal_connect(fv->view, "button-press-event", G_CALLBACK(on_btn_pressed), fv);

        fm_dnd_set_dest_auto_scroll(fv->view, gtk_scrolled_window_get_hadjustment((GtkScrolledWindow*)fv), gtk_scrolled_window_get_vadjustment((GtkScrolledWindow*)fv));

        gtk_widget_show(fv->view);
        gtk_container_add(GTK_CONTAINER(fv), fv->view);

        if(has_focus) /* restore the focus if needed. */
            gtk_widget_grab_focus(fv->view);
    }
    else
    {
        /* g_debug("same mode"); */
    }
}

/**
 * fm_folder_view_get_mode
 * @fv: a widget to inspect
 *
 * Returns: current mode of view.
 *
 * Since: 0.1.0
 * Deprecated: 1.0.1: Use fm_standard_view_get_mode() instead.
 */
guint fm_folder_view_get_mode(FmFolderView* fv)
{
    g_return_val_if_fail(FM_IS_STANDARD_VIEW(fv), 0);

    return ((FmStandardView*)fv)->mode;
}

/**
 * fm_standard_view_get_mode
 * @fv: a widget to inspect
 *
 * Retrieves current view mode for folder in @fv.
 *
 * Before 1.0.1 this API had name fm_folder_view_get_mode.
 *
 * Returns: current mode of view.
 *
 * Since: 0.1.0
 */
FmStandardViewMode fm_standard_view_get_mode(FmStandardView* fv)
{
    return fv->mode;
}

static void fm_standard_view_set_selection_mode(FmFolderView* ffv, GtkSelectionMode mode)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    if(fv->sel_mode != mode)
    {
        fv->sel_mode = mode;
        switch(fv->mode)
        {
        case FM_FV_LIST_VIEW:
        {
            GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
            gtk_tree_selection_set_mode(sel, mode);
            break;
        }
        case FM_FV_ICON_VIEW:
        case FM_FV_COMPACT_VIEW:
        case FM_FV_THUMBNAIL_VIEW:
        case FM_FV_ICON_OR_THUMB_VIEW:
            gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(fv->view), mode);
            break;
        }
    }
}

static GtkSelectionMode fm_standard_view_get_selection_mode(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    return fv->sel_mode;
}

static void fm_standard_view_set_show_hidden(FmFolderView* ffv, gboolean show)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    fv->show_hidden = show;
}

static gboolean fm_standard_view_get_show_hidden(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    return fv->show_hidden;
}

/* returned list should be freed with g_list_free_full(list, gtk_tree_path_free) */
static GList* fm_standard_view_get_selected_tree_paths(FmStandardView* fv)
{
    GList *sels = NULL;
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
    {
        GtkTreeSelection* sel;
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
        sels = gtk_tree_selection_get_selected_rows(sel, NULL);
        break;
    }
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
    case FM_FV_ICON_OR_THUMB_VIEW:
        sels = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(fv->view));
        break;
    }
    return sels;
}

static inline FmFileInfoList* fm_standard_view_get_selected_files(FmStandardView* fv)
{
    /* don't generate the data again if we have it cached. */
    if(!fv->cached_selected_files)
    {
        GList* sels = fm_standard_view_get_selected_tree_paths(fv);
        GList *l, *next;
        if(sels)
        {
            fv->cached_selected_files = fm_file_info_list_new();
            for(l = sels;l;l=next)
            {
                FmFileInfo* fi;
                GtkTreeIter it;
                GtkTreePath* tp = (GtkTreePath*)l->data;
                gtk_tree_model_get_iter(GTK_TREE_MODEL(fv->model), &it, l->data);
                gtk_tree_model_get(GTK_TREE_MODEL(fv->model), &it, FM_FOLDER_MODEL_COL_INFO, &fi, -1);
                gtk_tree_path_free(tp);
                next = l->next;
                l->data = fm_file_info_ref( fi );
                l->prev = l->next = NULL;
                fm_file_info_list_push_tail_link(fv->cached_selected_files, l);
            }
        }
    }
    return fv->cached_selected_files;
}

static FmFileInfoList* fm_standard_view_dup_selected_files(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    return fm_file_info_list_ref(fm_standard_view_get_selected_files(fv));
}

static FmPathList* fm_standard_view_dup_selected_file_paths(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    if(!fv->cached_selected_file_paths)
    {
        FmFileInfoList* files = fm_standard_view_get_selected_files(fv);
        if(files)
            fv->cached_selected_file_paths = fm_path_list_new_from_file_info_list(files);
        else
            fv->cached_selected_file_paths = NULL;
    }
    return fm_path_list_ref(fv->cached_selected_file_paths);
}

static gint fm_standard_view_count_selected_files(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    gint count = 0;
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
    {
        GtkTreeSelection* sel;
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
        count = gtk_tree_selection_count_selected_rows(sel);
        break;
    }
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
    case FM_FV_ICON_OR_THUMB_VIEW:
    {
        GList* sels = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(fv->view));
        count = g_list_length(sels);
        g_list_free_full(sels, (GDestroyNotify)gtk_tree_path_free);
        break;
    }
    }
    return count;
}

/* click-selected-label-to-rename gesture (icon view only): a delayed
   second click on the label of an already-selected item, distinct from
   a fast double-click, replaces ExoIconView's internal pending_rename
   timer which is no longer available with plain GtkIconView. */
static void fm_standard_view_cancel_pending_rename(FmStandardView* fv)
{
    if(fv->pending_rename_timer)
    {
        g_source_remove(fv->pending_rename_timer);
        fv->pending_rename_timer = 0;
    }
    if(fv->pending_rename_path)
    {
        gtk_tree_path_free(fv->pending_rename_path);
        fv->pending_rename_path = NULL;
    }
}

static gboolean on_pending_rename_timeout(gpointer data)
{
    FmStandardView* fv = FM_STANDARD_VIEW(data);
    GtkTreePath* path = fv->pending_rename_path;

    fv->pending_rename_timer = 0;
    fv->pending_rename_path = NULL;
    if(path)
    {
        fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), path, FM_FV_ACTIVATED, 1, -1, -1);
        gtk_tree_path_free(path);
    }
    return FALSE;
}

static gboolean on_icon_view_button_release(GtkWidget* view, GdkEventButton* evt, FmStandardView* fv)
{
    if(evt->button == 1 && fv->pending_rename_path && !fv->pending_rename_timer)
    {
        GtkTreePath* tp = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(view), evt->x, evt->y);
        if(tp && gtk_tree_path_compare(tp, fv->pending_rename_path) == 0)
            fv->pending_rename_timer = gdk_threads_add_timeout_full(G_PRIORITY_DEFAULT, 500,
                                                                    on_pending_rename_timeout, fv, NULL);
        else
            fm_standard_view_cancel_pending_rename(fv);
        if(tp)
            gtk_tree_path_free(tp);
    }
    return FALSE;
}

static gboolean on_tree_view_button_release(GtkWidget* view, GdkEventButton* evt, FmStandardView* fv)
{
    if(evt->button == 1 && fv->pending_rename_path && !fv->pending_rename_timer &&
       evt->window == gtk_tree_view_get_bin_window(GTK_TREE_VIEW(view)))
    {
        GtkTreePath* tp = NULL;

        gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view), evt->x, evt->y, &tp, NULL, NULL, NULL);
        if(tp && gtk_tree_path_compare(tp, fv->pending_rename_path) == 0)
            fv->pending_rename_timer = gdk_threads_add_timeout_full(G_PRIORITY_DEFAULT, 500,
                                                                    on_pending_rename_timeout, fv, NULL);
        else
            fm_standard_view_cancel_pending_rename(fv);
        if(tp)
            gtk_tree_path_free(tp);
    }
    return FALSE;
}

/* ---- Interactive (type-ahead) search, icon view only ----
 * GtkIconView, unlike GtkTreeView or the former ExoIconView, has no
 * built-in interactive search support at all. This is a self-contained
 * reimplementation, ported from the equivalent working code in
 * src/pcmanfm/desktop.c (itself adapted from ExoIconView), adjusted to
 * work off FmFolderModel/GtkIconView instead of desktop's own item/focus
 * bookkeeping, and positioned relative to the view widget like Exo did. */
#define FM_STANDARD_VIEW_SEARCH_DIALOG_TIMEOUT (5000)

/* cut and paste from gtkwindow.c / gtkwidget.c, as ExoIconView and
   pcmanfm/desktop.c both do for the same purpose */
static void fm_standard_view_send_focus_change(GtkWidget* widget, gboolean in)
{
    GdkEvent* fevent = gdk_event_new(GDK_FOCUS_CHANGE);

    fevent->focus_change.type = GDK_FOCUS_CHANGE;
    fevent->focus_change.window = g_object_ref(gtk_widget_get_window(widget));
    fevent->focus_change.in = in;

    gtk_widget_send_focus_change(widget, fevent);

    gdk_event_free(fevent);
}

static void fm_standard_view_search_dialog_hide(FmStandardView* fv)
{
    if(fv->search_window == NULL || !gtk_widget_get_visible(fv->search_window))
        return;

    if(fv->search_entry_changed_id != 0)
    {
        g_signal_handler_disconnect(fv->search_entry, fv->search_entry_changed_id);
        fv->search_entry_changed_id = 0;
    }
    if(fv->search_timeout_id != 0)
    {
        g_source_remove(fv->search_timeout_id);
        fv->search_timeout_id = 0;
    }

    fm_standard_view_send_focus_change(fv->search_entry, FALSE);
    gtk_widget_hide(fv->search_window);
    gtk_entry_set_text(GTK_ENTRY(fv->search_entry), "");
}

static gboolean on_search_delete_event(GtkWidget* widget, GdkEventAny* evt, FmStandardView* fv)
{
    fm_standard_view_search_dialog_hide(fv);
    return TRUE;
}

static void on_search_timeout_destroy(gpointer data)
{
    FM_STANDARD_VIEW(data)->search_timeout_id = 0;
}

static gboolean on_search_timeout(gpointer data)
{
    FmStandardView* fv = FM_STANDARD_VIEW(data);

    if(!g_source_is_destroyed(g_main_current_source()))
        fm_standard_view_search_dialog_hide(fv);
    return FALSE;
}

static void fm_standard_view_search_update_timeout(FmStandardView* fv)
{
    if(fv->search_timeout_id == 0)
        return;
    g_source_remove(fv->search_timeout_id);
    fv->search_timeout_id = gdk_threads_add_timeout_full(G_PRIORITY_LOW,
                                                         FM_STANDARD_VIEW_SEARCH_DIALOG_TIMEOUT,
                                                         on_search_timeout, fv,
                                                         on_search_timeout_destroy);
}

static void fm_standard_view_focus_and_select_path(FmStandardView* fv, GtkTreePath* path)
{
    GtkIconView* view = GTK_ICON_VIEW(fv->view);

    gtk_icon_view_unselect_all(view);
    gtk_icon_view_select_path(view, path);
    gtk_icon_view_set_cursor(view, path, NULL, FALSE);
    gtk_icon_view_scroll_to_path(view, path, FALSE, 0.5, 0.5);
}

/* case/accent-insensitive prefix match of @name against normalized @key */
static gboolean fm_standard_view_search_name_matches(const char* name, const char* key)
{
    char* casefold;
    char* normalized;
    gboolean matches;

    if(name == NULL)
        return FALSE;
    casefold = g_utf8_casefold(name, -1);
    normalized = g_utf8_normalize(casefold, -1, G_NORMALIZE_ALL);
    g_free(casefold);
    matches = (strncmp(normalized, key, strlen(key)) == 0);
    g_free(normalized);
    return matches;
}

static void fm_standard_view_search_move(FmStandardView* fv, gboolean move_up)
{
    GtkTreeModel* model;
    GtkTreePath* cursor_path;
    GtkTreeIter it;
    const gchar* text;
    gchar* casefold, *key, *name;
    gboolean found = FALSE;

    if(!fv->model)
        return;
    model = GTK_TREE_MODEL(fv->model);

    text = gtk_entry_get_text(GTK_ENTRY(fv->search_entry));
    if(G_UNLIKELY(text == NULL || text[0] == '\0'))
        return;

    if(!gtk_icon_view_get_cursor(GTK_ICON_VIEW(fv->view), &cursor_path, NULL))
        return;
    if(!gtk_tree_model_get_iter(model, &it, cursor_path))
    {
        gtk_tree_path_free(cursor_path);
        return;
    }
    gtk_tree_path_free(cursor_path);

    casefold = g_utf8_casefold(text, -1);
    key = g_utf8_normalize(casefold, -1, G_NORMALIZE_ALL);
    g_free(casefold);

    if(move_up)
    {
        while(!found && gtk_tree_model_iter_previous(model, &it))
        {
            gtk_tree_model_get(model, &it, FM_FOLDER_MODEL_COL_NAME, &name, -1);
            found = fm_standard_view_search_name_matches(name, key);
            g_free(name);
        }
    }
    else
    {
        while(!found && gtk_tree_model_iter_next(model, &it))
        {
            gtk_tree_model_get(model, &it, FM_FOLDER_MODEL_COL_NAME, &name, -1);
            found = fm_standard_view_search_name_matches(name, key);
            g_free(name);
        }
    }
    g_free(key);

    if(found)
    {
        GtkTreePath* path = gtk_tree_model_get_path(model, &it);
        fm_standard_view_focus_and_select_path(fv, path);
        gtk_tree_path_free(path);
    }
}

static gboolean on_search_scroll_event(GtkWidget* widget, GdkEventScroll* evt, FmStandardView* fv)
{
    if(evt->direction == GDK_SCROLL_UP)
        fm_standard_view_search_move(fv, TRUE);
    else if(evt->direction == GDK_SCROLL_DOWN)
        fm_standard_view_search_move(fv, FALSE);
    else
        return FALSE;
    return TRUE;
}

static gboolean on_search_key_press_event(GtkWidget* widget, GdkEventKey* evt, FmStandardView* fv)
{
    if(evt->keyval == GDK_KEY_Escape || evt->keyval == GDK_KEY_Tab)
        fm_standard_view_search_dialog_hide(fv);
    else if(evt->keyval == GDK_KEY_Up || evt->keyval == GDK_KEY_KP_Up)
        fm_standard_view_search_move(fv, TRUE);
    else if((evt->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
            && (evt->keyval == GDK_KEY_g || evt->keyval == GDK_KEY_G))
        fm_standard_view_search_move(fv, TRUE);
    else if(evt->keyval == GDK_KEY_Down || evt->keyval == GDK_KEY_KP_Down)
        fm_standard_view_search_move(fv, FALSE);
    else if((evt->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_CONTROL_MASK
            && (evt->keyval == GDK_KEY_g || evt->keyval == GDK_KEY_G))
        fm_standard_view_search_move(fv, FALSE);
    else
        return FALSE;

    fm_standard_view_search_update_timeout(fv);
    return TRUE;
}

static gboolean on_search_button_press_event(GtkWidget* widget, GdkEventButton* evt, FmStandardView* fv)
{
    fm_standard_view_search_dialog_hide(fv);
    return TRUE;
}

static void on_search_activate(GtkEntry* entry, FmStandardView* fv)
{
    GtkTreePath* path;

    fm_standard_view_search_dialog_hide(fv);

    if(gtk_icon_view_get_cursor(GTK_ICON_VIEW(fv->view), &path, NULL))
    {
        if(gtk_icon_view_path_is_selected(GTK_ICON_VIEW(fv->view), path))
            fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), path, FM_FV_ACTIVATED, 0, -1, -1);
        gtk_tree_path_free(path);
    }
}

static void on_search_preedit_changed(GtkEntry* entry, gchar* preedit, FmStandardView* fv)
{
    fv->search_imcontext_changed = TRUE;
    fm_standard_view_search_update_timeout(fv);
}

static void fm_standard_view_search_position(FmStandardView* fv)
{
    GtkWidget* view = fv->view;
    GtkRequisition requisition;
    GdkRectangle monitor;
    GdkWindow* view_window = gtk_widget_get_window(view);
    GdkDisplay* display;
    GdkMonitor* mon;
    gint view_width, view_height;
    gint view_x, view_y;
    gint x, y;

    if(view_window == NULL)
        return;

    gtk_widget_realize(fv->search_window);

    display = gtk_widget_get_display(view);
    mon = gdk_display_get_monitor_at_window(display, view_window);
    gdk_monitor_get_geometry(mon, &monitor);

    gdk_window_get_origin(view_window, &view_x, &view_y);
    view_width = gdk_window_get_width(view_window);
    view_height = gdk_window_get_height(view_window);
    gtk_widget_get_preferred_size(fv->search_window, NULL, &requisition);

    if(view_x + view_width > monitor.x + monitor.width)
        x = monitor.x + monitor.width - requisition.width;
    else if(view_x + view_width - requisition.width < monitor.x)
        x = monitor.x;
    else
        x = view_x + view_width - requisition.width;

    if(view_y + view_height + requisition.height > monitor.y + monitor.height)
        y = monitor.y + monitor.height - requisition.height;
    else if(view_y + view_height < monitor.y)
        y = monitor.y;
    else
        y = view_y + view_height;

    gtk_window_move(GTK_WINDOW(fv->search_window), x, y);
}

static void fm_standard_view_search_init(GtkWidget* search_entry, FmStandardView* fv)
{
    GtkTreeModel* model;
    GtkTreeIter it;
    const gchar* text;
    gchar* casefold, *key, *name;
    gboolean found = FALSE;

    if(!fv->model)
        return;
    model = GTK_TREE_MODEL(fv->model);

    fm_standard_view_search_update_timeout(fv);

    text = gtk_entry_get_text(GTK_ENTRY(fv->search_entry));
    if(G_UNLIKELY(text == NULL || text[0] == '\0'))
        return;

    gtk_icon_view_unselect_all(GTK_ICON_VIEW(fv->view));

    casefold = g_utf8_casefold(text, -1);
    key = g_utf8_normalize(casefold, -1, G_NORMALIZE_ALL);
    g_free(casefold);

    if(gtk_tree_model_get_iter_first(model, &it)) do
    {
        gtk_tree_model_get(model, &it, FM_FOLDER_MODEL_COL_NAME, &name, -1);
        found = fm_standard_view_search_name_matches(name, key);
        g_free(name);
    }
    while(!found && gtk_tree_model_iter_next(model, &it));
    g_free(key);

    if(found)
    {
        GtkTreePath* path = gtk_tree_model_get_path(model, &it);
        fm_standard_view_focus_and_select_path(fv, path);
        gtk_tree_path_free(path);
    }
}

static void fm_standard_view_search_ensure_window(FmStandardView* fv)
{
    GtkWidget* toplevel;
    GtkWindow* window;
    GtkWindowGroup* group;
    GtkWidget* frame;
    GtkWidget* vbox;

    if(G_LIKELY(fv->search_window != NULL))
        return;

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fv));

    fv->search_window = gtk_window_new(GTK_WINDOW_POPUP);
    window = GTK_WINDOW(fv->search_window);
    gtk_window_set_type_hint(window, GDK_WINDOW_TYPE_HINT_UTILITY);
    if(GTK_IS_WINDOW(toplevel) && (group = gtk_window_get_group(GTK_WINDOW(toplevel))) != NULL)
        gtk_window_group_add_window(group, window);
    gtk_window_set_modal(window, TRUE);
    gtk_window_set_screen(window, gtk_widget_get_screen(GTK_WIDGET(fv)));
    if(GTK_IS_WINDOW(toplevel))
        gtk_window_set_transient_for(window, GTK_WINDOW(toplevel));

    g_signal_connect(window, "delete-event", G_CALLBACK(on_search_delete_event), fv);
    g_signal_connect(window, "scroll-event", G_CALLBACK(on_search_scroll_event), fv);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_search_key_press_event), fv);
    g_signal_connect(window, "button-press-event", G_CALLBACK(on_search_button_press_event), fv);

    frame = g_object_new(GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_ETCHED_IN, NULL);
    gtk_container_add(GTK_CONTAINER(window), frame);
    gtk_widget_show(frame);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);
    gtk_container_add(GTK_CONTAINER(frame), vbox);
    gtk_widget_show(vbox);

    fv->search_entry = gtk_entry_new();
    g_signal_connect(fv->search_entry, "activate", G_CALLBACK(on_search_activate), fv);
    g_signal_connect(fv->search_entry, "preedit-changed", G_CALLBACK(on_search_preedit_changed), fv);
    gtk_box_pack_start(GTK_BOX(vbox), fv->search_entry, TRUE, TRUE, 0);
    gtk_widget_realize(fv->search_entry);
    gtk_widget_show(fv->search_entry);
}

static gboolean fm_standard_view_search_start(FmStandardView* fv)
{
    GTypeClass* klass;

    fm_standard_view_search_ensure_window(fv);
    fm_standard_view_search_position(fv);
    gtk_widget_show(fv->search_window);

    if(G_UNLIKELY(fv->search_entry_changed_id == 0))
        fv->search_entry_changed_id = g_signal_connect(fv->search_entry, "changed",
                                                       G_CALLBACK(fm_standard_view_search_init), fv);

    fv->search_timeout_id = gdk_threads_add_timeout_full(G_PRIORITY_LOW,
                                                         FM_STANDARD_VIEW_SEARCH_DIALOG_TIMEOUT,
                                                         on_search_timeout, fv,
                                                         on_search_timeout_destroy);

    /* grab focus without triggering the entry's usual select-all-on-focus,
       matching how GtkTreeView's own interactive search does this */
    klass = g_type_class_peek_parent(GTK_ENTRY_GET_CLASS(fv->search_entry));
    (*GTK_WIDGET_CLASS(klass)->grab_focus)(fv->search_entry);
    fm_standard_view_send_focus_change(fv->search_entry, TRUE);

    fm_standard_view_search_init(fv->search_entry, fv);

    return TRUE;
}

static gboolean on_icon_view_key_press(GtkWidget* view, GdkEventKey* evt, FmStandardView* fv)
{
    GdkEvent* new_event;
    gboolean retval;
    gulong popup_menu_id;
    gchar* old_text;
    gchar* new_text;

    /* 'space' keypress should not start search even if there is no selection */
    if(G_UNLIKELY(evt->keyval == GDK_KEY_space))
        return FALSE;

    fm_standard_view_search_ensure_window(fv);
    gtk_widget_realize(fv->search_window);

    old_text = gtk_editable_get_chars(GTK_EDITABLE(fv->search_entry), 0, -1);

    /* prevent an accidental context menu popup while probing the event */
    popup_menu_id = g_signal_connect(fv->search_entry, "popup-menu", G_CALLBACK(gtk_true), NULL);

    /* move the search window off-screen while we probe the keypress */
    gtk_window_move(GTK_WINDOW(fv->search_window), G_MAXINT, G_MAXINT);
    gtk_widget_show(fv->search_window);

    new_event = gdk_event_copy((GdkEvent*)evt);
    g_object_unref(((GdkEventKey*)new_event)->window);
    ((GdkEventKey*)new_event)->window = GDK_WINDOW(g_object_ref(gtk_widget_get_window(fv->search_entry)));

    fv->search_imcontext_changed = FALSE;
    retval = gtk_widget_event(fv->search_entry, new_event);
    gtk_widget_hide(fv->search_window);
    gdk_event_free(new_event);

    g_signal_handler_disconnect(fv->search_entry, popup_menu_id);

    new_text = gtk_editable_get_chars(GTK_EDITABLE(fv->search_entry), 0, -1);
    retval = retval && (g_strcmp0(new_text, old_text) != 0);
    g_free(old_text);
    g_free(new_text);

    if(fv->search_imcontext_changed || retval)
    {
        if(fm_standard_view_search_start(fv))
        {
            gtk_widget_grab_focus(view);
            return TRUE;
        }
        gtk_entry_set_text(GTK_ENTRY(fv->search_entry), "");
    }
    return FALSE;
}

static gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmStandardView* fv)
{
    GList* sels = NULL;
    FmFolderViewClickType type = 0;
    GtkTreePath* tp = NULL;

    if(!fv->model)
        return FALSE;

    if(evt->type == GDK_2BUTTON_PRESS || evt->type == GDK_3BUTTON_PRESS)
    {
        /* a fast double/triple click must never trigger the delayed rename */
        fm_standard_view_cancel_pending_rename(fv);
        return FALSE;
    }

    /* FIXME: handle single click activation */
    if( evt->type == GDK_BUTTON_PRESS )
    {
        if(evt->button == 1)
        {
            fm_standard_view_cancel_pending_rename(fv);
            if(fv->mode != FM_FV_LIST_VIEW)
            {
                GtkTreePath* click_tp = NULL;
                GtkCellRenderer* cell = NULL;

                if(!fm_config->single_click &&
                   gtk_icon_view_get_item_at_pos(GTK_ICON_VIEW(view), evt->x, evt->y, &click_tp, &cell) &&
                   cell == (GtkCellRenderer*)fv->renderer_text &&
                   gtk_icon_view_path_is_selected(GTK_ICON_VIEW(view), click_tp))
                    fv->pending_rename_path = click_tp;
                else if(click_tp)
                    gtk_tree_path_free(click_tp);
            }
            else if(evt->window == gtk_tree_view_get_bin_window(GTK_TREE_VIEW(view)))
            {
                GtkTreePath* click_tp = NULL;
                GtkTreeViewColumn* click_col = NULL;
                GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

                if(!fm_config->single_click &&
                   gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view), evt->x, evt->y, &click_tp, &click_col, NULL, NULL) &&
                   click_col == fv->activable_column &&
                   gtk_tree_selection_path_is_selected(tree_sel, click_tp))
                    fv->pending_rename_path = click_tp;
                else if(click_tp)
                    gtk_tree_path_free(click_tp);
            }
        }

        /* special handling for GtkIconView */
        if(evt->button != 1)
        {
            switch(fv->mode)
            {
            case FM_FV_ICON_VIEW:
            case FM_FV_COMPACT_VIEW:
            case FM_FV_THUMBNAIL_VIEW:
            case FM_FV_ICON_OR_THUMB_VIEW:
                /* select the item on right click for GtkIconView */
                if(gtk_icon_view_get_item_at_pos(GTK_ICON_VIEW(view), evt->x, evt->y, &tp, NULL))
                {
                    /* if the hit item is not currently selected */
                    if(!gtk_icon_view_path_is_selected(GTK_ICON_VIEW(view), tp))
                    {
                        sels = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(view));
                        if( sels ) /* if there are selected items */
                        {
                            gtk_icon_view_unselect_all(GTK_ICON_VIEW(view)); /* unselect all items */
                            g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
                            g_list_free(sels);
                        }
                        gtk_icon_view_select_path(GTK_ICON_VIEW(view), tp);
                        gtk_icon_view_set_cursor(GTK_ICON_VIEW(view), tp, NULL, FALSE);
                    }
                    /* GtkIconView has no built-in middle-click activation
                       property, unlike ExoIconView's set_middle_click */
                    if(fm_config->middle_click && evt->button == 2)
                        fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), tp, FM_FV_ACTIVATED, 0, -1, -1);
                }
                break;
            case FM_FV_LIST_VIEW:
              if(evt->window == gtk_tree_view_get_bin_window(GTK_TREE_VIEW(view)))
              {
                /* Fix #2986834: MAJOR PROBLEM: Deletes Wrong File Frequently. */
                GtkTreeViewColumn* col;
                if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view), evt->x, evt->y, &tp, &col, NULL, NULL))
                {
                    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
                    if(!gtk_tree_selection_path_is_selected(tree_sel, tp))
                    {
                        gtk_tree_selection_unselect_all(tree_sel);
                        if(col == fv->activable_column)
                        {
                            gtk_tree_selection_select_path(tree_sel, tp);
                            gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), tp, NULL, FALSE);
                        }
                    }
                    if(fm_config->middle_click && evt->button == 2)
                      fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), tp, FM_FV_ACTIVATED, 0, -1, -1);
                }
              }
            }
        }

        if(evt->button == 2 && !fm_config->middle_click)  /* middle click */
            type = FM_FV_MIDDLE_CLICK;
        else if(evt->button == 3) /* right click */
            type = FM_FV_CONTEXT_MENU;
    }

    if( type != FM_FV_CLICK_NONE )
    {
        sels = fm_standard_view_get_selected_tree_paths(fv);
        if( sels || type == FM_FV_CONTEXT_MENU )
        {
            fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), tp, type, 0, -1, -1);
            if(sels)
            {
                g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
                g_list_free(sels);
            }
        }
    }
    if(tp)
        gtk_tree_path_free(tp);
    return FALSE;
}

/* TODO: select files by custom func, not yet implemented */
void fm_folder_view_select_custom(FmFolderView* fv, GFunc filter, gpointer user_data)
{
}

static void fm_standard_view_select_all(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    if(fv->select_all)
        fv->select_all(fv->view);
}

static void fm_standard_view_unselect_all(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    if(fv->unselect_all)
        fv->unselect_all(fv->view);
}

static void on_dnd_src_data_get(FmDndSrc* ds, FmStandardView* fv)
{
    FmFileInfoList* files = fm_standard_view_dup_selected_files(FM_FOLDER_VIEW(fv));
    fm_dnd_src_set_files(ds, files);
    if(files)
        fm_file_info_list_unref(files);
}

static gboolean on_sel_changed_real(FmStandardView* fv)
{
    /* clear cached selected files */
    if(fv->cached_selected_files)
    {
        fm_file_info_list_unref(fv->cached_selected_files);
        fv->cached_selected_files = NULL;
    }
    if(fv->cached_selected_file_paths)
    {
        fm_path_list_unref(fv->cached_selected_file_paths);
        fv->cached_selected_file_paths = NULL;
    }
    fm_folder_view_sel_changed(NULL, FM_FOLDER_VIEW(fv));
    fv->sel_changed_pending = FALSE;
    return TRUE;
}

/*
 * We limit "sel-changed" emitting here:
 * - if no signal was in last 200ms then signal is emitted immidiately
 * - if there was < 200ms since last signal then it's marked as pending
 *   and signal will be emitted when that 200ms timeout ends
 */
static gboolean on_sel_changed_idle(gpointer user_data)
{
    FmStandardView* fv = (FmStandardView*)user_data;
    gboolean ret = FALSE;

    /* check if fv is destroyed already */
    if(g_source_is_destroyed(g_main_current_source()))
        goto _end;
    if(fv->sel_changed_pending) /* fast changing detected! continue... */
        ret = on_sel_changed_real(fv);
    fv->sel_changed_idle = 0;
_end:
    return ret;
}

static void on_sel_changed(GObject* obj, FmStandardView* fv)
{
    if(!fv->sel_changed_idle)
    {
        fv->sel_changed_idle = gdk_threads_add_timeout_full(G_PRIORITY_HIGH_IDLE, 200,
                                                            on_sel_changed_idle,
                                                            fv, NULL);
        on_sel_changed_real(fv);
    }
    else
        fv->sel_changed_pending = TRUE;
}

static void fm_standard_view_select_invert(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    if(fv->select_invert)
        fv->select_invert(fv->model, fv->view);
}

static FmFolder* fm_standard_view_get_folder(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    return fv->model ? fm_folder_model_get_folder(fv->model) : NULL;
}

static void fm_standard_view_select_file_path(FmFolderView* ffv, FmPath* path)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    FmFolder* folder = fm_standard_view_get_folder(ffv);
    FmPath* cwd = folder ? fm_folder_get_path(folder) : NULL;
    if(cwd && fm_path_equal(fm_path_get_parent(path), cwd))
    {
        FmFolderModel* model = fv->model;
        GtkTreeIter it;
        if(fv->select_path &&
           fm_folder_model_find_iter_by_filename(model, &it, fm_path_get_basename(path)))
            fv->select_path(model, fv->view, &it);
    }
}

static void fm_standard_view_get_custom_menu_callbacks(FmFolderView* ffv,
        FmFolderViewUpdatePopup *update_popup, FmLaunchFolderFunc *open_folders)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    *update_popup = fv->update_popup;
    *open_folders = fv->open_folders;
}

static FmFolderModel* fm_standard_view_get_model(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    return fv->model;
}

static void fm_standard_view_set_model(FmFolderView* ffv, FmFolderModel* model)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    int icon_size;
    unset_model(fv);
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        _check_tree_columns_defaults(fv);
        if(model)
        {
            icon_size = fm_config->small_icon_size;
            fm_folder_model_set_icon_size(model, icon_size);
            fm_folder_model_show_thumbnails (model, FALSE);
        }
        gtk_tree_view_set_model(GTK_TREE_VIEW(fv->view), GTK_TREE_MODEL(model));
        _reset_columns_widths(GTK_TREE_VIEW(fv->view));
        break;
    case FM_FV_ICON_VIEW:
        icon_size = fm_config->big_icon_size;
        if(model)
        {
            fm_folder_model_set_icon_size(model, icon_size);
            fm_folder_model_show_thumbnails (model, FALSE);
        }
        gtk_icon_view_set_model(GTK_ICON_VIEW(fv->view), GTK_TREE_MODEL(model));
        break;
    case FM_FV_ICON_OR_THUMB_VIEW:
        icon_size = fm_config->big_icon_size;
        if(model)
        {
            fm_folder_model_set_icon_size(model, icon_size);
            fm_folder_model_show_thumbnails (model, fv->show_thumbs);
        }
        gtk_icon_view_set_model(GTK_ICON_VIEW(fv->view), GTK_TREE_MODEL(model));
        break;
    case FM_FV_COMPACT_VIEW:
        if(model)
        {
            icon_size = fm_config->small_icon_size;
            fm_folder_model_set_icon_size(model, icon_size);
            fm_folder_model_show_thumbnails (model, FALSE);
        }
        gtk_icon_view_set_model(GTK_ICON_VIEW(fv->view), GTK_TREE_MODEL(model));
        break;
    case FM_FV_THUMBNAIL_VIEW:
        if(model)
        {
            icon_size = fm_config->thumbnail_size;
            fm_folder_model_set_icon_size(model, icon_size);
            fm_folder_model_show_thumbnails (model, TRUE);
        }
        gtk_icon_view_set_model(GTK_ICON_VIEW(fv->view), GTK_TREE_MODEL(model));
        break;
    }

    if(model)
    {
        fv->model = (FmFolderModel*)g_object_ref(model);
        g_signal_connect(model, "row-inserted", G_CALLBACK(on_row_inserted), fv);
        g_signal_connect(model, "row-deleted", G_CALLBACK(on_row_deleted), fv);
        g_signal_connect(model, "row-changed", G_CALLBACK(on_row_changed), fv);
    }
    else
        fv->model = NULL;
    /* reset tooltip after changing folder, it might stick from old one,
       see how FmCellRendererText works on that regard */
    g_object_set(G_OBJECT(fv->view), "tooltip-text", NULL, NULL);
}

typedef struct
{
    GtkTreeViewColumn* col;
    FmFolderViewColumnInfo* info;
} _ColumnsCache;

static gboolean _fm_standard_view_set_columns(FmFolderView* fv, const GSList* cols)
{
    FmStandardView* view;
    GtkTreeViewColumn *col, *last;
    FmFolderViewColumnInfo* info;
    _ColumnsCache* old_cols = NULL; /* satisfy the compiler */
    const GSList* l;
    GList *cols_list, *ld;
    guint i, n_cols;

    if(!FM_IS_STANDARD_VIEW(fv))
        return FALSE;
    view = (FmStandardView*)fv;

    if(view->mode != FM_FV_LIST_VIEW) /* other modes aren't supported now */
        return FALSE;

    cols_list = gtk_tree_view_get_columns(GTK_TREE_VIEW(view->view));
    n_cols = g_list_length(cols_list);
    if(n_cols > 0)
    {
        /* create more convenient for us list of columns */
        old_cols = g_new(_ColumnsCache, n_cols);
        for(ld = cols_list, i = 0; ld; ld = ld->next, i++)
        {
            col = ld->data; /* column */
            info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id); /* info */
            old_cols[i].col = col;
            old_cols[i].info = info;
        }
        g_list_free(cols_list);
    }
    last = NULL;
    for(l = cols; l; l = l->next)
    {
        info = l->data;
        /* find old one and move here */
        for(i = 0; i < n_cols; i++)
            if(old_cols[i].info && old_cols[i].info->col_id == info->col_id)
                break;
        if(i < n_cols)
        {
            /* we found it so just move it here */
            col = old_cols[i].col;
            /* update all other data - width for example */
            if(info->col_id != FM_FOLDER_MODEL_COL_NAME)
            {
                old_cols[i].info->width = info->width;
                if(info->width < 0)
                    old_cols[i].info->width = fm_folder_model_col_get_default_width(view->model, info->col_id);
                old_cols[i].info->reserved1 = 0;
                _update_width_sizing(col, info->width);
            }
            old_cols[i].col = NULL; /* we removed it from its place */
            old_cols[i].info = NULL; /* don't try to use it again */
        }
        else if(!fm_folder_model_col_is_valid(0))
            /* workaround for case when there is no model init yet, the most
               probably bug #3596550 is about this (it creates column with
               empty title), can g_return_val_if_fail() not fail somehow? */
            continue;
        else
        {
            /* if not found then append new one */
            col = create_list_view_column(view, info);
            if(col == NULL) /* failed! skipping it */
                continue;
        }
        gtk_tree_view_move_column_after(GTK_TREE_VIEW(view->view), col, last);
        last = col;
    }

    /* remove abandoned columns from view */
    for(i = 0; i < n_cols; i++)
        if(old_cols[i].col != NULL)
            gtk_tree_view_remove_column(GTK_TREE_VIEW(view->view),
                                        old_cols[i].col);
    if(n_cols > 0)
        g_free(old_cols);
    return TRUE;
}

static GSList* _fm_standard_view_get_columns(FmFolderView* fv)
{
    FmStandardView* view;
    GSList* list;
    GList *cols_list, *ld;

    if(!FM_IS_STANDARD_VIEW(fv))
        return NULL;
    view = (FmStandardView*)fv;

    if(view->mode != FM_FV_LIST_VIEW) /* other modes aren't supported now */
        return NULL;

    cols_list = gtk_tree_view_get_columns(GTK_TREE_VIEW(view->view));
    if(cols_list == NULL)
        return NULL;
    list = NULL;
    for(ld = cols_list; ld; ld = ld->next)
    {
        GtkTreeViewColumn *col = ld->data;
        FmFolderViewColumnInfo* info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
        list = g_slist_append(list, info); /* info */
    }
    g_list_free(cols_list);
    return list;
}

static void _fm_standard_view_scroll_to_path(FmFolderView* fv, FmPath *path, gboolean focus)
{
    FmStandardView *view;
    GtkTreeIter it;
    GtkTreePath *tp;

    if (!FM_IS_STANDARD_VIEW(fv) || path == NULL)
        return;
    view = (FmStandardView*)fv;
    if (!fm_folder_model_find_iter_by_filename(view->model, &it,
                                               fm_path_get_basename(path)))
        return;
    tp = gtk_tree_model_get_path(GTK_TREE_MODEL(view->model), &it);
    if (tp == NULL) /* invalid child */
        return;
    switch(view->mode)
    {
    case FM_FV_LIST_VIEW:
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(view->view), tp, NULL, TRUE, 0.5, 0.0);
        if (focus)
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(view->view), tp, NULL, FALSE);
        break;
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
    case FM_FV_ICON_OR_THUMB_VIEW:
        gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(view->view), tp, TRUE, 0.5, 0.5);
        if (focus)
            gtk_icon_view_set_cursor(GTK_ICON_VIEW(view->view), tp, NULL, FALSE);
        break;
    }
    gtk_tree_path_free(tp);
}

static void fm_standard_view_view_init(FmFolderViewInterface* iface)
{
    iface->set_sel_mode = fm_standard_view_set_selection_mode;
    iface->get_sel_mode = fm_standard_view_get_selection_mode;
    iface->set_show_hidden = fm_standard_view_set_show_hidden;
    iface->get_show_hidden = fm_standard_view_get_show_hidden;
    iface->get_folder = fm_standard_view_get_folder;
    iface->set_model = fm_standard_view_set_model;
    iface->get_model = fm_standard_view_get_model;
    iface->count_selected_files = fm_standard_view_count_selected_files;
    iface->dup_selected_files = fm_standard_view_dup_selected_files;
    iface->dup_selected_file_paths = fm_standard_view_dup_selected_file_paths;
    iface->select_all = fm_standard_view_select_all;
    iface->unselect_all = fm_standard_view_unselect_all;
    iface->select_invert = fm_standard_view_select_invert;
    iface->select_file_path = fm_standard_view_select_file_path;
    iface->get_custom_menu_callbacks = fm_standard_view_get_custom_menu_callbacks;
    iface->set_columns = _fm_standard_view_set_columns;
    iface->get_columns = _fm_standard_view_get_columns;
    iface->scroll_to_path = _fm_standard_view_scroll_to_path;
}

typedef struct
{
    const char* name;
    FmStandardViewMode mode;
    char *icon;
    char *label;
    char *tooltip;
    //char *shortkey;
} FmStandardViewModeinfo;

static const FmStandardViewModeinfo view_mode_names[] =
{
    { "icon", FM_FV_ICON_VIEW, NULL, N_("_Icon View"), NULL },
    { "compact", FM_FV_COMPACT_VIEW, NULL, N_("_Compact View"), NULL },
    { "thumbnail", FM_FV_THUMBNAIL_VIEW, NULL, N_("_Thumbnail View"), NULL },
    { "list", FM_FV_LIST_VIEW, NULL, N_("Detailed _List View"), NULL },
    { "icon_thumb", FM_FV_ICON_OR_THUMB_VIEW, NULL, N_("_Icon or Thumb View"), NULL },
};

/**
 * fm_standard_view_mode_to_str
 * @mode: mode id
 *
 * Retrieves string name of rendering @mode. That name may be used for
 * config save or similar purposes. Returned data are owned by the
 * implementation and should be not freed by caller.
 *
 * Returns: name associated with @mode.
 *
 * Since: 1.0.2
 */
const char* fm_standard_view_mode_to_str(FmStandardViewMode mode)
{
    guint i;

    if(G_LIKELY(FM_STANDARD_VIEW_MODE_IS_VALID(mode)))
        for(i = 0; i < G_N_ELEMENTS(view_mode_names); i++)
            if(view_mode_names[i].mode == mode)
                return view_mode_names[i].name;
    return NULL;
}

/**
 * fm_standard_view_mode_from_str
 * @str: the name of mode
 *
 * Finds mode which have an associated name equal to @str.
 *
 * Returns: mode id or (FmStandardViewMode)-1 if no such mode exists.
 *
 * Since: 1.0.2
 */
FmStandardViewMode fm_standard_view_mode_from_str(const char* str)
{
    guint i;

    for(i = 0; i < G_N_ELEMENTS(view_mode_names); i++)
        if(strcmp(str, view_mode_names[i].name) == 0)
            return view_mode_names[i].mode;
    return (FmStandardViewMode)-1;
}

/**
* fm_standard_view_get_n_modes
*
* Tests how many view modes are known to create #FmStandardView widget.
*
* Returns: number of known modes for standard folder view.
*
* Since: 1.2.0
*/
gint fm_standard_view_get_n_modes(void)
{
    /* FIXME: this is rough */
    return (gint)FM_FV_LIST_VIEW + 1;
}

/**
* fm_standard_view_get_mode_label
* @mode: the view mode
*
* Retrieves label for @mode which can be used in menus. Returned
* data should not be freed by caller.
*
* Returns: desription or %NULL if @mode is invalid.
*
* Since: 1.2.0
*/
const char *fm_standard_view_get_mode_label(FmStandardViewMode mode)
{
    guint i;

    if(G_LIKELY(FM_STANDARD_VIEW_MODE_IS_VALID(mode)))
        for(i = 0; i < G_N_ELEMENTS(view_mode_names); i++)
            if(view_mode_names[i].mode == mode && view_mode_names[i].label)
                return _(view_mode_names[i].label);
    return NULL;
}

/**
* fm_standard_view_get_mode_tooltip
* @mode: the view mode
*
* Retrieves detailed description for @mode which can be used in tooltip.
* Returned data should not be freed by caller.
*
* Returns: detailed description or %NULL if it is not available.
*
* Since: 1.2.0
*/
const char *fm_standard_view_get_mode_tooltip(FmStandardViewMode mode)
{
    guint i;

    if(G_LIKELY(FM_STANDARD_VIEW_MODE_IS_VALID(mode)))
        for(i = 0; i < G_N_ELEMENTS(view_mode_names); i++)
            if(view_mode_names[i].mode == mode && view_mode_names[i].tooltip)
                return _(view_mode_names[i].tooltip);
    return NULL;
}

/**
* fm_standard_view_get_mode_icon
* @mode: the view mode
*
* Retrieves icon name for @mode which can be used in menus. Returned
* data should not be freed by caller.
*
* Returns: icon name or %NULL if it is not available.
*
* Since: 1.2.0
*/
const char *fm_standard_view_get_mode_icon(FmStandardViewMode mode)
{
    guint i;

    if(G_LIKELY(FM_STANDARD_VIEW_MODE_IS_VALID(mode)))
        for(i = 0; i < G_N_ELEMENTS(view_mode_names); i++)
            if(view_mode_names[i].mode == mode)
                return view_mode_names[i].icon;
    return NULL;
}

void fm_standard_view_set_thumbs(FmStandardView* fv, gboolean thumbs)
{
    fv->show_thumbs = thumbs;
}

gboolean fm_standard_view_get_thumbs(FmStandardView* fv)
{
    return fv->show_thumbs;
}
