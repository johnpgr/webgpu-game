#pragma once

#include "base/base_arena.h"

struct String {
    u8* str;
    u64 size;
};

String string_substring(String source, u64 start, u64 end);
bool string_equals(String a, String b);
i32 string_compare(String a, String b);
String string_lit(const char* s);
String string_from_cstr(const char* s);
String string_copy(Arena* arena, String source);
const char* string_to_cstr(Arena* arena, String source);
String string_copy_cstr(Arena* arena, const char* source);
String string_fmt(Arena* arena, const char* format, ...) PRINTF_FORMAT(2, 3);
String string_concat(Arena* arena, String a, String b);
