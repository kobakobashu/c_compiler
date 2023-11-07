#ifndef CC_H
#define CC_H

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;

//
// strings.c
//

char *format(char *fmt, ...);

//
// tokenizer
//

typedef enum {
    TK_IDENT,   // Identifiers
    TK_PUNCT,   // Punctuators
    TK_KEYWORD, // Keywords
    TK_STR,     // String literals
    TK_NUM,     // Numeric literals
    TK_EOF,     // End-of-file markers
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    int val;
    char *loc;
    int len;
    Type *ty;
    char *str;
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *op);
bool consume(Token **rest, Token *tok, char *str);
Token *tokenize_file(char *filename);

//
// parser
//

typedef struct Obj Obj;

typedef enum {
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_NUM,
    ND_EQ, // ==
    ND_NE, // !=
    ND_LT, // <
    ND_LE, // <=
    ND_VAR,
    ND_ASSIGN, // =
    ND_RETURN, // ret
    ND_BLOCK, // { ... }
    ND_IF, // if
    ND_FOR, // for
    ND_ADDR, // &
    ND_DEREF, // *
    ND_FUNCALL, // function call
    ND_STMT_EXPR, // Statement expression
    ND_EXPR_STMT, // Expression statement
    ND_NEG,
} NodeKind;

typedef struct Node Node;
struct Node {
  NodeKind kind;
  Node *next;
  Type *ty;
  Token *tok;
  Node *lhs;
  Node *rhs;
  Node *body;
  Node *cond;
  Node *then;
  Node *els;
  Node *inc;
  Node *init;
  char *funcname;
  int val;
  Obj *var;
  Node *args;
};

struct Obj {
  Obj *next;
  char *name;
  Obj *params;
  Node *body;
  Obj *locals;
  int stack_size;
  Type *ty;
  int offset;
  bool is_local;
  bool is_function;
  char *init_data;
};

Obj *parse();

extern Node *code[100];

//
// codegen.c
//

void codegen(Obj *prog, FILE *out);

//
// type.c
//

typedef enum {
  TY_CHAR,
  TY_INT,
  TY_PTR,
  TY_FUNC,
  TY_ARRAY,
} TypeKind;

struct Type {
  TypeKind kind;
  int size; // sizeof() value
  Type *base;
  Token *name;
  Type *return_ty;
  Type *params;
  Type *next;
  int array_len;
};

bool is_integer(Type *ty);
void add_type(Node *node);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *copy_type(Type *ty);
Type *array_of(Type *base, int size);

void codegen(Obj *prog, FILE *out);

extern Type *ty_int;
extern Type *ty_char;

#endif