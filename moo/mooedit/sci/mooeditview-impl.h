#pragma once

#ifndef MOO_USE_SCI
#error "This header must not be used"
#endif

#include "mooedit/sci/mooeditview.h"

MooEditView    *_moo_edit_view_new                      (MooEdit        *doc);
void            _moo_edit_view_unset_doc                (MooEditView    *view);
void            _moo_edit_view_set_tab                  (MooEditView    *view,
                                                         MooEditTab     *tab);

//GtkTextMark    *_moo_edit_view_get_fake_cursor_mark     (MooEditView    *view);

void            _moo_edit_view_apply_prefs              (MooEditView    *view);
void            _moo_edit_view_apply_config             (MooEditView    *view);

void            _moo_edit_view_ui_set_line_wrap         (MooEditView    *view,
                                                         gboolean        enabled);
void            _moo_edit_view_ui_set_show_line_numbers (MooEditView    *view,
                                                         gboolean        show);

//void            _moo_edit_view_do_popup                 (MooEditView    *view,
//                                                         GdkEventButton *event);
