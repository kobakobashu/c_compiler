#include "9cc.h"
#include <stdint.h>

static FILE *output_file;
static void gen_stmt(Node *node);
static void gen_expr(Node *node);
static int depth;
static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static Obj *current_fn;

static void println(char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(output_file, fmt, ap);
  va_end(ap);
  fprintf(output_file, "\n");
}

static int count(void)
{
  static int i = 1;
  return i++;
}

static void push(void)
{
  println("  push rax");
  depth++;
}

static void pop(char *arg)
{
  println("  pop %s", arg);
  depth--;
}

// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
int align_to(int n, int align)
{
  return (n + align - 1) / align * align;
}

static void gen_addr(Node *node)
{
  switch (node->kind)
  {
  case ND_VAR:
    if (node->var->is_local)
    {
      println("  lea rax, [rbp + %d]", node->var->offset);
    }
    else
    {
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
static void load(Type *ty)
{
  if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT || ty->kind == TY_UNION)
  {
    // If it is an array, do not attempt to load a value to the
    // register because in general we can't load an entire array to a
    // register. As a result, the result of an evaluation of an array
    // becomes not the array itself but the address of the array.
    // This is where "array is automatically converted to a pointer to
    // the first element of the array in C" occurs.
    return;
  }

  // When we load a char or a short value to a register, we always
  // extend them to the size of int, so we can assume the lower half of
  // a register always contains a valid value. The upper half of a
  // register for char, short and int may contain garbage. When we load
  // a long value to a register, it simply occupies the entire register.

  if (ty->size == 1)
    println("  movsx eax, byte ptr [rax]");
  else if (ty->size == 2)
    println("  movsx eax, word ptr [rax]");
  else if (ty->size == 4)
    println("  movsxd rax, dword ptr [rax]");
  else
    println("  mov rax, [rax]");
}

// Store %rax to an address that the stack top is pointing to.
static void store(Type *ty)
{
  pop("rdi");
  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION)
  {
    for (int i = 0; i < ty->size; i++)
    {
      println("  mov r8b, %d[rax]", i);
      println("  mov %d[rdi], r8b", i);
    }
    return;
  }
  if (ty->size == 1)
    // ToDo: Research why rdi is enough if size == 1
    println("  mov [rdi], al");
  else if (ty->size == 2)
    println("  mov [rdi], ax");
  else if (ty->size == 4)
    println("  mov [rdi], eax");
  else
    println("  mov [rdi], rax");
}

static void store_gp(int r, int offset, int sz)
{
  switch (sz)
  {
  case 1:
    println("  mov %d[rbp], %s", offset, argreg8[r]);
    return;
  case 2:
    println("  mov %d[rbp], %s", offset, argreg16[r]);
    return;
  case 4:
    println("  mov %d[rbp], %s", offset, argreg32[r]);
    return;
  case 8:
    println("  mov %d[rbp], %s", offset, argreg64[r]);
    return;
  }
  unreachable();
}

static void cmp_zero(Type *ty)
{
  if (is_integer(ty) && ty->size <= 4)
    println("  cmp eax, 0");
  else
    println("  cmp rax, 0");
}

enum
{
  I8,
  I16,
  I32,
  I64
};

static int getTypeId(Type *ty)
{
  switch (ty->kind)
  {
  case TY_CHAR:
    return I8;
  case TY_SHORT:
    return I16;
  case TY_INT:
    return I32;
  }
  return I64;
}

// The table for type casts
static char i32i8[] = "movsbl eax, al";
static char i32i16[] = "movswl eax, ax";
static char i32i64[] = "movsxd rax, eax";

static char *cast_table[][10] = {
    {NULL, NULL, NULL, i32i64},    // i8
    {i32i8, NULL, NULL, i32i64},   // i16
    {i32i8, i32i16, NULL, i32i64}, // i32
    {i32i8, i32i16, NULL, NULL},   // i64
};

static void cast(Type *from, Type *to)
{
  if (to->kind == TY_VOID)
    return;

  if (to->kind == TY_BOOL)
  {
    cmp_zero(from);
    println("  setne al");
    println("  movzx eax, al");
    return;
  }

  int t1 = getTypeId(from);
  int t2 = getTypeId(to);
  if (cast_table[t1][t2])
    println("  %s", cast_table[t1][t2]);
}

// Generate code for a given node.
static void gen_expr(Node *node)
{
  println(" .loc 1 %d", node->tok->line_no);
  switch (node->kind)
  {
  case ND_NULL_EXPR:
    return;
  case ND_NUM:
    println("  mov rax, %ld", node->val);
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
  case ND_CAST:
    gen_expr(node->lhs);
    cast(node->lhs->ty, node->ty);
    return;
  case ND_MEMZERO:
    // `rep stosb` is equivalent to `memset(%rdi, %al, %rcx)`.
    println("  mov rcx, %d", node->var->ty->size);
    println("  lea rdi, %d[rbp]", node->var->offset);
    println("  mov al, 0");
    println("  rep stosb");
    return;
  case ND_COND:
  {
    int c = count();
    gen_expr(node->cond);
    println("  cmp rax, 0");
    println("  je .L.else.%d", c);
    gen_expr(node->then);
    println("  jmp .L.end.%d", c);
    println(".L.else.%d:", c);
    gen_expr(node->els);
    println(".L.end.%d:", c);
    return;
  }
  case ND_NOT:
    gen_expr(node->lhs);
    println("  cmp rax, 0");
    println("  sete al");
    println("  movzx rax, al");
    return;
  case ND_BITNOT:
    gen_expr(node->lhs);
    println("  not rax");
    return;

  case ND_LOGAND:
  {
    int c = count();
    gen_expr(node->lhs);
    println("  cmp rax, 0");
    println("  je .L.false.%d", c);
    gen_expr(node->rhs);
    println("  cmp rax, 0");
    println("  je .L.false.%d", c);
    println("  mov rax, 1");
    println("  jmp .L.end.%d", c);
    println(".L.false.%d:", c);
    println("  mov rax, 0");
    println(".L.end.%d:", c);
    return;
  }
  case ND_LOGOR:
  {
    int c = count();
    gen_expr(node->lhs);
    println("  cmp rax, 0");
    println("  jne .L.true.%d", c);
    gen_expr(node->rhs);
    println("  cmp rax, 0");
    println("  jne .L.true.%d", c);
    println("  mov rax, 0");
    println("  jmp .L.end.%d", c);
    println(".L.true.%d:", c);
    println("  mov rax, 1");
    println(".L.end.%d:", c);
    return;
  }

  case ND_FUNCALL:
  {
    int nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next)
    {
      gen_expr(arg);
      push();
      nargs++;
    }
    for (int i = nargs - 1; i >= 0; i--)
      pop(argreg64[i]);
    println("  mov rax, 0");
    if (depth % 2 == 0) {
      println("  call %s", node->funcname);
    } else {
      println("  sub rsp, 8");
      println("  call %s", node->funcname);
      println("  add rsp, 8");
    }

    // It looks like the most significant 48 or 56 bits in RAX may
    // contain garbage if a function return type is short or bool/char,
    // respectively. We clear the upper bits here.
    switch (node->ty->kind) {
    case TY_BOOL:
      println("  movzx eax, al");
      return;
    case TY_CHAR:
      println("  movsbl eax, al");
      return;
    case TY_SHORT:
      println("  movswl eax, ax");
      return;
    }
    return;
  }
  }

  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("rdi");

  char *ax, *di;

  if (node->lhs->ty->kind == TY_LONG || node->lhs->ty->base)
  {
    ax = "rax";
    di = "rdi";
  }
  else
  {
    ax = "eax";
    di = "edi";
  }

  switch (node->kind)
  {
  case ND_ADD:
    println("  add %s, %s", ax, di);
    return;
  case ND_SUB:
    println("  sub %s, %s", ax, di);
    return;
  case ND_MUL:
    println("  imul %s, %s", ax, di);
    return;
  case ND_DIV:
  case ND_MOD:
    if (node->lhs->ty->size == 8)
      println("  cqo");
    else
      println("  cdq");
    println("  idiv %s", di);

    if (node->kind == ND_MOD)
      println("  mov rax, rdx");
    return;
  case ND_BITAND:
    println("  and rax, rdi");
    return;
  case ND_BITOR:
    println("  or rax, rdi");
    return;
  case ND_BITXOR:
    println("  xor rax, rdi");
    return;
  case ND_EQ:
    println("  cmp %s, %s", ax, di);
    println("  sete al");
    println("  movzb rax, al");
    return;
  case ND_NE:
    println("  cmp %s, %s", ax, di);
    println("  setne al");
    println("  movzb rax, al");
    return;
  case ND_LT:
    println("  cmp %s, %s", ax, di);
    println("  setl al");
    println("  movzb rax, al");
    return;
  case ND_LE:
    println("  cmp %s, %s", ax, di);
    println("  setle al");
    println("  movzb rax, al");
    return;
  case ND_SHL:
    println("  mov rcx, rdi");
    println("  shl %s, cl", ax);
    return;
  case ND_SHR:
    println("  mov rcx, rdi");
    if (node->ty->size == 8)
      println("  sar %s, cl", ax);
    else
      println("  sar %s, cl", ax);
    return;
  }
  error_tok(node->tok, "invalid expression");
}

