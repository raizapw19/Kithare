/*
 * This file is a part of the Kithare programming language source code.
 * The source code for Kithare programming language is distributed under the MIT license,
 *     and it is available as a repository at https://github.com/Kithare/Kithare
 * Copyright (C) 2021 Kithare Organization at https://www.kithare.de
 */

#include <math.h>
#include <string.h>
#include <wctype.h>

#include <kithare/lexer.h>
#include <kithare/string.h>


#define ERROR(MSG)                                                                   \
    if (errors) {                                                                    \
        khArray_khLexError_push(errors, (khLexError){.ptr = *cursor, .error = MSG}); \
    }

#define ERROR_STR(MSG) ERROR(kh_string(MSG))


static inline uint8_t digitOf(char32_t chr) {
    // Regular decimal characters
    if (chr >= U'0' && chr <= U'9') {
        return chr - U'0';
    }
    // Uppercase and lowercase hex characters
    else if (chr >= U'A' && chr <= U'Z') {
        return chr - U'A' + 10;
    }
    else if (chr >= U'a' && chr <= U'z') {
        return chr - U'a' + 10;
    }
    // 0xFF (255) as an unrecognized character
    else {
        return 0xFF;
    }
}

khToken kh_lex(char32_t** cursor, khArray_khLexError* errors) {
    // Skips any whitespace
    while (iswspace(**cursor)) {
        (*cursor)++;
    }

    if (iswalpha(**cursor)) {
        if (**cursor == U'b' || **cursor == U'B') {
            (*cursor)++;

            switch (**cursor) {
                // Byte characters: b'X'
                case U'\'':
                    return khToken_fromByte(kh_lexChar(cursor, true, true, errors));

                // Buffers: b"1234"
                case U'"': {
                    khArray_char string = kh_lexString(cursor, true, errors);
                    khArray_byte buffer = khArray_byte_new();
                    khArray_byte_reserve(&buffer, string.size);

                    for (char32_t* chr = string.array; chr < string.array + string.size; chr++) {
                        khArray_byte_push(&buffer, *chr);
                    }

                    khArray_char_delete(&string);
                    return khToken_fromBuffer(buffer);
                }

                default:
                    (*cursor)--;
                    return kh_lexWord(cursor, errors);
            }
        }
        else {
            return kh_lexWord(cursor, errors);
        }
    }
    else if (digitOf(**cursor) < 10) {
        return kh_lexNumber(cursor, errors);
    }
    else if (**cursor == U'\'') {
        return khToken_fromChar(kh_lexChar(cursor, true, false, errors));
    }
    else if (**cursor == U'"') {
        return khToken_fromString(kh_lexString(cursor, false, errors));
    }
    else if (**cursor == U'#') {
        (*cursor)++;

        for (; !(**cursor == U'\n' || **cursor == U'\0'); (*cursor)++) {}
        if (**cursor == U'\n') {
            (*cursor)++;
        }

        return khToken_fromComment();
    }
    else {
        return kh_lexSymbol(cursor, errors);
    }
}

