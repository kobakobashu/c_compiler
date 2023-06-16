#ifndef CC_H
#define CC_H

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *user_input;

typedef struct Type Type;

//
// util.c
//

char* strndup(const char* source, size_t length);

//
// tokenizer
//

typedef enum {
    TK_RESERVED,
    TK_IDENT,
    TK_NUM,
    TK_EOF,
    TK_RETURN,
    TK_IF,
    TK_ELSE,
    TK_WHILE,
    TK_FOR,
    TK_INT,
    TK_CHAR,
    TK_SIZEOF,
    TK_STR,
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    int val;
    char *str;
    int len;
    Type *ty;
};

void error_at(char *loc, char *fmt, ...);
void error(char *fmt, ...);
Token *tokenize();

extern Token *token;

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
    ND_WHILE, // while
    ND_FOR, // for
    ND_ADDR, // &
    ND_DEREF, // *
    ND_FUNCALL, // function call
} NodeKind;

typedef struct Node Node;
struct Node {
  NodeKind kind;
  Node *next;
  Type *ty;
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
  int len;
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

void codegen(Obj *prog);

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
Type *array_of(Type *base, int len);

extern Type *ty_int;
extern Type *ty_char;

#endif