static void gen_stmt(Node *node)
{
  println(" .loc 1 %d", node->tok->line_no);
  switch (node->kind)
  {
  case ND_IF:
  {
    int c = count();
    gen_expr(node->cond);
    println("  cmp rax, 0");
    println("  je  .L.else%d", c);
    gen_stmt(node->then);
    println("  jmp .L.end%d", c);
    println(".L.else%d:", c);
    if (node->els)
    {
      gen_stmt(node->els);
    }
    println(".L.end%d:", c);
    return;
  }
  case ND_FOR:
  {
    int c = count();
    if (node->init)
      gen_stmt(node->init);
    println(".L.begin%d:", c);
    if (node->cond)
    {
      gen_expr(node->cond);
      println("  cmp rax, 0");
      println("  je %s", node->brk_label);
    }
    gen_stmt(node->then);
    println("%s:", node->cont_label);
    if (node->inc)
      gen_expr(node->inc);
    println("  jmp .L.begin%d", c);
    println("%s:", node->brk_label);
    return;
  }
  case ND_DO: {
    int c = count();
    println(".L.begin.%d:", c);
    gen_stmt(node->then);
    println("%s:", node->cont_label);
    gen_expr(node->cond);
    println("  cmp rax, 0");
    println("  jne .L.begin.%d", c);
    println("%s:", node->brk_label);
    return;
  }
  case ND_SWITCH:
    gen_expr(node->cond);

    for (Node *n = node->case_next; n; n = n->case_next)
    {
      char *reg = (node->cond->ty->size == 8) ? "rax" : "eax";
      println("  cmp %s, %ld", reg, n->val);
      println("  je %s", n->label);
    }

    if (node->default_case)
      println("  jmp %s", node->default_case->label);

    println("  jmp %s", node->brk_label);
    gen_stmt(node->then);
    println("%s:", node->brk_label);
    return;
  case ND_CASE:
    println("%s:", node->label);
    gen_stmt(node->lhs);
    return;
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    return;
  case ND_GOTO:
    println("  jmp %s", node->unique_label);
    return;
  case ND_LABEL:
    println("%s:", node->unique_label);
    gen_stmt(node->lhs);
    return;
  case ND_RETURN:
    if (node->lhs)
      gen_expr(node->lhs);
    println("  jmp .L.return.%s", current_fn->name);
    return;
  case ND_EXPR_STMT:
    gen_expr(node->lhs);
    return;
  }

  error_tok(node->tok, "invalid statement");
}

