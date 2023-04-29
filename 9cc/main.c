#include "9cc.h"

char *user_input;

int main(int argc, char **argv) {
  if (argc != 2) {
    error_at(token->str, "invalid input");
    return 1;
  }

  user_input = argv[1];
  token = tokenize();
  Function *prog = parse();
  codegen(prog);
  return 0;
}