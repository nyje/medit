%{
#include "mooscript-parser-priv.h"
#include "mooscript-yacc.h"


static MSNode *
node_list_add (MSParser   *parser,
                      MSNodeList *list,
                      MSNode     *node)
{
    if (!node)
        return NULL;

    if (!list)
    {
        list = ms_node_list_new ();
        _ms_parser_add_node (parser, list);
    }

    ms_node_list_add (list, node);
    return MS_NODE (list);
}


static MSNode *
node_command (MSParser   *parser,
              const char *name,
              MSNodeList *list)
{
    MSNodeCommand *cmd;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (!list || MS_IS_NODE_LIST (list), NULL);

    cmd = ms_node_command_new (name, list);
    _ms_parser_add_node (parser, cmd);

    return MS_NODE (cmd);
}


static MSNode *
node_if_else (MSParser   *parser,
              MSNode     *condition,
              MSNode     *then_,
              MSNode     *else_)
{
    MSNodeIfElse *node;

    g_return_val_if_fail (condition && then_, NULL);

    node = ms_node_if_else_new (condition, then_, else_);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_while (MSParser   *parser,
            MSCondType  type,
            MSNode     *cond,
            MSNode     *what)
{
    MSNodeWhile *loop;

    g_return_val_if_fail (cond != NULL, NULL);

    loop = ms_node_while_new (type, cond, what);
    _ms_parser_add_node (parser, loop);

    return MS_NODE (loop);
}


static MSNode *
node_for (MSParser   *parser,
          MSNode     *var,
          MSNode     *list,
          MSNode     *what)
{
    MSNodeFor *loop;

    g_return_val_if_fail (var && list, NULL);

    loop = ms_node_for_new (var, list, what);
    _ms_parser_add_node (parser, loop);

    return MS_NODE (loop);
}


static MSNode *
node_var (MSParser   *parser,
          const char *name)
{
    MSNodeVar *node;

    node = ms_node_var_new (name);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_assignment (MSParser   *parser,
                 const char *name,
                 MSNode     *val)
{
    MSNodeAssign *node;
    MSNode *var;

    g_return_val_if_fail (name && name[0], NULL);
    g_return_val_if_fail (val != NULL, NULL);

    var = node_var (parser, name);
    node = ms_node_assign_new (MS_NODE_VAR (var), val);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_binary_op (MSParser   *parser,
                MSBinaryOp  op,
                MSNode     *lval,
                MSNode     *rval)
{
    MSNodeCommand *cmd;

    g_return_val_if_fail (lval && rval, NULL);

    cmd = ms_node_binary_op_new (op, lval, rval);
    _ms_parser_add_node (parser, cmd);

    return MS_NODE (cmd);
}


static MSNode *
node_unary_op (MSParser   *parser,
               MSUnaryOp   op,
               MSNode     *val)
{
    MSNodeCommand *cmd;

    g_return_val_if_fail (val != NULL, NULL);

    cmd = ms_node_unary_op_new (op, val);
    _ms_parser_add_node (parser, cmd);

    return MS_NODE (cmd);
}


static MSNode *
node_int (MSParser   *parser,
          int         n)
{
    MSNodeValue *node;
    MSValue *value;

    value = ms_value_int (n);
    node = ms_node_value_new (value);
    ms_value_unref (value);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_string (MSParser   *parser,
             const char *string)
{
    MSNodeValue *node;
    MSValue *value;

    value = ms_value_string (string);
    node = ms_node_value_new (value);
    ms_value_unref (value);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_python (MSParser   *parser,
             const char *string)
{
    MSNodePython *node;

    node = ms_node_python_new (string);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_value_list (MSParser   *parser,
                 MSNodeList *list)
{
    MSNodeValList *node;

    node = ms_node_val_list_new (list);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_value_range (MSParser   *parser,
                  MSNode     *first,
                  MSNode     *last)
{
    MSNodeValList *node;

    node = ms_node_val_range_new (first, last);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_list_elm (MSParser   *parser,
               MSNode     *list,
               MSNode     *ind)
{
    MSNodeListElm *node;

    node = ms_node_list_elm_new (list, ind);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_list_assign (MSParser   *parser,
                  MSNode     *list,
                  MSNode     *ind,
                  MSNode     *val)
{
    MSNodeListAssign *node;

    node = ms_node_list_assign_new (list, ind, val);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_return (MSParser   *parser,
             MSNode     *val)
{
    MSNodeReturn *node;

    node = ms_node_return_new (val);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_break (MSParser *parser)
{
    MSNodeBreak *node;

    node = ms_node_break_new (MS_BREAK_BREAK);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}


static MSNode *
node_continue (MSParser *parser)
{
    MSNodeBreak *node;

    node = ms_node_break_new (MS_BREAK_CONTINUE);
    _ms_parser_add_node (parser, node);

    return MS_NODE (node);
}
%}

%name-prefix="_ms_script_yy"

%union {
    int ival;
    const char *str;
    MSNode *node;
}

%token <str> IDENTIFIER
%token <str> LITERAL
%token <str> VARIABLE
%token <str> PYTHON
%token <ival> NUMBER

%type <node> program stmt stmt_or_python
%type <node> if_stmt ternary loop assignment
%type <node> simple_expr compound_expr expr variable list_elms

%token IF THEN ELSE FI
%token WHILE DO OD FOR IN
%token CONTINUE BREAK RETURN
%token EQ NEQ LE GE
%token AND OR NOT
%token UMINUS
%token TWODOTS

%lex-param      {MSParser *parser}
%parse-param    {MSParser *parser}
/* %expect 1 */

%left '-' '+'
%left '*' '/'
%left '%'
%left EQ NEQ '<' '>' GE LE
%left OR
%left AND
%left NOT
%left '#'
%left UMINUS

%%

script:   program           { _ms_parser_set_top_node (parser, $1); }
;

program:  stmt_or_python            { $$ = node_list_add (parser, NULL, $1); }
        | program stmt_or_python    { $$ = node_list_add (parser, MS_NODE_LIST ($1), $2); }
;

stmt_or_python:
          stmt ';'          { $$ = $1; }
        | PYTHON            { $$ = node_python (parser, $1); }
;

stmt:   /* empty */         { $$ = NULL; }
        | expr
        | if_stmt
        | loop
        | assignment /* not an expr because otherwise 'var = var + 2' gets parsed as '(var = var) + 2'*/
        | CONTINUE          { $$ = node_continue (parser); }
        | BREAK             { $$ = node_break (parser); }
        | RETURN            { $$ = node_return (parser, NULL); }
        | RETURN expr       { $$ = node_return (parser, $2); }
;

loop:     WHILE expr DO program OD              { $$ = node_while (parser, MS_COND_BEFORE, $2, $4); }
        | DO program WHILE expr                 { $$ = node_while (parser, MS_COND_AFTER, $4, $2); }
        | FOR variable IN expr DO program OD    { $$ = node_for (parser, $2, $4, $6); }
;

if_stmt:
          IF expr THEN program FI               { $$ = node_if_else (parser, $2, $4, NULL); }
        | IF expr THEN program ELSE program FI  { $$ = node_if_else (parser, $2, $4, $6); }
;

expr:     simple_expr
        | compound_expr
        | ternary
;

assignment:
          IDENTIFIER '=' expr               { $$ = node_assignment (parser, $1, $3); }
        | simple_expr '[' expr ']' '=' expr { $$ = node_list_assign (parser, $1, $3, $6); }
;

ternary:  simple_expr '?' simple_expr ':' simple_expr { $$ = node_if_else (parser, $1, $3, $5); }
;

compound_expr:
          expr '+' expr                     { $$ = node_binary_op (parser, MS_OP_PLUS, $1, $3); }
        | expr '-' expr                     { $$ = node_binary_op (parser, MS_OP_MINUS, $1, $3); }
        | expr '/' expr                     { $$ = node_binary_op (parser, MS_OP_DIV, $1, $3); }
        | expr '*' expr                     { $$ = node_binary_op (parser, MS_OP_MULT, $1, $3); }

        | expr AND expr                     { $$ = node_binary_op (parser, MS_OP_AND, $1, $3); }
        | expr OR expr                      { $$ = node_binary_op (parser, MS_OP_OR, $1, $3); }

        | expr EQ expr                      { $$ = node_binary_op (parser, MS_OP_EQ, $1, $3); }
        | expr NEQ expr                     { $$ = node_binary_op (parser, MS_OP_NEQ, $1, $3); }
        | expr '<' expr                     { $$ = node_binary_op (parser, MS_OP_LT, $1, $3); }
        | expr '>' expr                     { $$ = node_binary_op (parser, MS_OP_GT, $1, $3); }
        | expr LE expr                      { $$ = node_binary_op (parser, MS_OP_LE, $1, $3); }
        | expr GE expr                      { $$ = node_binary_op (parser, MS_OP_GE, $1, $3); }
        | '-' simple_expr %prec UMINUS      { $$ = node_unary_op (parser, MS_OP_UMINUS, $2); }
        | NOT simple_expr                   { $$ = node_unary_op (parser, MS_OP_NOT, $2); }
        | '#' simple_expr                   { $$ = node_unary_op (parser, MS_OP_LEN, $2); }
        | simple_expr '%' simple_expr       { $$ = node_binary_op (parser, MS_OP_FORMAT, $1, $3); }
;

simple_expr:
          NUMBER                            { $$ = node_int (parser, $1); }
        | LITERAL                           { $$ = node_string (parser, $1); }
        | variable
        | '(' stmt ')'                      { $$ = $2; }
        | '[' list_elms ']'                 { $$ = node_value_list (parser, MS_NODE_LIST ($2)); }
        | '[' expr TWODOTS expr ']'         { $$ = node_value_range (parser, $2, $4); }
        | IDENTIFIER '(' list_elms ')'      { $$ = node_command (parser, $1, MS_NODE_LIST ($3)); }
        | simple_expr '[' expr ']'          { $$ = node_list_elm (parser, $1, $3); }
;

list_elms: /* empty */                      { $$ = NULL; }
        | expr                              { $$ = node_list_add (parser, NULL, $1); }
        | list_elms ',' expr                { $$ = node_list_add (parser, MS_NODE_LIST ($1), $3); }
;

variable: IDENTIFIER                        { $$ = node_var (parser, $1); }
;

%%
