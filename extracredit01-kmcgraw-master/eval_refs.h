#ifndef EVAL_REFS_H
#define EVAL_REFS_H

#include "types.h"

/* These are references held by the evaluation engine to the singletons
 * None, True, and False. These references should never be collected
 * since they are always considered globals and should never change because
 * reference numbers should never change! */
extern reference_t NONE_REF;
extern reference_t TRUE_REF;
extern reference_t FALSE_REF;

reference_t make_reference_none(void);
reference_t make_reference_bool(void);
reference_t make_reference_int(int64_t value);
reference_t make_reference_string(const char *value);
reference_t make_reference_string_concat(char *value1, char *value2);
reference_t make_reference_list(reference_t values);
reference_t make_reference_dict(reference_t keys, reference_t values);
reference_t make_reference_refarray(size_t capacity);

#endif /* EVAL_REFS_H */