khToken kh_lexWord(char32_t** cursor, khArray_khLexError* errors) {
    char32_t* origin = *cursor;

    // Passes through alphanumeric characters in a row
    while (iswalnum(**cursor)) {
        (*cursor)++;
    }

    khArray_char identifier = khArray_char_fromMemory(origin, *cursor - origin);

#define CASE_OPERATOR(STRING, OPERATOR)           \
    if (kh_compareCstring(&identifier, STRING)) { \
        khArray_char_delete(&identifier);         \
        return khToken_fromOperator(OPERATOR);    \
    }

    CASE_OPERATOR(U"not", khOperatorToken_NOT);
    CASE_OPERATOR(U"and", khOperatorToken_AND);
    CASE_OPERATOR(U"or", khOperatorToken_OR);
    CASE_OPERATOR(U"xor", khOperatorToken_XOR);

#define CASE_KEYWORD(STRING, KEYWORD)             \
    if (kh_compareCstring(&identifier, STRING)) { \
        khArray_char_delete(&identifier);         \
        return khToken_fromKeyword(KEYWORD);      \
    }

    CASE_KEYWORD(U"import", khKeywordToken_IMPORT);
    CASE_KEYWORD(U"include", khKeywordToken_INCLUDE);
    CASE_KEYWORD(U"as", khKeywordToken_AS);
    CASE_KEYWORD(U"try", khKeywordToken_TRY);
    CASE_KEYWORD(U"def", khKeywordToken_DEF);
    CASE_KEYWORD(U"class", khKeywordToken_CLASS);
    CASE_KEYWORD(U"struct", khKeywordToken_STRUCT);
    CASE_KEYWORD(U"enum", khKeywordToken_ENUM);
    CASE_KEYWORD(U"alias", khKeywordToken_ALIAS);

    CASE_KEYWORD(U"ref", khKeywordToken_REF);
    CASE_KEYWORD(U"public", khKeywordToken_PUBLIC);
    CASE_KEYWORD(U"private", khKeywordToken_PRIVATE);
    CASE_KEYWORD(U"static", khKeywordToken_STATIC);

    CASE_KEYWORD(U"if", khKeywordToken_IF);
    CASE_KEYWORD(U"elif", khKeywordToken_ELIF);
    CASE_KEYWORD(U"else", khKeywordToken_ELSE);
    CASE_KEYWORD(U"for", khKeywordToken_FOR);
    CASE_KEYWORD(U"while", khKeywordToken_WHILE);
    CASE_KEYWORD(U"do", khKeywordToken_DO);
    CASE_KEYWORD(U"break", khKeywordToken_BREAK);
    CASE_KEYWORD(U"continue", khKeywordToken_CONTINUE);
    CASE_KEYWORD(U"return", khKeywordToken_RETURN);

#undef CASE_OPERATOR
#undef CASE_KEYWORD

    return khToken_fromIdentifier(identifier);
}

