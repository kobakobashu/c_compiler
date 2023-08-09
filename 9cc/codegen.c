#include "9cc.h"

static FILE *output_file;
static void gen(Node *node);

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
}

static void pop(char *arg) {
  println("  pop %s", arg);
}

// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
static int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

static void gen_val(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local) {
      println("  mov rax, rbp");
      println("  add rax, %d", node->var->offset);
      println("  push rax");
    } else {
      println("  lea rax, [rip + %s]", node->var->name);
      println("  push rax");
    }
    return;
  case ND_DEREF:
    gen(node->lhs);
    return;
  }
}

// Load a value from where %rax is pointing to.
static void load(Type *ty) {
  if (ty->kind == TY_ARRAY) {
    // If it is an array, do not attempt to load a value to the
    // register because in general we can't load an entire array to a
    // register. As a result, the result of an evaluation of an array
    // becomes not the array itself but the address of the array.
    // This is where "array is automatically converted to a pointer to
    // the first element of the array in C" occurs.
    return;
  }

  println("  pop rax");
  if (ty->size == 1)
    println("  movsx rax, byte ptr [rax]");
  else
    println("  mov rax, [rax]");

  println("  push rax");
}

// Store %rax to an address that the stack top is pointing to.
static void store(Type *ty) {
  println("  pop rdi");
  println("  pop rax");
  if (ty->size == 1)
    println("  mov [rax], dil");
  else
    println("  mov [rax], rdi");
  println("  push rdi");
}

static void gen(Node *node) {
  switch (node->kind) {
  case ND_IF: {
    int c = count();
    gen(node->cond);
    println("  pop rax");
    println("  cmp rax, 0");
    println("  je  .L.else%d", c);
    gen(node->then);
    println("  jmp .L.end%d", c);
    println(".L.else%d:", c);
    if (node->els) {
      gen(node->els);
    }
    println(".L.end%d:", c);
    return;
  }
  case ND_WHILE: {
    int c = count();
    println(".L.begin%d:", c);
    gen(node->cond);
    println("  pop rax");
    println("  cmp rax, 0");
    println("  je .L.end%d", c);
    gen(node->then);
    println("  jmp .L.begin%d", c);
    println(".L.end%d:", c);
    return;
  }
  case ND_FOR: {
    int c = count();
    gen(node->init);
    println(".L.begin%d:", c);
    gen(node->cond);
    println("  pop rax");
    println("  cmp rax, 0");
    println("  je .L.end%d", c);
    gen(node->then);
    gen(node->inc);
    println("  jmp .L.begin%d", c);
    println(".L.end%d:", c);
    return;
  }
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next)
      gen(n);
    return;
  case ND_NUM:
    println("  push %d", node->val);
    return;
  case ND_VAR:
    gen_val(node);
    load(node->ty);
    return;
  case ND_ASSIGN:
    gen_val(node->lhs);
    gen(node->rhs);

    store(node->ty);
    return;
  case ND_STMT_EXPR:
    for (Node *n = node->body; n; n = n->next)
      gen(n);
    return;
  case ND_FUNCALL: {
    int nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      gen(arg);
      nargs++;
    }

    for (int i = nargs - 1; i >= 0; i--)
      pop(argreg64[i]);

    println("  call %s", node->funcname);
    println("  push rax");
    return;
  }
  case ND_RETURN:
    gen(node->lhs);
    println("  pop rax");
    println("  jmp .L.return.%s", current_fn->name);
    return;
  case ND_EXPR_STMT:
    gen(node->lhs);
    return;
  case ND_ADDR:
    gen_val(node->lhs);
    return;
  case ND_DEREF:
    gen(node->lhs);
    load(node->ty);
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  println("  pop rdi");
  println("  pop rax");

  switch (node->kind) {
  case ND_ADD:
    println("  add rax, rdi");
    break;
  case ND_SUB:
    println("  sub rax, rdi");
    break;
  case ND_MUL:
    println("  imul rax, rdi");
    break;
  case ND_DIV:
    println("  cqo");
    println("  idiv rdi");
    break;
  case ND_EQ:
    println("  cmp rax, rdi");
    println("  sete al");
    println("  movzb rax, al");
    break;
  case ND_NE:
    println("  cmp rax, rdi");
    println("  setne al");
    println("  movzb rax, al");
    break;
  case ND_LT:
    println("  cmp rax, rdi");
    println("  setl al");
    println("  movzb rax, al");
    break;
  case ND_LE:
    println("  cmp rax, rdi");
    println("  setle al");
    println("  movzb rax, al");
    break;
  }

  println("  push rax");
}

static void assign_lvar_offset(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function) {
      continue;
    }

    if (fn->locals) {
      int offset = 0;
      for (Obj *var = fn->locals; var; var = var->next) {
        offset += var->ty->size;
        var->offset = -offset;
      }
      fn->stack_size = align_to(offset, 16);
    }
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

    gen(fn->body);

    println(".L.return.%s:", fn->name);
    println("  mov rsp, rbp");
    println("  pop rbp");
    println("  ret");
  }
}

void codegen(Obj *prog, FILE *out) {
  output_file = out;

  assign_lvar_offset(prog);
  emit_data(prog);
  emit_text(prog);
}