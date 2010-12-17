#include <errno.h>
#include <mooedit/mooeditfileinfo.h>

static MooEditOpenInfo *
parse_filename (const char *filename)
{
    char *freeme2 = NULL;
    char *freeme1 = NULL;
    char *uri;
    int line = 0;
    MooEditOpenInfo *info;

    freeme1 = _moo_normalize_file_path (filename);
    filename = freeme1;

    if (g_str_has_suffix (filename, "/") ||
#ifdef G_OS_WIN32
        g_str_has_suffix (filename, "\\") ||
#endif
        g_file_test (filename, G_FILE_TEST_IS_DIR))
    {
        g_free (freeme1);
        g_free (freeme2);
        return NULL;
    }

    if (g_utf8_validate (filename, -1, NULL))
    {
        GError *error = NULL;
        GRegex *re = g_regex_new ("((?P<path>.*):(?P<line>\\d+)?|(?P<path>.*)\\((?P<line>\\d+)\\))$",
                                  G_REGEX_OPTIMIZE | G_REGEX_DUPNAMES, 0, &error);
        if (!re)
        {
            moo_critical ("could not compile regex: %s", error->message);
            g_error_free (error);
        }
        else
        {
            GMatchInfo *match_info = NULL;

            if (g_regex_match (re, filename, 0, &match_info))
            {
                char *path = g_match_info_fetch_named (match_info, "path");
                char *line_string = g_match_info_fetch_named (match_info, "line");
                if (path && *path)
                {
                    filename = path;
                    freeme2 = path;
                    path = NULL;
                    if (line_string && *line_string)
                    {
                        errno = 0;
                        line = strtol (line_string, NULL, 10);
                        if (errno)
                            line = 0;
                    }
                }
                g_free (line_string);
                g_free (path);
            }

            g_match_info_free (match_info);
            g_regex_unref (re);
        }
    }

    if (!(uri = g_filename_to_uri (filename, NULL, NULL)))
    {
        g_critical ("could not convert filename to URI");
        g_free (freeme1);
        g_free (freeme2);
        return NULL;
    }

    info = moo_edit_open_info_new_uri (uri, NULL);

    info->line = line - 1;

    g_free (uri);
    g_free (freeme1);
    g_free (freeme2);
    return info;
}

static void
parse_options_from_uri (const char      *optstring,
                        MooEditOpenInfo *info)
{
    char **p, **comps;

    comps = g_strsplit (optstring, ";", 0);

    for (p = comps; p && *p; ++p)
    {
        if (!strncmp (*p, "line=", strlen ("line=")))
        {
            /* doesn't matter if there is an error */
            info->line = strtoul (*p + strlen ("line="), NULL, 10) - 1;
        }
        else if (!strncmp (*p, "options=", strlen ("options=")))
        {
            char **opts, **op;
            opts = g_strsplit (*p + strlen ("options="), ",", 0);
            for (op = opts; op && *op; ++op)
            {
                if (!strcmp (*op, "new-window"))
                    info->flags |= MOO_EDIT_OPEN_NEW_WINDOW;
                else if (!strcmp (*op, "new-tab"))
                    info->flags |= MOO_EDIT_OPEN_NEW_TAB;
            }
            g_strfreev (opts);
        }
    }

    g_strfreev (comps);
}

static MooEditOpenInfo *
parse_uri (const char *scheme,
           const char *uri)
{
    const char *question_mark;
    const char *optstring = NULL;
    MooEditOpenInfo *info;
    char *real_uri;

    if (strcmp (scheme, "file") != 0)
        return moo_edit_open_info_new_uri (uri, NULL);

    question_mark = strchr (uri, '?');

    if (question_mark && question_mark > uri)
    {
        real_uri = g_strndup (uri, question_mark - uri);
        optstring = question_mark + 1;
    }
    else
    {
        real_uri = g_strdup (uri);
    }

    info = moo_edit_open_info_new_uri (real_uri, NULL);

    if (optstring)
        parse_options_from_uri (optstring, info);

    g_free (real_uri);
    return info;
}

static char *
parse_uri_scheme (const char *string)
{
    const char *p;

    for (p = string; *p; ++p)
    {
        if (*p == ':')
        {
            if (p != string)
                return g_strndup (string, p - string);

            break;
        }

        if (!(p != string && g_ascii_isalnum (*p)) &&
            !(p == string && g_ascii_isalpha (*p)))
                break;
    }

    return NULL;
}

static MooEditOpenInfo *
parse_file (const char  *string,
            char       **current_dir)
{
    char *uri_scheme;
    char *filename;
    MooEditOpenInfo *ret;

    if (g_path_is_absolute (string))
        return parse_filename (string);

    if ((uri_scheme = parse_uri_scheme (string)))
    {
        ret = parse_uri (uri_scheme, string);
        g_free (uri_scheme);
        return ret;
    }

    if (!*current_dir)
        *current_dir = g_get_current_dir ();

    filename = g_build_filename (*current_dir, string, NULL);
    ret = parse_filename (filename);

    g_free (filename);
    return ret;
}

static MooEditOpenInfoArray *
parse_files (void)
{
    int i;
    int n_files;
    char *current_dir = NULL;
    MooEditOpenInfoArray *files;

    if (!medit_opts.files || !(n_files = g_strv_length (medit_opts.files)))
        return NULL;

    files = moo_edit_open_info_array_new ();

    for (i = 0; i < n_files; ++i)
    {
        MooEditOpenInfo *info;

        info = parse_file (medit_opts.files[i], &current_dir);

        if (!info)
            continue;

        if (medit_opts.new_window)
            info->flags |= MOO_EDIT_OPEN_NEW_WINDOW;
        if (medit_opts.new_tab)
            info->flags |= MOO_EDIT_OPEN_NEW_TAB;
        if (medit_opts.reload)
            info->flags |= MOO_EDIT_OPEN_RELOAD;

        if (info->line < 0)
            info->line = medit_opts.line - 1;

        if (!info->encoding && medit_opts.encoding && medit_opts.encoding[0])
            info->encoding = g_strdup (medit_opts.encoding);

        moo_edit_open_info_array_take (files, info);
    }

    g_free (current_dir);
    return files;
}
