#ifndef MOO_EDIT_VIEW_PRIV_H
#define MOO_EDIT_VIEW_PRIV_H

#ifdef MOO_USE_SCI
#error "This header must not be used"
#endif

#include "mooedit/native/mooeditview-impl.h"

G_BEGIN_DECLS

struct MooEditViewPrivate
{
    MooEdit *doc;
    MooEditor *editor;
    MooEditTab *tab;
    GtkTextMark *fake_cursor_mark;
};

G_END_DECLS

#endif /* MOO_EDIT_VIEW_PRIV_H */
