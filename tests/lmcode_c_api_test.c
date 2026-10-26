#include "lmcode_c.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add(void *user, const lmcode_value *args, size_t count,
               lmcode_value *result, lmcode_error *error) {
  (void)user;
  (void)error;
  assert(count == 2 && args[0].kind == LMCODE_INT &&
         args[1].kind == LMCODE_INT);
  result->kind = LMCODE_INT;
  result->i = args[0].i + args[1].i;
  return 0;
}

static int record_bool(void *user, void *self, const lmcode_value *args,
                       size_t count, lmcode_value *result,
                       lmcode_error *error) {
  (void)self;
  (void)result;
  (void)error;
  assert(count == 1 && args[0].kind == LMCODE_BOOL);
  *(int *)user += args[0].b ? 1 : 0;
  return 0;
}
static int twice(void *user, const lmcode_value *args, size_t count,
                 lmcode_value *result, lmcode_error *error) {
  (void)error;
  assert(count == 1);
  *(int *)user += 1;
  if (args[0].kind == LMCODE_INT) {
    result->kind = LMCODE_INT;
    result->i = args[0].i * 2;
  } else {
    assert(args[0].kind == LMCODE_FLOAT);
    result->kind = LMCODE_FLOAT;
    result->f = args[0].f * 2.0;
  }
  return 0;
}
static int record_string(void *user, void *self, const lmcode_value *args,
                         size_t count, lmcode_value *result,
                         lmcode_error *error) {
  (void)self;
  (void)result;
  (void)error;
  assert(count == 1 && args[0].kind == LMCODE_STRING);
  *(int *)user += (int)args[0].length;
  return 0;
}
static int fail(void *user, const lmcode_value *args, size_t count,
                lmcode_value *result, lmcode_error *error) {
  (void)user;
  (void)args;
  (void)count;
  (void)result;
  error->code = "RUN_HOST";
  error->message = "located host";
  error->line = 3;
  error->column = 3;
  error->has_location = 1;
  error->has_column = 1;
  return 1;
}
static int bytes(void *user, const lmcode_value *args, size_t count,
                 lmcode_value *result, lmcode_error *error) {
  (void)user;
  (void)error;
  assert(count == 1 && args[0].kind == LMCODE_STRING);
  static char decoded[64];
  size_t n = args[0].length;
  assert(n % 2 == 0 && n / 2 < sizeof(decoded));
  for (size_t i = 0; i < n; i += 2) {
    unsigned hi =
        (unsigned)(args[0].data[i] <= '9' ? args[0].data[i] - '0'
                                          : args[0].data[i] - 'a' + 10);
    unsigned lo =
        (unsigned)(args[0].data[i + 1] <= '9' ? args[0].data[i + 1] - '0'
                                              : args[0].data[i + 1] - 'a' + 10);
    decoded[i / 2] = (char)(hi * 16 + lo);
  }
  result->kind = LMCODE_STRING;
  result->data = decoded;
  result->length = n / 2;
  return 0;
}
static int printed(void *user, const lmcode_value *args, size_t count,
                   lmcode_value *result, lmcode_error *error) {
  (void)args;
  (void)error;
  assert(count == 0);
  struct bytes {
    char data[64];
    size_t length;
  } *sink = (struct bytes *)user;
  result->kind = LMCODE_STRING;
  result->data = sink->data;
  result->length = sink->length;
  return 0;
}
static int hex(void *user, void *self, const lmcode_value *args, size_t count,
               lmcode_value *result, lmcode_error *error) {
  (void)self;
  (void)result;
  (void)error;
  assert(count == 1 && args[0].kind == LMCODE_STRING);
  unsigned char *seen = (unsigned char *)user;
  *seen = 1;
  assert(args[0].length == 8 && args[0].data[0] == (char)0xff &&
         args[0].data[3] == '\n');
  return 0;
}
static void print_bytes(void *user, const char *data, size_t length) {
  struct bytes {
    char data[64];
    size_t length;
  } *sink = (struct bytes *)user;
  assert(sink->length + length <= sizeof(sink->data));
  memcpy(sink->data + sink->length, data, length);
  sink->length += length;
}

static char *read_file(const char *path, size_t *length) {
  FILE *file = fopen(path, "rb");
  long size;
  assert(file);
  assert(fseek(file, 0, SEEK_END) == 0);
  size = ftell(file);
  assert(size >= 0);
  rewind(file);
  char *data = (char *)malloc((size_t)size);
  assert(data);
  assert(fread(data, 1, (size_t)size, file) == (size_t)size);
  fclose(file);
  *length = (size_t)size;
  return data;
}

