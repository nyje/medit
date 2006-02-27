/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *
 *   mooscript-node.h
 *
 *   Copyright (C) 2004-2006 by Yevgen Muntyan <muntyan@math.tamu.edu>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   See COPYING file that comes with this distribution.
 */

#ifndef __MOO_SCRIPT_NODE_H__
#define __MOO_SCRIPT_NODE_H__

#include "mooscript-func.h"

G_BEGIN_DECLS


#define MS_TYPE_NODE                    (ms_node_get_type ())
#define MS_NODE(object)                 (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE, MSNode))
#define MS_NODE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE, MSNodeClass))
#define MS_IS_NODE(object)              (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE))
#define MS_IS_NODE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE))
#define MS_NODE_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE, MSNodeClass))

#define MS_TYPE_NODE_LIST               (ms_node_list_get_type ())
#define MS_NODE_LIST(object)            (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE_LIST, MSNodeList))
#define MS_NODE_LIST_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE_LIST, MSNodeListClass))
#define MS_IS_NODE_LIST(object)         (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE_LIST))
#define MS_IS_NODE_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE_LIST))
#define MS_NODE_LIST_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE_LIST, MSNodeListClass))

#define MS_TYPE_NODE_VAR                (ms_node_var_get_type ())
#define MS_NODE_VAR(object)             (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE_VAR, MSNodeVar))
#define MS_NODE_VAR_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE_VAR, MSNodeVarClass))
#define MS_IS_NODE_VAR(object)          (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE_VAR))
#define MS_IS_NODE_VAR_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE_VAR))
#define MS_NODE_VAR_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE_VAR, MSNodeVarClass))

#define MS_TYPE_NODE_COMMAND            (ms_node_command_get_type ())
#define MS_NODE_COMMAND(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE_COMMAND, MSNodeCommand))
#define MS_NODE_COMMAND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE_COMMAND, MSNodeCommandClass))
#define MS_IS_NODE_COMMAND(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE_COMMAND))
#define MS_IS_NODE_COMMAND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE_COMMAND))
#define MS_NODE_COMMAND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE_COMMAND, MSNodeCommandClass))

#define MS_TYPE_NODE_IF_ELSE            (ms_node_if_else_get_type ())
#define MS_NODE_IF_ELSE(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE_IF_ELSE, MSNodeIfElse))
#define MS_NODE_IF_ELSE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE_IF_ELSE, MSNodeIfElseClass))
#define MS_IS_NODE_IF_ELSE(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE_IF_ELSE))
#define MS_IS_NODE_IF_ELSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE_IF_ELSE))
#define MS_NODE_IF_ELSE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE_IF_ELSE, MSNodeIfElseClass))

#define MS_TYPE_NODE_WHILE              (ms_node_while_get_type ())
#define MS_NODE_WHILE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE_WHILE, MSNodeWhile))
#define MS_NODE_WHILE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE_WHILE, MSNodeWhileClass))
#define MS_IS_NODE_WHILE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE_WHILE))
#define MS_IS_NODE_WHILE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE_WHILE))
#define MS_NODE_WHILE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE_WHILE, MSNodeWhileClass))

#define MS_TYPE_NODE_FOR                (ms_node_for_get_type ())
#define MS_NODE_FOR(object)             (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE_FOR, MSNodeFor))
#define MS_NODE_FOR_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE_FOR, MSNodeForClass))
#define MS_IS_NODE_FOR(object)          (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE_FOR))
#define MS_IS_NODE_FOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE_FOR))
#define MS_NODE_FOR_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE_FOR, MSNodeForClass))

#define MS_TYPE_NODE_ASSIGN             (ms_node_assign_get_type ())
#define MS_NODE_ASSIGN(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE_ASSIGN, MSNodeAssign))
#define MS_NODE_ASSIGN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE_ASSIGN, MSNodeAssignClass))
#define MS_IS_NODE_ASSIGN(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE_ASSIGN))
#define MS_IS_NODE_ASSIGN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE_ASSIGN))
#define MS_NODE_ASSIGN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE_ASSIGN, MSNodeAssignClass))

