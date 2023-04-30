#ifndef CC_H
#define CC_H

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *user_input;

//
// tokenizer
//

typedef enum {
    TK_RESERVED,
    TK_IDENT,
    TK_NUM,
    TK_EOF,
    TK_RETURN,
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    int val;
    char *str;
    int len;
};

void error_at(char *loc, char *fmt, ...);
void error(char *fmt, ...);
Token *tokenize();

extern Token *token;

//
// parser
//

typedef struct LVar LVar;
struct LVar {
  LVar *next;
  char *name;
  int len;
  int offset;
};

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
    ND_LVAR,
    ND_ASSIGN, // =
    ND_RETURN, // ret
    ND_BLOCK, // { ... }
} NodeKind;

typedef struct Node Node;
struct Node {
  NodeKind kind;
  Node *next;
  Node *lhs;
  Node *rhs;
  Node *body;
  int val;
  int offset;
};


typedef struct Function Function;
struct Function {
  Node *body;
  LVar *locals;
};

Function *parse();

extern Node *code[100];

//
// codegen.c
//

void codegen(Function *prog);

#endif