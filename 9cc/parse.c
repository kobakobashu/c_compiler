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

    if (strchr("+-*/()<>=;{}&,[]", *p)) {
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

    if (strncmp(p, "int", 3) == 0 && !is_alnum(p[3])) {
      cur = new_token(TK_INT, cur, p, 3);
      p += 3;
      continue;
    }

    if (strncmp(p, "sizeof", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_SIZEOF, cur, p, 6);
      p += 6;
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

static Obj *locals;
static Obj *globals;

static Node *assign();
static Node *expr();
static Node *compound_stmt();
static Type *declarator(Type *ty);

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

static bool equal(char *op) {
  return memcmp(token->str, op, token->len) == 0 && op[token->len] == '\0';
}

static bool equal_token(Token *tok, char *op) {
  return memcmp(tok->str, op, tok->len) == 0;
}

static Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

static Node *new_unary(NodeKind kind, Node *expr) {
  Node *node = new_node(kind);
  node->lhs = expr;
  return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
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

Obj *find_var(Token *tok) {
  for (Obj *var = locals; var; var = var->next) {
    if (var->len == tok->len && !memcmp(var->name, tok->str, var->len)) {
      return var;
    }
  }
  for (Obj *var = globals; var; var = var->next) {
    if (var->len == tok->len && !memcmp(var->name, tok->str, var->len)) {
      return var;
    }
  }
  return NULL;
}

int get_number() {
  if (token->kind != TK_NUM) {
    error_at(token->str, "expected a number");
  }
  int sz = token->val;
  token = token->next;
  return sz;
}

static Node *new_add(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);

  // num + num
  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_ADD, lhs, rhs);

  if (lhs->ty->base && rhs->ty->base)
    error_at(token->str, "invalid operands");

  // Canonicalize `num + ptr` to `ptr + num`.
  if (!lhs->ty->base && rhs->ty->base) {
    Node *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  // ptr + num
  rhs = new_binary(ND_MUL, rhs, new_node_num(lhs->ty->base->size));
  return new_binary(ND_ADD, lhs, rhs);
}

static Node *new_sub(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);

  // num - num
  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_SUB, lhs, rhs);

  // ptr - num
  if (lhs->ty->base && is_integer(rhs->ty)) {
    rhs = new_binary(ND_MUL, rhs, new_node_num(lhs->ty->base->size));
    add_type(rhs);
    Node *node = new_binary(ND_SUB, lhs, rhs);
    node->ty = lhs->ty;
    return node;
  }

  // ptr - ptr, which returns how many elements are between the two.
  if (lhs->ty->base && rhs->ty->base) {
    Node *node = new_binary(ND_SUB, lhs, rhs);
    node->ty = ty_int;
    return new_binary(ND_DIV, node, new_node_num(lhs->ty->base->size));
  }

  error_at(token->str, "invalid operands");
}

static Obj *new_var(Token *tok, Type *ty) {
  Obj *var = calloc(1, sizeof(Obj));
  var->name = tok->str;
  var->len = tok->len;
  var->ty = ty;
  return var;
}

static Obj *new_lvar(Token *tok, Type *ty) {
  Obj *var = new_var(tok, ty);
  var->is_local = true;
  var->next = locals;
  if (locals) {
    var->offset = locals->offset + 8;
  } else {
    var->offset = 0;
  }
  locals = var;
  return var;
}

static Obj *new_gvar(Token *tok, Type *ty) {
  Obj *var = new_var(tok, ty);
  var->next = globals;
  if (globals) {
    var->offset = globals->offset + 8;
  } else {
    var->offset = 0;
  }
  globals = var;
  return var;
}

static void global_variable(Type *basety) {
  bool first = true;

  while (!consume(";")) {
    if (!first) {
      consume(",");
    }
    first = false;

    Type *ty = declarator(basety);
    new_gvar(ty->name, ty);
  }
  return;
}

static bool is_function() {
  Token *dummy = token;
  Type *base;
  Type *ty = declarator(base);
  token = dummy;
  return ty->kind == TY_FUNC;
}

// func-call = primary ("," primary)*

static Node *func_call(Node *node) {
  Node head = {};
  Node *cur = &head;
  while (!consume(")")) {
    Node *node = assign();
    cur->next = node;
    cur = cur->next;
    consume(",");
  }
  return head.next;
}

// primary = num 
//         | ident ("(" func-call? ")")?
//         | "(" expr ")"

static Node *primary() {
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }
  Token *tok = consume_ident();
  if (tok) {
    if (consume("(")) {
      Node *node = new_node(ND_FUNCALL);
      node->funcname = strndup(tok->str, tok->len);
      // func with no arg
      if (consume(")")) {
        return node;
      }
      // func with args
      Node *args = func_call(node);
      node->args = args;
      return node;
    }
    Node *node = new_node(ND_VAR);
    Obj *lvar = find_var(tok);
    if (!lvar) {
      error_at(token->str, "undefined variable");
    }
    node->var = lvar;
    return node;
  }
  return new_node_num(expect_number());
}

// postfix = primary ("[" expr "]")*

static Node *postfix() {
  Node *node = primary();
  while (equal("[")) {
    // x[y] is short for *(x+y)
    token = token->next;
    Node *idx = expr();
    expect("]");
    node = new_unary(ND_DEREF, new_add(node, idx));
  }
  return node;
}

// unary = "+"? unary
//       | "-"? unary
//       | "*" unary
//       | "&" unary
//       | postfix
//       | "sizeof" unary

