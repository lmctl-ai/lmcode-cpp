#ifndef LMCODE_C_H
#define LMCODE_C_H

/* C ABI for lmcode-cpp's header-only DSL VM. Handles are opaque. Registry and
 * context handles must outlive programs/runs that use them. Callback argument
 * buffers are borrowed only during the callback; callback-returned strings and
 * type names must remain valid until the callback returns because the adapter
 * copies them at that boundary. Never return stack-local buffers. Object
 * pointers are borrowed for the run. */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lmcode_registry lmcode_registry;
typedef struct lmcode_context lmcode_context;
typedef struct lmcode_program lmcode_program;

typedef enum lmcode_value_kind {
  LMCODE_VOID = 0,
  LMCODE_INT = 1,
  LMCODE_FLOAT = 2,
  LMCODE_STRING = 3,
  LMCODE_BOOL = 4,
  LMCODE_OBJECT = 5
} lmcode_value_kind;
typedef struct lmcode_value {
  lmcode_value_kind kind;
  int64_t i;
  double f;
  int b;
  const char *data;
  size_t length;
  void *object;
  const char *type;
  size_t type_length;
} lmcode_value;

typedef struct lmcode_error {
  const char *code;
  const char *message;
  uint32_t line;
  uint16_t column;
  int has_location;
  int has_column;
} lmcode_error;

/* Return zero on success. On failure, out_error is filled with borrowed text
 * valid until the next call on the same handle (signature text is valid until
 * the next signature query or registry mutation). Callbacks return zero and
 * fill
 * result, or nonzero and fill error (message/code are copied immediately). */
typedef int (*lmcode_function_callback)(void *user, const lmcode_value *args,
                                        size_t count, lmcode_value *result,
                                        lmcode_error *error);
typedef int (*lmcode_method_callback)(void *user, void *self,
                                      const lmcode_value *args, size_t count,
                                      lmcode_value *result,
                                      lmcode_error *error);
/* Testing print receives every byte written by print(), including newlines and
 * embedded NUL bytes. The data pointer is borrowed only for this call. */
typedef void (*lmcode_print_callback)(void *user, const char *data,
                                      size_t length);

lmcode_registry *lmcode_registry_create(void);
lmcode_registry *lmcode_registry_create_without_std(void);
void lmcode_registry_destroy(lmcode_registry *registry);
int lmcode_registry_function(lmcode_registry *, const char *name,
                             const char *const *params, size_t count,
                             const char *returns, lmcode_function_callback,
                             void *user, lmcode_error *error);
int lmcode_registry_method(lmcode_registry *, const char *type,
                           const char *name, const char *const *params,
                           size_t count, const char *returns,
                           lmcode_method_callback, void *user,
                           lmcode_error *error);
int lmcode_registry_context(lmcode_registry *, const char *name,
                            const char *type, lmcode_error *error);
size_t lmcode_registry_signature_count(const lmcode_registry *);
const char *lmcode_registry_signature(const lmcode_registry *, size_t index);
int lmcode_registry_register_testing(lmcode_registry *, lmcode_print_callback,
                                     void *user, lmcode_error *error);
int lmcode_registry_call_function(lmcode_registry *, const char *signature,
                                  const lmcode_value *args, size_t count,
                                  lmcode_value *result, lmcode_error *error);
int lmcode_registry_call_method(lmcode_registry *, const char *signature,
                                void *self, const lmcode_value *args,
                                size_t count, lmcode_value *result,
                                lmcode_error *error);

lmcode_context *lmcode_context_create(void);
void lmcode_context_destroy(lmcode_context *);
int lmcode_context_set(lmcode_context *, const char *name, void *object,
                       lmcode_error *error);
int lmcode_context_set_file(lmcode_context *, const char *file,
                            lmcode_error *error);
int lmcode_context_set_dry(lmcode_context *, int dry, lmcode_error *error);
int lmcode_context_set_step_budget(lmcode_context *, uint64_t budget,
                                   lmcode_error *error);

int lmcode_program_load(const void *bytecode, size_t bytecode_length,
                        const void *schema, size_t schema_length,
                        const lmcode_registry *, lmcode_program **out,
                        lmcode_error *error);
int lmcode_program_run(const lmcode_program *, lmcode_context *,
                       lmcode_error *error);
void lmcode_program_destroy(lmcode_program *);

/* Standard one-line result helpers for C hosts. Return the process exit code.
 */
int lmcode_report_ok_json(const char *json);
int lmcode_report_error(const char *phase, const lmcode_error *error);

#ifdef __cplusplus
}
#endif
#endif