int main(void) {
  lmcode_error error = {0};
  lmcode_registry *registry = lmcode_registry_create();
  assert(registry);
  const char *params[] = {"int", "int"};
  assert(lmcode_registry_function(registry, "add", params, 2, "int", add, 0,
                                  &error) == 0);
  assert(lmcode_registry_signature_count(registry) > 0);
  assert(lmcode_registry_context(registry, "root", "Thing", &error) == 0);
  int trues = 0, overloads = 0, strings = 0;
  const char *bool_params[] = {"bool"};
  assert(lmcode_registry_context(registry, "t", "Trace", &error) == 0);
  assert(lmcode_registry_method(registry, "Trace", "bool", bool_params, 1,
                                "void", record_bool, &trues, &error) == 0);
  assert(lmcode_registry_method(registry, "Trace", "str",
                                (const char *[]){"string"}, 1, "void",
                                record_string, &strings, &error) == 0);
  assert(lmcode_registry_function(registry, "twice", (const char *[]){"int"}, 1,
                                  "int", twice, &overloads, &error) == 0);
  assert(lmcode_registry_function(registry, "twice", (const char *[]){"float"},
                                  1, "float", twice, &overloads, &error) == 0);
  lmcode_context *context = lmcode_context_create();
  assert(context);
  assert(lmcode_context_set(context, "root", (void *)0x1, &error) == 0);
  assert(lmcode_context_set(context, "t", (void *)0x2, &error) == 0);
  lmcode_program *program = 0;
  const char bad[] = "not bytecode";
  const char schema[] = "{}";
  assert(lmcode_program_load(bad, sizeof bad - 1, schema, sizeof schema - 1,
                             registry, &program, &error) != 0);
  assert(program == 0 && error.code &&
         strcmp(error.code, "LOAD_BAD_BYTECODE") == 0);
  size_t bytecode_length, schema_length;
  char *bytecode =
      read_file("conformance/vectors/compare.dslb", &bytecode_length);
  char *schema_bytes =
      read_file("conformance/vectors/schema.json", &schema_length);
  assert(lmcode_program_load(bytecode, bytecode_length, schema_bytes,
                             schema_length, registry, &program, &error) == 0);
  assert(lmcode_program_run(program, context, &error) == 0 && trues == 9);
  lmcode_program_destroy(program);
  free(bytecode);
  free(schema_bytes);
  bytecode =
      read_file("conformance/vectors/if-else-chain.dslb", &bytecode_length);
  schema_bytes = read_file("conformance/vectors/schema.json", &schema_length);
  assert(lmcode_program_load(bytecode, bytecode_length, schema_bytes,
                             schema_length, registry, &program, &error) == 0);
  assert(lmcode_program_run(program, context, &error) == 0 && overloads == 1 &&
         strings == 19);
  lmcode_program_destroy(program);
  free(bytecode);
  free(schema_bytes);
  assert(lmcode_registry_function(registry, "fail", (const char *[]){"string"},
                                  1, "void", fail, 0, &error) == 0);
  bytecode = read_file("conformance/vectors/runtime-host-error-at.dslb",
                       &bytecode_length);
  schema_bytes = read_file("conformance/vectors/schema.json", &schema_length);
  assert(lmcode_program_load(bytecode, bytecode_length, schema_bytes,
                             schema_length, registry, &program, &error) == 0);
  assert(lmcode_program_run(program, context, &error) != 0 &&
         strcmp(error.code, "RUN_HOST") == 0 && error.has_location &&
         error.line == 3 && error.column == 3);
  lmcode_program_destroy(program);
  free(bytecode);
  free(schema_bytes);
  lmcode_registry *testing = lmcode_registry_create();
  assert(testing);
  unsigned char saw_hex = 0;
  struct {
    char data[64];
    size_t length;
  } sink = {{0}, 0};
  assert(lmcode_registry_context(testing, "t", "Trace", &error) == 0);
  assert(lmcode_registry_register_testing(testing, print_bytes, &sink,
                                          &error) == 0);
  assert(lmcode_registry_function(testing, "bytes", (const char *[]){"string"},
                                  1, "string", bytes, 0, &error) == 0);
  assert(lmcode_registry_function(testing, "printed", 0, 0, "string", printed,
                                  &sink, &error) == 0);
  assert(lmcode_registry_method(testing, "Trace", "hex",
                                (const char *[]){"string"}, 1, "void", hex,
                                &saw_hex, &error) == 0);
  bytecode = read_file("conformance/stdlib-vectors/testing-print.dslb",
                       &bytecode_length);
  schema_bytes = read_file("conformance/stdlib-vectors/collections.schema.json",
                           &schema_length);
  assert(lmcode_program_load(bytecode, bytecode_length, schema_bytes,
                             schema_length, testing, &program, &error) == 0);
  assert(lmcode_program_run(program, context, &error) == 0 && saw_hex == 1);
  lmcode_program_destroy(program);
  free(bytecode);
  free(schema_bytes);
  lmcode_registry_destroy(testing);
  lmcode_context_destroy(context);
  lmcode_registry_destroy(registry);
  return lmcode_report_ok_json("{\"c_api\":true}");
}