khToken kh_lexNumber(char32_t** cursor, khArray_khLexError* errors) {
    if (digitOf(**cursor) > 9) {
        ERROR_STR(U"expecting a decimal number, from 0 to 9");
        return khToken_fromNone();
    }

    uint8_t base = 10;
    if (**cursor == U'0') {
        (*cursor)++;

        switch (*(*cursor)++) {
            // Binary, 0b...
            case U'b':
            case U'B':
                base = 2;
                break;

            // Octal, 0o...
            case U'o':
            case U'O':
                base = 8;
                break;

            // Hexadecimal, 0x...
            case U'x':
            case U'X':
                base = 16;
                break;

            default:
                (*cursor) -= 2;
                break;
        }
    }

    char32_t* origin = *cursor;
    bool had_overflowed;
    uint64_t integer = kh_lexInt(cursor, base, -1, &had_overflowed);

    // When it didn't lex any characters (presumably because it's out of base)
    if (*cursor == origin) {
        switch (base) {
            case 2:
                ERROR_STR(U"expecting a binary number, either 0 or 1");
                break;

            case 8:
                ERROR_STR(U"expecting an octal number, from 0 to 7");
                break;

            case 10:
                ERROR_STR(U"expecting a decimal number, from 0 to 9");
                break;

            case 16:
                ERROR_STR(U"expecting a hexadecimal number, from 0 to 9 or A to F");
                break;
        }

        return khToken_fromNone();
    }
    // If it was a floating point
    else if (**cursor == U'e' || **cursor == U'E' || **cursor == U'p' || **cursor == U'P' ||
             **cursor == U'.' || had_overflowed) {
        *cursor = origin;
        double floating = kh_lexFloat(cursor, base);

        switch (*(*cursor)++) {
            // 4.f -> float(4.0)
            case U'f':
            case U'F':
                return khToken_fromFloat(floating);

            // 4.d -> double(4.0)
            case U'd':
            case U'D':
                return khToken_fromDouble(floating);

            // Imaginary floating points
            case U'i':
            case U'I':
                switch (*(*cursor)++) {
                    // 5.if -> cfloat(0.0, 5.0)
                    case U'f':
                    case U'F':
                        return khToken_fromIfloat(floating);

                    // 5.id -> cdouble(0.0, 5.0)
                    case U'd':
                    case U'D':
                        return khToken_fromIdouble(floating);

                    // Defaults to a cdouble, without any extra suffixes
                    default:
                        (*cursor)--;
                        return khToken_fromIdouble(floating);
                }

            // Defaults to a double, without any suffixes
            default:
                (*cursor)--;
                return khToken_fromDouble(floating);
        }
    }
    else {
        switch (*(*cursor)++) {
            // 1b -> byte(1)
            case U'b':
            case U'B':
                return khToken_fromByte(integer);

            // 1s -> short(1)
            case U's':
            case U'S':
                switch (*(*cursor)++) {
                    // 2sb -> sbyte(2)
                    case U'b':
                    case U'B':
                        return khToken_fromSbyte(integer);

                    // 2ss -> short(2)
                    case U's':
                    case U'S':
                        return khToken_fromShort(integer);

                    // 2sl -> long(2)
                    case U'l':
                    case U'L':
                        return khToken_fromLong(integer);

                    // Defaults to an int16 (signed short), without any extra suffixes
                    default:
                        (*cursor)--;
                        return khToken_fromShort(integer);
                }

            // 1l -> long(1)
            case U'l':
            case U'L':
                return khToken_fromLong(integer);

            // Unsigned integers
            case U'u':
            case U'U':
                switch (*(*cursor)++) {
                    // 2ub -> byte(2)
                    case U'b':
                    case U'B':
                        return khToken_fromByte(integer);

                    // 2us -> ushort(2)
                    case U's':
                    case U'S':
                        return khToken_fromUshort(integer);

                    // 2ul -> ulong(2)
                    case U'l':
                    case U'L':
                        return khToken_fromUlong(integer);

                    // Defaults to an uint32 (unsigned int), without any extra suffixes
                    default:
                        (*cursor)--;
                        return khToken_fromUint(integer);
                }

            // 4f -> float(4.0)
            case U'f':
            case U'F':
                return khToken_fromFloat(integer);

            // 4d -> double(4.0)
            case U'd':
            case U'D':
                return khToken_fromDouble(integer);

            // Imaginary floating points
            case U'i':
            case U'I':
                switch (*(*cursor)++) {
                    // 5if -> cfloat(0.0, 5.0)
                    case U'f':
                    case U'F':
                        return khToken_fromIfloat(integer);

                    // 5id -> cdouble(0.0, 5.0)
                    case U'd':
                    case U'D':
                        return khToken_fromIdouble(integer);

                    // Defaults to an imaginary double, without any extra suffixes
                    default:
                        (*cursor)--;
                        return khToken_fromIdouble(integer);
                }

            // Defaults to an int32 (unsigned int), without any suffixes
            default:
                (*cursor)--;
                return khToken_fromInt(integer);
        }
    }
}

