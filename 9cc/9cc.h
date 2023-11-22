#ifndef CC_H
#define CC_H

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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
    int64_t val;
    char *loc;
    int len;
    Type *ty;
    char *str;
    int line_no;
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *op);
bool consume(Token **rest, Token *tok, char *str);
Token *tokenize_file(char *filename);

#define unreachable() \
  error("internal error at %s:%d", __FILE__, __LINE__)

//
// parser
//

typedef struct Obj Obj;
typedef struct Member Member;

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
    ND_COMMA, // ,
    ND_MEMBER, // . (struct member access)
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
    ND_CAST, // Type cast
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
  Member *member;
  Node *cond;
  Node *then;
  Node *els;
  Node *inc;
  Node *init;
  char *funcname;
  Type *func_ty;
  int64_t val;
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
  bool is_definition;
  bool is_static;
  char *init_data;
};

Node *new_cast(Node *expr, Type *ty);
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
  TY_VOID,
  TY_BOOL,
  TY_CHAR,
  TY_SHORT,
  TY_INT,
  TY_LONG,
  TY_ENUM,
  TY_PTR,
  TY_FUNC,
  TY_ARRAY,
  TY_STRUCT,
  TY_UNION,
} TypeKind;

struct Type {
  TypeKind kind;
  int size; // sizeof() value
  int align; // alignment
  Type *base;
  Token *name;
  Type *return_ty;
  Type *params;
  Type *next;
  int array_len;
  Member *members;
};

bool is_integer(Type *ty);
Type *enum_type(void);
void add_type(Node *node);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *copy_type(Type *ty);
Type *array_of(Type *base, int size);

void codegen(Obj *prog, FILE *out);
int align_to(int n, int align);

// Struct member
struct Member {
  Member *next;
  Type *ty;
  Token *name;
  int offset;
};

extern Type *ty_void;
extern Type *ty_bool;
extern Type *ty_int;
extern Type *ty_char;
extern Type *ty_long;
extern Type *ty_short;

#endif