#define MS_TYPE_NODE_VALUE              (ms_node_value_get_type ())
#define MS_NODE_VALUE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE_VALUE, MSNodeValue))
#define MS_NODE_VALUE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE_VALUE, MSNodeValueClass))
#define MS_IS_NODE_VALUE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE_VALUE))
#define MS_IS_NODE_VALUE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE_VALUE))
#define MS_NODE_VALUE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE_VALUE, MSNodeValueClass))

#define MS_TYPE_NODE_VAL_LIST            (ms_node_val_list_get_type ())
#define MS_NODE_VAL_LIST(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE_VAL_LIST, MSNodeValList))
#define MS_NODE_VAL_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE_VAL_LIST, MSNodeValListClass))
#define MS_IS_NODE_VAL_LIST(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE_VAL_LIST))
#define MS_IS_NODE_VAL_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE_VAL_LIST))
#define MS_NODE_VAL_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE_VAL_LIST, MSNodeValListClass))

#define MS_TYPE_NODE_PYTHON              (ms_node_python_get_type ())
#define MS_NODE_PYTHON(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), MS_TYPE_NODE_PYTHON, MSNodePython))
#define MS_NODE_PYTHON_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), MS_TYPE_NODE_PYTHON, MSNodePythonClass))
#define MS_IS_NODE_PYTHON(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), MS_TYPE_NODE_PYTHON))
#define MS_IS_NODE_PYTHON_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MS_TYPE_NODE_PYTHON))
#define MS_NODE_PYTHON_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_TYPE_NODE_PYTHON, MSNodePythonClass))

typedef struct _MSNode MSNode;
typedef struct _MSNodeClass MSNodeClass;
typedef struct _MSNodeList MSNodeList;
typedef struct _MSNodeListClass MSNodeListClass;
typedef struct _MSNodeVar MSNodeVar;
typedef struct _MSNodeVarClass MSNodeVarClass;
typedef struct _MSNodeCommand MSNodeCommand;
typedef struct _MSNodeCommandClass MSNodeCommandClass;
typedef struct _MSNodeIfElse MSNodeIfElse;
typedef struct _MSNodeIfElseClass MSNodeIfElseClass;
typedef struct _MSNodeWhile MSNodeWhile;
typedef struct _MSNodeWhileClass MSNodeWhileClass;
typedef struct _MSNodeFor MSNodeFor;
typedef struct _MSNodeForClass MSNodeForClass;
typedef struct _MSNodeAssign MSNodeAssign;
typedef struct _MSNodeAssignClass MSNodeAssignClass;
typedef struct _MSNodeValue MSNodeValue;
typedef struct _MSNodeValueClass MSNodeValueClass;
typedef struct _MSNodeValList MSNodeValList;
typedef struct _MSNodeValListClass MSNodeValListClass;
typedef struct _MSNodePython MSNodePython;
typedef struct _MSNodePythonClass MSNodePythonClass;


typedef MSValue* (*MSNodeEval) (MSNode *node, MSContext *ctx);


struct _MSNode {
    GObject object;
};

struct _MSNodeClass {
    GObjectClass object_class;
    MSNodeEval eval;
};


struct _MSNodeList {
    MSNode node;
    MSNode **nodes;
    guint n_nodes;
    guint n_nodes_allocd__;
};

struct _MSNodeListClass {
    MSNodeClass node_class;
};


struct _MSNodeVar {
    MSNode node;
    char *name;
};

struct _MSNodeVarClass {
    MSNodeClass node_class;
};


struct _MSNodeCommand {
    MSNode node;
    char *name;
    MSNodeList *args;
};

struct _MSNodeCommandClass {
    MSNodeClass node_class;
};


