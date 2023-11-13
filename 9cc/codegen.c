#include "9cc.h"

static FILE *output_file;
static void gen_stmt(Node *node);
static void gen_expr(Node *node);
static int depth;
static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static Obj *current_fn;

static void println(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(output_file, fmt, ap);
  va_end(ap);
  fprintf(output_file, "\n");
}

static int count(void) {
  static int i = 1;
  return i++;
}

static void push(void) {
  println("  push rax");
  depth++;
}

static void pop(char *arg) {
  println("  pop %s", arg);
  depth--;
}

// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

static void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local) {
      println("  lea rax, [rbp + %d]", node->var->offset);
    } else {
      println("  lea rax, [rip + %s]", node->var->name);
    }
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    return;
  case ND_COMMA:
    gen_expr(node->lhs);
    gen_addr(node->rhs);
    return;
  case ND_MEMBER:
    gen_addr(node->lhs);
    println("  add rax, %d", node->member->offset);
    return;
  }

  error_tok(node->tok, "not an lvalue");
}

// Load a value from where %rax is pointing to.
static void load(Type *ty) {
  if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    // If it is an array, do not attempt to load a value to the
    // register because in general we can't load an entire array to a
    // register. As a result, the result of an evaluation of an array
    // becomes not the array itself but the address of the array.
    // This is where "array is automatically converted to a pointer to
    // the first element of the array in C" occurs.
    return;
  }

  if (ty->size == 1)
    println("  movsx rax, byte ptr [rax]");
  else
    println("  mov rax, [rax]");

}

// Store %rax to an address that the stack top is pointing to.
static void store(Type *ty) {
  pop("rdi");
  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    for (int i = 0; i < ty->size; i++) {
      println("  mov r8b, %d[rax]", i);
      println("  mov %d[rdi], r8b", i);
    }
    return;
  }
  if (ty->size == 1)
    // ToDo: Research why rdi is enough if size == 1
    println("  mov [rdi], rax");
  else
    println("  mov [rdi], rax");
}

// Generate code for a given node.
static void gen_expr(Node *node) {
  println(" .loc 1 %d", node->tok->line_no);
  switch (node->kind) {
  case ND_NUM:
    println("  mov rax, %d", node->val);
    return;
  case ND_NEG:
    gen_expr(node->lhs);
    println("  neg rax");
    return;
  case ND_VAR:
  case ND_MEMBER:
    gen_addr(node);
    load(node->ty);
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    load(node->ty);
    return;
  case ND_ADDR:
    gen_addr(node->lhs);
    return;
  case ND_ASSIGN:
    gen_addr(node->lhs);
    push();
    gen_expr(node->rhs);
    store(node->ty);
    return;
  case ND_STMT_EXPR:
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    return;
  case ND_COMMA:
    gen_expr(node->lhs);
    gen_expr(node->rhs);
    return;
  case ND_FUNCALL: {
    int nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      gen_expr(arg);
      push();
      nargs++;
    }
    for (int i = nargs - 1; i >= 0; i--)
      pop(argreg64[i]);
    println("  mov rax, 0");
    println("  call %s", node->funcname);
    return;
  }
  }

  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("rdi");

  switch (node->kind) {
  case ND_ADD:
    println("  add rax, rdi");
    return;
  case ND_SUB:
    println("  sub rax, rdi");
    return;
  case ND_MUL:
    println("  imul rax, rdi");
    return;
  case ND_DIV:
    println("  cqo");
    println("  idiv rdi");
    return;
  case ND_EQ:
    println("  cmp rax, rdi");
    println("  sete al");
    println("  movzb rax, al");
    return;
  case ND_NE:
    println("  cmp rax, rdi");
    println("  setne al");
    println("  movzb rax, al");
    return;
  case ND_LT:
    println("  cmp rax, rdi");
    println("  setl al");
    println("  movzb rax, al");
    return;
  case ND_LE:
    println("  cmp rax, rdi");
    println("  setle al");
    println("  movzb rax, al");
    return;
  }
  error_tok(node->tok, "invalid expression");
}

static void gen_stmt(Node *node) {
  println(" .loc 1 %d", node->tok->line_no);
  switch (node->kind) {
  case ND_IF: {
    int c = count();
    gen_expr(node->cond);
    println("  cmp rax, 0");
    println("  je  .L.else%d", c);
    gen_stmt(node->then);
    println("  jmp .L.end%d", c);
    println(".L.else%d:", c);
    if (node->els) {
      gen_stmt(node->els);
    }
    println(".L.end%d:", c);
    return;
  }
  case ND_FOR: {
    int c = count();
    if (node->init)
      gen_stmt(node->init);
    println(".L.begin%d:", c);
    if (node->cond) {
      gen_expr(node->cond);
      println("  cmp rax, 0");
      println("  je .L.end%d", c);
    }
    gen_stmt(node->then);
    if (node->inc)
      gen_expr(node->inc);
    println("  jmp .L.begin%d", c);
    println(".L.end%d:", c);
    return;
  }
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    return;
  case ND_RETURN:
    gen_expr(node->lhs);
    println("  jmp .L.return.%s", current_fn->name);
    return;
  case ND_EXPR_STMT:
    gen_expr(node->lhs);
    return;
  }

  error_tok(node->tok, "invalid statement");
}

static void assign_lvar_offsets(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function) {
      continue;
    }

    int offset = 0;
    for (Obj *var = fn->locals; var; var = var->next) {
      offset += var->ty->size;
      offset = align_to(offset, var->ty->align);
      var->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);
  }
}

static void emit_data(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (var->is_function)
      continue;

    println("  .data");
    println("  .globl %s", var->name);
    println("%s:", var->name);
    if (var->init_data) {
      for (int i = 0; i < var->ty->size; i++)
        println("  .byte %d", var->init_data[i]);
    } else {
      println("  .zero %d", var->ty->size);
    }
  }
}

static void emit_text(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function) {
      continue;
    }
    println(".intel_syntax noprefix");
    println(".globl %s", fn->name);
    println(".text");
    println("%s:", fn->name);
    current_fn = fn;

    // prologue
    println("  push rbp");
    println("  mov rbp, rsp");
    println("  sub rsp, %d", fn->stack_size);

    // Save passed-by-register arguments to the stack
    int i = 0;
    for (Obj *var = fn->params; var; var = var->next) {
      if (var->ty->size == 1)
        println("  mov %d[rbp], %s", var->offset, argreg8[i++]);
      else
        println("  mov %d[rbp], %s", var->offset, argreg64[i++]);
    }

    gen_stmt(fn->body);
    assert(depth == 0);

    println(".L.return.%s:", fn->name);
    println("  mov rsp, rbp");
    println("  pop rbp");
    println("  ret");
  }
}

void codegen(Obj *prog, FILE *out) {
  output_file = out;

  assign_lvar_offsets(prog);
  emit_data(prog);
  emit_text(prog);
}