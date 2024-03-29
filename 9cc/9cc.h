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

#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

typedef struct Type Type;

//
// strings.c
//

char *format(char *fmt, ...);

//
// tokenizer
//

typedef enum
{
  TK_IDENT,   // Identifiers
  TK_PUNCT,   // Punctuators
  TK_KEYWORD, // Keywords
  TK_STR,     // String literals
  TK_NUM,     // Numeric literals
  TK_EOF,     // End-of-file markers
} TokenKind;

typedef struct Token Token;
struct Token
{
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
typedef struct Relocation Relocation;

typedef enum
{
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
  ND_ASSIGN,    // =
  ND_COMMA,     // ,
  ND_MEMBER,    // . (struct member access)
  ND_RETURN,    // ret
  ND_BLOCK,     // { ... }
  ND_GOTO,      // "goto"
  ND_LABEL,     // Labeled statement
  ND_IF,        // if
  ND_FOR,       // for
  ND_DO,        // "do"
  ND_SWITCH,    // "switch"
  ND_CASE,      // "case"
  ND_ADDR,      // &
  ND_DEREF,     // *
  ND_FUNCALL,   // function call
  ND_STMT_EXPR, // Statement expression
  ND_EXPR_STMT, // Expression statement
  ND_NEG,
  ND_CAST,      // Type cast
  ND_NOT,       // !
  ND_BITNOT,    // ~
  ND_LOGAND,    // &&
  ND_LOGOR,     // ||
  ND_MOD,       // %
  ND_BITAND,    // &
  ND_BITOR,     // |
  ND_BITXOR,    // ^
  ND_SHL,       // <<
  ND_SHR,       // >>
  ND_COND,      // ?:
  ND_NULL_EXPR, // Do nothing
  ND_MEMZERO,   // Zero-clear a stack variable
} NodeKind;

typedef struct Node Node;
struct Node
{
  NodeKind kind;
  Node *next;
  Type *ty;
  Token *tok;
  Node *lhs;
  Node *rhs;
  // "break" label
  char *brk_label;
  char *cont_label;
  Node *body;
  Member *member;
  Node *cond;
  Node *then;
  Node *els;
  Node *inc;
  Node *init;
  char *funcname;
  // Goto or labeled statement
  char *label;
  char *unique_label;
  Node *goto_next;
  Type *func_ty;
  int64_t val;
  Obj *var;
  Node *args;
  // Switch-cases
  Node *case_next;
  Node *default_case;
};

struct Obj
{
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
  Relocation *rel;
  int align; // alignment
  Obj *va_area;
};

// Global variable can be initialized either by a constant expression
// or a pointer to another global variable. This struct represents the
// latter.
typedef struct Relocation Relocation;
struct Relocation {
  Relocation *next;
  int offset;
  char *label;
  long addend;
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

typedef enum
{
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

struct Type
{
  TypeKind kind;
  int size;  // sizeof() value
  int align; // alignment
  Type *base;
  Token *name;
  Type *return_ty;
  Type *params;
  Type *next;
  int array_len;
  Member *members;
  bool is_flexible;
  bool is_variadic;
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
Type *struct_type(void);

// Struct member
struct Member
{
  Member *next;
  Type *ty;
  Token *tok; // for error message
  Token *name;
  int offset;
  int idx;
  int align;
};

extern Type *ty_void;
extern Type *ty_bool;
extern Type *ty_int;
extern Type *ty_char;
extern Type *ty_long;
extern Type *ty_short;

#endif