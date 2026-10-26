#include "lmcode_c.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add(void *user, const lmcode_value *args, size_t count, lmcode_value *result, lmcode_error *error) {
  (void)user; (void)error;
  assert(count == 2 && args[0].kind == LMCODE_INT && args[1].kind == LMCODE_INT);
  result->kind = LMCODE_INT; result->i = args[0].i + args[1].i; return 0;
}

static int record_bool(void *user, void *self, const lmcode_value *args, size_t count, lmcode_value *result, lmcode_error *error) {
  (void)self; (void)result; (void)error;
  assert(count == 1 && args[0].kind == LMCODE_BOOL);
  *(int *)user += args[0].b ? 1 : 0;
  return 0;
}

static char *read_file(const char *path, size_t *length) {
  FILE *file = fopen(path, "rb"); long size;
  assert(file); assert(fseek(file, 0, SEEK_END) == 0); size = ftell(file); assert(size >= 0); rewind(file);
  char *data = (char *)malloc((size_t)size); assert(data); assert(fread(data, 1, (size_t)size, file) == (size_t)size); fclose(file); *length = (size_t)size; return data;
}

int main(void) {
  lmcode_error error = {0};
  lmcode_registry *registry = lmcode_registry_create();
  assert(registry);
  const char *params[] = {"int", "int"};
  assert(lmcode_registry_function(registry, "add", params, 2, "int", add, 0, &error) == 0);
  assert(lmcode_registry_signature_count(registry) > 0);
  assert(lmcode_registry_context(registry, "root", "Thing", &error) == 0);
  int trues = 0; const char *bool_params[] = {"bool"};
  assert(lmcode_registry_context(registry, "t", "Trace", &error) == 0);
  assert(lmcode_registry_method(registry, "Trace", "bool", bool_params, 1, "void", record_bool, &trues, &error) == 0);
  lmcode_context *context = lmcode_context_create(); assert(context);
  assert(lmcode_context_set(context, "root", (void *)0x1, &error) == 0);
  assert(lmcode_context_set(context, "t", (void *)0x2, &error) == 0);
  lmcode_program *program = 0;
  const char bad[] = "not bytecode"; const char schema[] = "{}";
  assert(lmcode_program_load(bad, sizeof bad - 1, schema, sizeof schema - 1, registry, &program, &error) != 0);
  assert(program == 0 && error.code && strcmp(error.code, "LOAD_BAD_BYTECODE") == 0);
  size_t bytecode_length, schema_length;
  char *bytecode = read_file("conformance/vectors/compare.dslb", &bytecode_length);
  char *schema_bytes = read_file("conformance/vectors/schema.json", &schema_length);
  assert(lmcode_program_load(bytecode, bytecode_length, schema_bytes, schema_length, registry, &program, &error) == 0);
  assert(lmcode_program_run(program, context, &error) == 0 && trues == 9);
  lmcode_program_destroy(program); free(bytecode); free(schema_bytes);
  lmcode_context_destroy(context); lmcode_registry_destroy(registry);
  return 0;
}