khToken kh_lexSymbol(char32_t** cursor, khArray_khLexError* errors) {
#define CASE_DELIMITER(CHR, DELIMITER) \
    case CHR:                          \
        return khToken_fromDelimiter(DELIMITER)

    switch (*(*cursor)++) {
        // Pretty repetitive stuff here. Macros are utilized
        CASE_DELIMITER(',', khDelimiterToken_COMMA);
        CASE_DELIMITER(':', khDelimiterToken_COLON);
        CASE_DELIMITER(';', khDelimiterToken_SEMICOLON);

        CASE_DELIMITER('(', khDelimiterToken_PARENTHESES_OPEN);
        CASE_DELIMITER(')', khDelimiterToken_PARENTHESES_CLOSE);
        CASE_DELIMITER('{', khDelimiterToken_CURLY_BRACKET_OPEN);
        CASE_DELIMITER('}', khDelimiterToken_CURLY_BRACKET_CLOSE);
        CASE_DELIMITER('[', khDelimiterToken_SQUARE_BRACKET_OPEN);
        CASE_DELIMITER(']', khDelimiterToken_CURLY_BRACKET_CLOSE);

        case U'.':
            if ((*cursor)[0] == U'.' && (*cursor)[1] == U'.') {
                *cursor += 2;
                return khToken_fromDelimiter(khDelimiterToken_ELLIPSIS);
            }
            else {
                return khToken_fromDelimiter(khDelimiterToken_DOT);
            }

        // Down here are where many multiple-character-operators are handled
        case U'+':
            switch (*(*cursor)++) {
                case U'+':
                    return khToken_fromOperator(khOperatorToken_INCREMENT);

                case U'=':
                    return khToken_fromOperator(khOperatorToken_IADD);

                default:
                    (*cursor)--;
                    return khToken_fromOperator(khOperatorToken_ADD);
            }

        case U'-':
            switch (*(*cursor)++) {
                case U'-':
                    return khToken_fromOperator(khOperatorToken_DECREMENT);

                case U'=':
                    return khToken_fromOperator(khOperatorToken_ISUB);

                case U'>':
                    return khToken_fromDelimiter(khDelimiterToken_ARROW);

                default:
                    (*cursor)--;
                    return khToken_fromOperator(khOperatorToken_SUB);
            }

        case U'*':
            if (**cursor == U'=') {
                (*cursor)++;
                return khToken_fromOperator(khOperatorToken_IMUL);
            }
            else {
                return khToken_fromOperator(khOperatorToken_MUL);
            }

        case U'/':
            if (**cursor == U'=') {
                (*cursor)++;
                return khToken_fromOperator(khOperatorToken_IDIV);
            }
            else {
                return khToken_fromOperator(khOperatorToken_DIV);
            }

        case U'%':
            if (**cursor == U'=') {
                (*cursor)++;
                return khToken_fromOperator(khOperatorToken_IMODULO);
            }
            else {
                return khToken_fromOperator(khOperatorToken_MODULO);
            }

        case U'^':
            if (**cursor == U'=') {
                (*cursor)++;
                return khToken_fromOperator(khOperatorToken_IPOWER);
            }
            else {
                return khToken_fromOperator(khOperatorToken_POWER);
            }

        case U'=':
            if (**cursor == U'=') {
                (*cursor)++;
                return khToken_fromOperator(khOperatorToken_EQUALS);
            }
            else {
                return khToken_fromOperator(khOperatorToken_ASSIGN);
            }

        case U'@':
            return khToken_fromOperator(khOperatorToken_ID);

        case U'!':
            if (**cursor == U'=') {
                (*cursor)++;
                return khToken_fromOperator(khOperatorToken_NOT_EQUAL);
            }
            else {
                return khToken_fromDelimiter(khDelimiterToken_EXCLAMATION);
            }

        case U'<':
            switch (*(*cursor)++) {
                case U'=':
                    return khToken_fromOperator(khOperatorToken_ELESS);

                case U'<':
                    if (**cursor == U'=') {
                        (*cursor)++;
                        return khToken_fromOperator(khOperatorToken_IBIT_LSHIFT);
                    }
                    else {
                        return khToken_fromOperator(khOperatorToken_BIT_LSHIFT);
                    }

                default:
                    (*cursor)--;
                    return khToken_fromOperator(khOperatorToken_LESS);
            }

        case U'>':
            switch (*(*cursor)++) {
                case U'=':
                    return khToken_fromOperator(khOperatorToken_EMORE);

                case U'<':
                    if (**cursor == U'=') {
                        (*cursor)++;
                        return khToken_fromOperator(khOperatorToken_IBIT_RSHIFT);
                    }
                    else {
                        return khToken_fromOperator(khOperatorToken_BIT_RSHIFT);
                    }

                default:
                    (*cursor)--;
                    return khToken_fromOperator(khOperatorToken_MORE);
            }

        case U'~':
            if (**cursor == U'=') {
                (*cursor)++;
                return khToken_fromOperator(khOperatorToken_IBIT_XOR);
            }
            else {
                // It's also khOperatorToken_BIT_XOR
                return khToken_fromOperator(khOperatorToken_BIT_OR);
            }

        case U'&':
            if (**cursor == U'=') {
                (*cursor)++;
                return khToken_fromOperator(khOperatorToken_IBIT_AND);
            }
            else {
                return khToken_fromOperator(khOperatorToken_BIT_AND);
            }

        case U'|':
            if (**cursor == U'=') {
                (*cursor)++;
                return khToken_fromOperator(khOperatorToken_IBIT_OR);
            }
            else {
                return khToken_fromOperator(khOperatorToken_BIT_OR);
            }

        // Unexpected null-terminator
        case U'\0':
            (*cursor)--;
            ERROR_STR(U"expecting a token, met with a dead end");
            return khToken_fromNone();

        default:
            (*cursor)--;
            ERROR_STR(U"unknown character");
            return khToken_fromNone();
    }
#undef CASE_DELIMITER
}

