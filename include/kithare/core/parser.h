/*
 * This file is a part of the Kithare programming language source code.
 * The source code for Kithare programming language is distributed under the MIT license,
 *     and it is available as a repository at https://github.com/Kithare/Kithare
 * Copyright (C) 2022 Kithare Organization at https://www.kithare.de
 */

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <kithare/core/ast.h>
#include <kithare/core/error.h>
#include <kithare/lib/array.h>


kharray(khAstStatement) kh_parse(khstring* string);

khAstStatement kh_parseStatement(char32_t** cursor);
khAstExpression kh_parseExpression(char32_t** cursor, bool ignore_newline, bool filter_type);


#ifdef __cplusplus
}
#endif
