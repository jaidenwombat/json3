/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef JSON4
#define JSON4

#ifdef __cplusplus

#include <cstdlib>

extern "C" {

struct JsonString;
struct JsonArray;
struct JsonObject;
struct JsonError;

#else

#include <stdlib.h>
#include <stdbool.h>

typedef enum JsonKind JsonKind;
typedef struct Json Json;

typedef struct JsonString JsonString;

typedef struct JsonArrayEntry JsonArrayEntry;
typedef struct JsonArray JsonArray;

typedef struct JsonObjectEntry JsonObjectEntry;
typedef struct JsonObject JsonObject;

typedef struct JsonError JsonError;

#endif

enum JsonKind {
	JSON_NULL = 0,
	JSON_BOOLEAN = 1,
	JSON_NUMBER = 2,
	JSON_STRING = 3,
	JSON_ARRAY = 4,
	JSON_OBJECT = 5,
	JSON_ERROR = 6
};

struct Json {
	JsonKind kind;
	union {
		bool boolean;
		double number;
		JsonString *string;
		JsonArray *array;
		JsonObject *object;
		JsonError *error;
	};
};

struct JsonString {
	char *text;
	// The size of the string in bytes
	size_t size;
};

struct JsonArrayEntry {
	JsonArrayEntry *next;
	Json *value;
};

struct JsonArray {
	// The number of items in the array
	size_t size;
	JsonArrayEntry *first;
};

struct JsonObjectEntry {
	JsonObjectEntry *next;
	char *key;
	// The size of the key string in bytes
	size_t key_size;
	Json *value;
};

struct JsonObject {
	// The number of items in the object
	size_t size;
	JsonObjectEntry *first;
};

struct JsonError {
	// Null terminated message string
	char const *message;
	size_t line;
};

Json *json_parse(char const *source_begin, size_t source_length);
Json *json_parse_file(char const *path);

#define json_is_null(json) ((json)->kind == JSON_NULL)
#define json_is_boolean(json) ((json)->kind == JSON_BOOLEAN)
#define json_is_number(json) ((json)->kind == JSON_NUMBER)
#define json_is_string(json) ((json)->kind == JSON_STRING)
#define json_is_array(json) ((json)->kind == JSON_ARRAY)
#define json_is_object(json) ((json)->kind == JSON_OBJECT)
#define json_is_error(json) ((json)->kind == JSON_ERROR)

#ifdef __cplusplus
}
#endif

#endif // JSON4