static void assign_lvar_offsets(Obj *prog)
{
  for (Obj *fn = prog; fn; fn = fn->next)
  {
    if (!fn->is_function)
    {
      continue;
    }

    int offset = 0;
    for (Obj *var = fn->locals; var; var = var->next)
    {
      offset += var->ty->size;
      offset = align_to(offset, var->align);
      var->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);
  }
}

static void emit_data(Obj *prog)
{
  for (Obj *var = prog; var; var = var->next)
  {
    if (var->is_function || !var->is_definition)
      continue;

    if (var->is_static)
      println("  .local %s", var->name);
    else
      println("  .globl %s", var->name);
    println("  .align %d", var->align);
    if (var->init_data)
    {
      println("  .data");
      println("%s:", var->name);

      Relocation *rel = var->rel;
      int pos = 0;
      while (pos < var->ty->size)
      {
        if (rel && rel->offset == pos)
        {
          println("  .quad %s%+ld", rel->label, rel->addend);
          rel = rel->next;
          pos += 8;
        }
        else
        {
          println("  .byte %d", var->init_data[pos++]);
        }
      }
      continue;
    }

    println("  .bss");
    println("%s:", var->name);
    println("  .zero %d", var->ty->size);
  }
}

static void emit_text(Obj *prog)
{
  for (Obj *fn = prog; fn; fn = fn->next)
  {
    if (!fn->is_function || !fn->is_definition)
    {
      continue;
    }
    println(".intel_syntax noprefix");
    if (fn->is_static)
      println(".local %s", fn->name);
    else
      println(".globl %s", fn->name);
    println(".text");
    println("%s:", fn->name);
    current_fn = fn;

    // prologue
    println("  push rbp");
    println("  mov rbp, rsp");
    println("  sub rsp, %d", fn->stack_size);

    // Save arg registers if function is variadic
    if (fn->va_area) {
      int gp = 0;
      for (Obj *var = fn->params; var; var = var->next)
        gp++;
      int off = fn->va_area->offset;

      // va_elem
      println("  mov dword ptr %d[rbp], %d", off, gp * 8);
      println("  mov dword ptr %d[rbp], 0", off + 4);
      println("  movq %d[rbp], rbp", off + 16);
      println("  addq %d[rbp], %d", off + 16, off + 24);

      // __reg_save_area__
      println("  movq %d[rbp], rdi", off + 24);
      println("  movq %d[rbp], rsi", off + 32);
      println("  movq %d[rbp], rdx", off + 40);
      println("  movq %d[rbp], rcx", off + 48);
      println("  movq %d[rbp], r8", off + 56);
      println("  movq %d[rbp], r9", off + 64);
      println("  movsd %d[rbp], xmm0", off + 72);
      println("  movsd %d[rbp], xmm1", off + 80);
      println("  movsd %d[rbp], xmm2", off + 88);
      println("  movsd %d[rbp], xmm3", off + 96);
      println("  movsd %d[rbp], xmm4", off + 104);
      println("  movsd %d[rbp], xmm5", off + 112);
      println("  movsd %d[rbp], xmm6", off + 120);
      println("  movsd %d[rbp], xmm7", off + 128);
    }

    // Save passed-by-register arguments to the stack
    int i = 0;
    for (Obj *var = fn->params; var; var = var->next)
      store_gp(i++, var->offset, var->ty->size);

    gen_stmt(fn->body);
    assert(depth == 0);

    println(".L.return.%s:", fn->name);
    println("  mov rsp, rbp");
    println("  pop rbp");
    println("  ret");
  }
}

void codegen(Obj *prog, FILE *out)
{
  output_file = out;

  assign_lvar_offsets(prog);
  emit_data(prog);
  emit_text(prog);
}