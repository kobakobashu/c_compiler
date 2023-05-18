#include "9cc.h"

static void gen(Node *node);

static char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static int count(void) {
  static int i = 1;
  return i++;
}

static void push(void) {
  printf("  push rax\n");
}

static void pop(char *arg) {
  printf("  pop %s\n", arg);
}

// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
static int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

static void gen_lval(Node *node) {
  switch (node->kind) {
  case ND_LVAR:
    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->var->offset);
    printf("  push rax\n");
    return;
  case ND_DEREF:
    gen(node->lhs);
    return;
  }
}

static void gen(Node *node) {
  switch (node->kind) {
  case ND_IF: {
    int c = count();
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je  .L.else%d\n", c);
    gen(node->then);
    printf("  jmp .L.end%d\n", c);
    printf(".L.else%d:\n", c);
    if (node->els) {
      gen(node->els);
    }
    printf(".L.end%d:\n", c);
    return;
  }
  case ND_WHILE: {
    int c = count();
    printf(".L.begin%d:\n", c);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .L.end%d\n", c);
    gen(node->then);
    printf("  jmp .L.begin%d\n", c);
    printf(".L.end%d:\n", c);
    return;
  }
  case ND_FOR: {
    int c = count();
    gen(node->init);
    printf(".L.begin%d:\n", c);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .L.end%d\n", c);
    gen(node->then);
    gen(node->inc);
    printf("  jmp .L.begin%d\n", c);
    printf(".L.end%d:\n", c);
    return;
  }
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next)
      gen(n);
    return;
  case ND_NUM:
    printf("  push %d\n", node->val);
    return;
  case ND_LVAR:
    gen_lval(node);
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
    return;
  case ND_ASSIGN:
    gen_lval(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  mov [rax], rdi\n");
    printf("  push rdi\n");
    return;
  case ND_FUNCALL: {
    int nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      gen(arg);
      nargs++;
    }

    for (int i = nargs - 1; i >= 0; i--)
      pop(argreg[i]);

    printf("  call %s\n", node->funcname);
    printf("  push rax\n");
    return;
  }
  case ND_RETURN:
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  jmp .L.return\n");
    return;
  case ND_ADDR:
    gen_lval(node->lhs);
    return;
  case ND_DEREF:
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
  case ND_ADD:
    printf("  add rax, rdi\n");
    break;
  case ND_SUB:
    printf("  sub rax, rdi\n");
    break;
  case ND_MUL:
    printf("  imul rax, rdi\n");
    break;
  case ND_DIV:
    printf("  cqo\n");
    printf("  idiv rdi\n");
    break;
  case ND_EQ:
    printf("  cmp rax, rdi\n");
    printf("  sete al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_NE:
    printf("  cmp rax, rdi\n");
    printf("  setne al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_LT:
    printf("  cmp rax, rdi\n");
    printf("  setl al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_LE:
    printf("  cmp rax, rdi\n");
    printf("  setle al\n");
    printf("  movzb rax, al\n");
    break;
  }

  printf("  push rax\n");
}

static void assign_lvar_offset(Function *prog) {
  int max_off = prog->locals->offset;
  for (LVar *var = prog->locals; var; var = var->next) {
    var->offset = max_off - var->offset;
  }
  prog->stack_size = align_to(max_off, 16);
}

void codegen(Function *prog) {
  if (prog->locals) {
    assign_lvar_offset(prog);
  }
  printf(".intel_syntax noprefix\n");
  printf(".globl main\n");
  printf("main:\n");

  // prologue
  printf("  push rbp\n");
  printf("  mov rbp, rsp\n");
  printf("  sub rsp, %d\n", prog->stack_size);

  gen(prog->body);

  printf(".L.return:\n");
  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");
  printf("  ret\n");
}