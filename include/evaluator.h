#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "expression.h"
#include "schema.h"

bool evaluate(Expression* expr, void* record_data, TableSchema* schema);

#endif
