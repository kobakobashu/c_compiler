#include "9cc.h"

//
// tokenizer
//

Token *token;

void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, " ");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

static Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

static bool startswith(char *p, char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}

int is_alnum(char c) {
  return ('a' <= c && c <= 'z') ||
         ('A' <= c && c <= 'Z') ||
         ('1' <= c && c <= '9') ||
         (c == '_');
}

Token *tokenize() {
  char *p = user_input;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (startswith(p, "==") || startswith(p, "!=") ||
        startswith(p, "<=") || startswith(p, ">=")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    if (strchr("+-*/()<>=;{}", *p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    if (strncmp(p, "return", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_RETURN, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "if", 2) == 0 && !is_alnum(p[2])) {
      cur = new_token(TK_IF, cur, p, 2);
      p += 2;
      continue;
    }

    if (strncmp(p, "else", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_ELSE, cur, p, 4);
      p += 4;
      continue;
    }

    if (strncmp(p, "while", 5) == 0 && !is_alnum(p[5])) {
      cur = new_token(TK_WHILE, cur, p, 5);
      p += 5;
      continue;
    }

    if (strncmp(p, "for", 3) == 0 && !is_alnum(p[3])) {
      cur = new_token(TK_FOR, cur, p, 3);
      p += 3;
      continue;
    }

    if ('a' <= *p && *p <= 'z') {
      cur = new_token(TK_IDENT, cur, p, 0);
      char *q = p;
      while ('a' <= *p && *p <= 'z' || *p == '_' || isdigit(*p)) {
        p++;
      }
      cur->len = p - q;
      continue;
    }

    error_at(token->str, "invalid character");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}

//
// parser
//

LVar *locals;

static Node *expr();

static Node *compound_stmt();

static bool consume(char *op) {
  if (token->kind != TK_RESERVED ||
      strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    return false;
  token = token->next;
  return true;
}

static Token *consume_ident() {
  if (token->kind != TK_IDENT) {
    return NULL;
  }
  Token *tok = token;
  token = token->next;
  return tok;
}

static bool consume_return(TokenKind kind) {
  if (token->kind != kind) {
    return false;
  }
  token = token->next;
  return true;
}

static void expect(char *op) {
  if (token->kind != TK_RESERVED ||
      strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    error_at(token->str, "unexpected operator: \"%s\"", op);
  token = token->next;
}

static int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "unexpected type: not integer");
  int val = token->val;
  token = token->next;
  return val;
}

static bool at_eof() {
    return token->kind == TK_EOF;
}

static Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_node_num(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

LVar *find_lvar(Token *tok) {
  for (LVar *var = locals; var; var = var->next) {
    if (var->len == tok->len && !memcmp(var->name, tok->str, var->len)) {
      return var;
    }
  }
  return NULL;
}

static Node *primary() {
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }
  Token *tok = consume_ident();
  if (tok) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;

    LVar *lvar = find_lvar(tok);
    if (lvar) {
      node->offset = lvar->offset;
    } else {
      lvar = calloc(1, sizeof(LVar));
      lvar->next = locals;
      lvar->name = tok->str;
      lvar->len = tok->len;
      if (locals) {
        lvar->offset = locals->offset + 8;
      } else {
        lvar->offset = 0;
      }
      node->offset = lvar->offset;
      locals = lvar;
    }
    return node;
  }
  return new_node_num(expect_number());
}

static Node *unary() {
  if (consume("+")) {
    return unary();
  }
  if (consume("-")) {
    return new_node(ND_SUB, new_node_num(0), unary());
  }
  return primary();
}

static Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume("*")) {
      node = new_node(ND_MUL, node, unary());
    } else if (consume("/")) {
      node = new_node(ND_DIV, node, unary());
    } else {
      return node;
    }
  }
}

static Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume("+")) {
      node = new_node(ND_ADD, node, mul());
    } else if (consume("-")) {
      node = new_node(ND_SUB, node, mul());
    } else {
      return node;
    }
  }
}

static Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<")) {
      node = new_node(ND_LT, node, add());
    } else if (consume("<=")) {
      node = new_node(ND_LE, node, add());
    } else if (consume(">")) {
      node = new_node(ND_LT, add(), node);
    } else if (consume(">=")) {
      node = new_node(ND_LE, add(), node);
    } else {
      return node;
    }
  }
}

static Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("==")) {
      node = new_node(ND_EQ, node, relational());
    } else if (consume("!=")) {
      node = new_node(ND_NE, node, relational());
    } else {
      return node;
    }
  }
}

static Node *assign() {
  Node *node = equality();
  if (consume("="))
    node = new_node(ND_ASSIGN, node, assign());
  return node;
}

static Node *expr() {
  return assign();
}

static Node *stmt() {
  Node *node;

  if (consume_return(TK_RETURN)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->lhs = expr();
    if (!consume(";")) {
      error_at(token->str, "';' is needed");
    }
    return node;
  }

  if (*token->str == '{') {
    token = token->next;
    return compound_stmt();
  }

  if (*token->str == ';') {
    token = token->next;
    return new_node(ND_BLOCK, NULL, NULL);
  }

  if (token->kind == TK_IF) {
    Node *node = new_node(ND_IF, NULL, NULL);
    token = token->next;
    if (!consume("(")) {
      error_at(token->str, "need '('");
    }
    node->cond = expr();
    if (!consume(")")) {
      error_at(token->str, "need ')'");
    }
    node->then = stmt();
    if (token->kind == TK_ELSE) {
      token = token->next;
      node->els = stmt();
    }
    return node;
  }

  if (token->kind == TK_WHILE) {
    Node *node = new_node(ND_WHILE, NULL, NULL);
    token = token->next;
    if (!consume("(")) {
      error_at(token->str, "need '('");
    }
    node->cond = expr();
    if (!consume(")")) {
      error_at(token->str, "need ')'");
    }
    node->then = stmt();
    return node;
  }

  if (token->kind == TK_FOR) {
    Node *node = new_node(ND_FOR, NULL, NULL);
    token = token->next;
    if (!consume("(")) {
      error_at(token->str, "need '('");
    }
    node->init = expr();
    if (consume(";")) {
      node->cond = expr();
    }
    consume(";");
    if (!consume(")")) {
      node->inc = expr();
    }
    consume(")");
    node->then = stmt();
    return node;
  }

  node = expr();
  if (!consume(";")) {
    error_at(token->str, "';' is needed");
  }
  return node;
}

static Node *compound_stmt() {
  Node head = {};
  Node *cur = &head;
  while (*token->str != '}') {
    cur->next = stmt();
    cur = cur->next;
  }

  Node *node = new_node(ND_BLOCK, NULL, NULL);
  node->body = head.next;
  token = token->next;
  return node;
}

Function *parse() {
  if (*token->str != '{') {
    error_at(token->str, "need '{'");
  }
  token = token->next;

  Function *prog = calloc(1, sizeof(Function));
  prog->body = compound_stmt();
  prog->locals = locals;
  return prog;
}