char32_t kh_lexChar(char32_t** cursor, bool with_quotes, bool is_byte, khArray_khLexError* errors) {
    char32_t chr = 0;

    if (with_quotes) {
        if (**cursor == U'\'') {
            (*cursor)++;
        }
        else {
            ERROR_STR(U"expecting a single quote opening for a character");
        }
    }

    // Handle escapes
    if (**cursor == U'\\') {
        (*cursor)++;

        switch (*(*cursor)++) {
            // Handle many single character escapes
            case U'0':
                chr = U'\0';
                break;
            case U'n':
                chr = U'\n';
                break;
            case U'r':
                chr = U'\r';
                break;
            case U't':
                chr = U'\t';
                break;
            case U'v':
                chr = U'\v';
                break;
            case U'b':
                chr = U'\b';
                break;
            case U'a':
                chr = U'\a';
                break;
            case U'f':
                chr = U'\f';
                break;
            case U'\\':
                chr = U'\\';
                break;
            case U'\'':
                chr = U'\'';
                break;
            case U'"':
                chr = U'"';
                break;

            // \xAA
            case U'x': {
                char32_t* origin = *cursor;

                chr = kh_lexInt(cursor, 16, 2, NULL);
                if (*cursor != origin + 2) {
                    ERROR_STR(U"expecting 2 hexadecimal digits for 1 byte character, from 0 to 9 or "
                              U"A to F");
                }

                break;
            }

            // \uAABB
            case U'u': {
                if (is_byte) {
                    (*cursor)--;
                    ERROR_STR(
                        U"only allowing one byte characters, 2 byte unicode escapes are not allowed");
                    break;
                }

                char32_t* origin = *cursor;

                chr = kh_lexInt(cursor, 16, 4, NULL);
                if (*cursor != origin + 4) {
                    ERROR_STR(
                        U"expecting 4 hexadecimal digits for 2 byte unicode character, from 0 to 9 or "
                        U"A to F");
                }

                break;
            }

            // \UAABBCCDD
            case U'U': {
                if (is_byte) {
                    (*cursor)--;
                    ERROR_STR(
                        U"only allowing one byte characters, 4 byte unicode escapes are not allowed");
                    break;
                }

                char32_t* origin = *cursor;

                chr = kh_lexInt(cursor, 16, 8, NULL);
                if (*cursor != origin + 8) {
                    ERROR_STR(
                        U"expecting 8 hexadecimal digits for 4 byte unicode character, from 0 to 9 or "
                        U"A to F");
                }

                break;
            }

            // Unexpected null-terminator
            case U'\0':
                (*cursor)--;
                ERROR_STR(U"expecting a backslash escape character, met with a dead end");
                return chr;

            // Unrecognized escape character
            default:
                (*cursor)--;
                ERROR_STR(U"unknown backslash escape character");
                break;
        }
    }
    else {
        switch (**cursor) {
            // Encourage users to use U'\'' instead
            case U'\'':
                if (with_quotes) {
                    ERROR_STR(U"a character cannot be closed empty, did you mean U'\\''");
                }
                break;

            // Encourage users to use U'\n' instead
            case U'\n':
                ERROR_STR(U"a newline instead of an inline character, did you mean U'\\n'");
                break;

            // Unexpected null-terminator
            case U'\0':
                ERROR_STR(U"expecting a character, met with a dead end");
                return chr;

            default:
                chr = **cursor;
                if (is_byte && chr > 255) {
                    ERROR_STR(U"only allowing one byte characters, unicode character is forbidden");
                }
                else {
                    (*cursor)++;
                }

                break;
        }
    }

    if (with_quotes) {
        if (**cursor == U'\'') {
            (*cursor)++;
        }
        else {
            ERROR_STR(U"expecting a single quote closing of the character");
        }
    }

    return chr;
}