typedef enum {
    MS_COND_BEFORE,
    MS_COND_AFTER
} MSCondType;

struct _MSNodeWhile {
    MSNode node;
    MSCondType type;
    MSNode *condition;
    MSNode *what;
};

struct _MSNodeWhileClass {
    MSNodeClass node_class;
};


struct _MSNodeFor {
    MSNode node;
    MSNode *variable;
    MSNode *list;
    MSNode *what;
};

struct _MSNodeForClass {
    MSNodeClass node_class;
};


struct _MSNodeIfElse {
    MSNode node;
    MSNode *condition;
    MSNode *then_;
    MSNode *else_;
};

struct _MSNodeIfElseClass {
    MSNodeClass node_class;
};


struct _MSNodeAssign {
    MSNode node;
    MSNodeVar *var;
    MSNode *val;
};

struct _MSNodeAssignClass {
    MSNodeClass node_class;
};


struct _MSNodeValue {
    MSNode node;
    MSValue *value;
};

struct _MSNodeValueClass {
    MSNodeClass node_class;
};


struct _MSNodePython {
    MSNode node;
    char *script;
};

struct _MSNodePythonClass {
    MSNodeClass node_class;
};


typedef enum {
    MS_VAL_LIST,
    MS_VAL_RANGE
} MSValListType;

struct _MSNodeValList {
    MSNode node;
    MSValListType type;
    MSNodeList *elms;
    MSNode *first;
    MSNode *last;
};

struct _MSNodeValListClass {
    MSNodeClass node_class;
};


GType           ms_node_get_type            (void) G_GNUC_CONST;
GType           ms_node_list_get_type       (void) G_GNUC_CONST;
GType           ms_node_var_get_type        (void) G_GNUC_CONST;
GType           ms_node_command_get_type    (void) G_GNUC_CONST;
GType           ms_node_if_else_get_type    (void) G_GNUC_CONST;
GType           ms_node_while_get_type      (void) G_GNUC_CONST;
GType           ms_node_for_get_type        (void) G_GNUC_CONST;
GType           ms_node_assign_get_type     (void) G_GNUC_CONST;
GType           ms_node_val_list_get_type   (void) G_GNUC_CONST;
GType           ms_node_value_get_type      (void) G_GNUC_CONST;
GType           ms_node_python_get_type     (void) G_GNUC_CONST;

MSValue        *ms_node_eval                (MSNode     *node,
                                             MSContext  *ctx);

MSNodeList     *ms_node_list_new            (void);
void            ms_node_list_add            (MSNodeList *list,
                                             MSNode     *node);

MSNodeCommand  *ms_node_command_new         (const char *name,
                                             MSNodeList *args);
MSNodeCommand  *ms_node_binary_op_new       (MSBinaryOp  op,
                                             MSNode     *lval,
                                             MSNode     *rval);
MSNodeCommand  *ms_node_unary_op_new        (MSUnaryOp   op,
                                             MSNode     *val);

MSNodeIfElse   *ms_node_if_else_new         (MSNode     *condition,
                                             MSNode     *then_,
                                             MSNode     *else_);

MSNodeWhile    *ms_node_while_new           (MSCondType  type,
                                             MSNode     *cond,
                                             MSNode     *what);
MSNodeFor      *ms_node_for_new             (MSNode     *var,
                                             MSNode     *list,
                                             MSNode     *what);

MSNodeAssign   *ms_node_assign_new          (MSNodeVar  *var,
                                             MSNode     *val);

MSNodeValue    *ms_node_value_new           (MSValue    *value);
MSNodeValList  *ms_node_val_list_new        (MSNodeList *list);
MSNodeValList  *ms_node_val_range_new       (MSNode     *first,
                                             MSNode     *last);

MSNodeVar      *ms_node_var_new             (const char *name);

MSNodePython   *ms_node_python_new          (const char *script);


G_END_DECLS

#endif /* __MOO_SCRIPT_NODE_H__ */