static Node *unary() {
  if (consume("+")) {
    return unary();
  }
  if (consume("-")) {
    return new_binary(ND_SUB, new_node_num(0), unary());
  }
  if (consume("&")) {
    return new_unary(ND_ADDR, unary());
  }
  if (consume("*")) {
    return new_unary(ND_DEREF, unary());
  }
  if (token->kind == TK_SIZEOF) {
    token = token->next;
    Node *node = unary();
    add_type(node);
    return new_node_num(node->ty->size);
  }
  return postfix();
}

// mul = unary ("*" unary | "/" unary)*

static Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume("*")) {
      node = new_binary(ND_MUL, node, unary());
    } else if (consume("/")) {
      node = new_binary(ND_DIV, node, unary());
    } else {
      return node;
    }
  }
}

// add = mul ("+" mul | "-" mul)*

static Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume("+")) {
      node = new_add(node, mul());
    } else if (consume("-")) {
      node = new_sub(node, mul());
    } else {
      return node;
    }
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*

static Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<")) {
      node = new_binary(ND_LT, node, add());
    } else if (consume("<=")) {
      node = new_binary(ND_LE, node, add());
    } else if (consume(">")) {
      node = new_binary(ND_LT, add(), node);
    } else if (consume(">=")) {
      node = new_binary(ND_LE, add(), node);
    } else {
      return node;
    }
  }
}

// equality = relational ("==" relational | "!=" relational)*

static Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("==")) {
      node = new_binary(ND_EQ, node, relational());
    } else if (consume("!=")) {
      node = new_binary(ND_NE, node, relational());
    } else {
      return node;
    }
  }
}

// assign = equality ("=" assign)?

static Node *assign() {
  Node *node = equality();
  if (consume("="))
    node = new_binary(ND_ASSIGN, node, assign());
  return node;
}

// expr = assign

static Node *expr() {
  return assign();
}

// stmt = expr ";"
//      | "return" expr ";"
//      | "{" stmt* "}"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt

static Node *stmt() {
  Node *node;

  if (consume_return(TK_RETURN)) {
    node = new_unary(ND_RETURN, expr());
    if (!consume(";")) {
      error_at(token->str, "';' is needed");
    }
    return node;
  }

  if (equal("{")) {
    token = token->next;
    return compound_stmt();
  }

  if (equal(";")) {
    token = token->next;
    return new_node(ND_BLOCK);
  }

  if (token->kind == TK_IF) {
    Node *node = new_node(ND_IF);
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
    Node *node = new_node(ND_WHILE);
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
    Node *node = new_node(ND_FOR);
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

// declspec = "int"

static Type *declspec() {
  if (!equal("int")) {
    error("invalit declaration");
  }
  token = token->next;
  return ty_int;
}

// func-params = param ("," param)*
// param       = declspec declarator

static Type *func_params() {
  Type head = {};
  Type *cur = &head;
  while (!consume(")")) {
    Type *basety = declspec();
    Type *ty = declarator(basety);
    cur->next = copy_type(ty);
    cur = cur->next;
    consume(",");
  }
  return head.next;
}

// type-suffix = ("(" func-params? ")")
//             = "[" num "]" type-suffix
//             = ε

static Type *type_suffix(Type *ty) {
  if (consume("(")) {
    ty = func_type(ty);
    ty->params = func_params();
    return ty;
  }
  if (consume("[")) {
    int sz = get_number();

    expect("]");
    ty = type_suffix(ty);
    return array_of(ty, sz);
  }
  return ty;
}

// declarator = "*"* ident type-suffix

static Type *declarator(Type *ty) {
  while (consume("*")) {
    ty = pointer_to(ty);
  }

  if (token->kind != TK_IDENT) {
    error_at(token->str, "expected a variable name");
  }
  Token *tmp = token;
  if (equal_token(token->next, "(") || equal_token(token->next, "[")) {
    consume_ident();
    ty = type_suffix(ty);
  } else {
    consume_ident();
  }
  ty->name = tmp;
  return ty;
}

// declaration = declspec declarator ";"

static Node *declaration() {
  Type *basety = declspec();
  Type *ty = declarator(basety);
  Obj *lvar = new_lvar(ty->name, ty);
  Node *node = new_node(ND_BLOCK);
  if (!consume(";")) {
    error_at(token->str, "need ;");
  }
  return node;
}

// compound_stmt = (declaration | stmt)* "}"

static Node *compound_stmt() {
  Node head = {};
  Node *cur = &head;
  while (!equal("}")) {
    if (token->kind == TK_INT) {
      cur->next = declaration();
    } else {
      cur->next = stmt();
    }
    cur = cur->next;
    add_type(cur);
  }

  Node *node = new_node(ND_BLOCK);
  node->body = head.next;
  token = token->next;
  return node;
}

static void create_param_lvars(Type *param) {
  if (param) {
    create_param_lvars(param->next);
    Obj *lvar;
    new_lvar(param->name, param);
  }
}

// function = declspec declarator "{" compound_stmt* "}"
Obj *function(Type *basety) {
  Type *ty = declarator(basety);
  Obj *fn = new_gvar(ty->name, ty);
  fn->is_function = true;
  if (!equal("{")) {
    error_at(token->str, "need '{'");
  }
  token = token->next;
  locals = NULL;

  fn->name = strndup(ty->name->str, ty->name->len);
  create_param_lvars(ty->params);
  fn->params = locals;
  fn->body = compound_stmt();
  fn->locals = locals;
  return fn;
}

// code = function*

Obj *parse() {
  globals = NULL;

  while (token->kind != TK_EOF) {
    Type *basety = declspec();
    if (is_function()) {
      function(basety);
      continue;
    }
    
    global_variable(basety);
  }

  return globals;
}