khArray_char kh_lexString(char32_t** cursor, bool is_buffer, khArray_khLexError* errors) {
    khArray_char string = khArray_char_new();
    bool multiline = false;

    if (**cursor == U'"') {
        (*cursor)++;

        // For multiline strings, uses triple double quotes
        if ((*cursor)[0] == U'"' && (*cursor)[1] == U'"') {
            *cursor += 2;
            multiline = true;
        }
    }
    else {
        ERROR_STR(U"expecting a double quote for a string");
    }

    while (true) {
        switch (**cursor) {
            // End string
            case U'"':
                if (multiline) {
                    if ((*cursor)[1] == U'"' && (*cursor)[2] == U'"') {
                        *cursor += 3;
                        return string;
                    }
                    else {
                        (*cursor)++;
                        khArray_char_push(&string, U'"');
                    }
                }
                else {
                    (*cursor)++;
                    return string;
                }
                break;

            // Explicitly handle U'\n', separate from kh_lexChar
            case U'\n':
                if (multiline) {
                    (*cursor)++;
                    khArray_char_push(&string, U'\n');
                }
                else {
                    ERROR_STR(U"a newline instead of an inline character, use U'\\n' or a multiline "
                              U"string instead");
                }
                break;

            // Unexpected null-terminator
            case U'\0':
                ERROR_STR(U"expecting a character, met with a dead end");
                return string;

            // Use kh_lexChar for other character encounters
            default:
                khArray_char_push(&string, kh_lexChar(cursor, false, is_buffer, errors));
                break;
        }
    }

    return string;
}

uint64_t kh_lexInt(char32_t** cursor, uint8_t base, size_t max_length, bool* had_overflowed) {
    uint64_t result = 0;

    if (had_overflowed != NULL) {
        *had_overflowed = false;
    }

    while (digitOf(**cursor) < base && max_length > 0) {
        uint64_t previous = result;
        result *= base; // Shift left of the base: 123 -> 1230 (decimal)
        result += digitOf(**cursor);

        // Simple overflow check
        if (result < previous && had_overflowed != NULL) {
            *had_overflowed = true;
        }

        (*cursor)++;
        max_length--;
    }

    return result;
}

double kh_lexFloat(char32_t** cursor, uint8_t base) {
    double result = 0;

    // The same implementation of kh_lexInt is used here. The reason of not using kh_lexInt directly
    // is to avoid any integer overflows
    while (digitOf(**cursor) < base) {
        result *= base;
        result += digitOf(**cursor);
        (*cursor)++;
    }

    // Where the main show begins
    if (**cursor == U'.') {
        (*cursor)++;
        double exponent = 1.l / base;

        while (digitOf(**cursor) < base) {
            result += digitOf(**cursor) * exponent;
            exponent /= base; // Shift right of the base: 0.01 -> 0.001 (decimal)
            (*cursor)++;
        }
    }

    bool had_overflowed;
    switch (**cursor) {
        // 2e3 -> 2 * 10^3 -> 2000
        case U'e':
        case U'E':
            (*cursor)++;

            // Negative exponent
            if (**cursor == U'-') {
                (*cursor)++;

                result *= pow(10, -kh_lexInt(cursor, 10, -1, &had_overflowed));
                if (had_overflowed) {
                    result = 0.0;
                }
            }
            else {
                if (**cursor == U'+') {
                    (*cursor)++;
                }

                result *= pow(10, kh_lexInt(cursor, 10, -1, &had_overflowed));
                if (had_overflowed) {
                    result = INFINITY;
                }
            }

            break;

        // 2p3 -> 2 * 2^3 -> 16
        case U'p':
        case U'P':
            (*cursor)++;

            // Negative exponent
            if (**cursor == U'-') {
                (*cursor)++;

                result *= pow(2, -kh_lexInt(cursor, 10, -1, &had_overflowed));
                if (had_overflowed) {
                    result = 0.0;
                }
            }
            else {
                if (**cursor == U'+') {
                    (*cursor)++;
                }

                result *= pow(2, kh_lexInt(cursor, 10, -1, &had_overflowed));
                if (had_overflowed) {
                    result = INFINITY;
                }
            }

            break;
    }

    return result;
}