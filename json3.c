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

#include "json3.h"

#include <stdio.h>
#include <string.h>

#define DEPTH_LIMIT 1000

typedef struct _ParseState {
	char *it;
	char const *end;
	
	char *buffer_begin;
	char *buffer_it;
	
	size_t line;
	
	int depth;
} _ParseState;

static bool _create_buffer(_ParseState *state) {
	int values_estm = 1;
	int arrays_estm = 0;
	int objects_estm = 0;
	int object_entries_estm = 0;
	int strings_estm = 0;
	int characters_estm = 0;
	
	bool in_string = false;
	
	for (char const *it = state->it; it < state->end; it++) {
		if (!in_string) {
			switch (*it) {
			case ',': values_estm++; break;
			case ':': object_entries_estm++; break;
			case '[': {
				arrays_estm++;
				values_estm++;
				break;
			}
			case '{': {
				objects_estm++;
				values_estm++;
				break;
			}
			case '"': {
				strings_estm++;
				characters_estm++;
				in_string = true;
				break;
			}
			}
		}
		else {
			if (*it == '"') {
				in_string = false;
			}
			else {
				characters_estm++;
			}
		}
	}
	
	size_t buffer_size = (
		(values_estm * (sizeof(JsonArrayEntry) + sizeof(Json))) +
		(arrays_estm * sizeof(JsonArray)) +
		(objects_estm * sizeof(JsonObject)) +
		(object_entries_estm * ((sizeof(JsonObjectEntry) - sizeof(JsonArrayEntry)) + sizeof(Json))) +
		(strings_estm * sizeof(JsonString)) +
		characters_estm +
		sizeof(JsonError) +
		32
	);
	state->buffer_begin = (char*)malloc(buffer_size);
	if (state->buffer_begin == NULL) {
		return false;
	}
	state->buffer_it = state->buffer_begin;
	
	return true;
}

static void *_alloc(_ParseState *state, size_t amount) {
	void *result = state->buffer_it;
	state->buffer_it += amount;
	return result;
}
#define _alloc_for(state, T) ((T*)_alloc(state, sizeof(T)))

#define _is_whitespace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')
#define _is_lowercase(c) ((c) >= 'a' && (c) <= 'z')
#define _is_uppercase(c) ((c) >= 'A' && (c) <= 'z')
#define _to_lowercase(c) ((c) + ('a' - 'A') * _is_uppercase(c))
#define _to_uppercase(c) ((c) - ('a' - 'A') * _is_lowercase(c))
#define _is_letter(c) (_is_lowercase(c) || _is_uppercase(c))
#define _is_digit(c) ((c) >= '0' && c <= '9')

#define _peek(state) (*(state)->it)
#define _peek_ahead(state, ahead) (*((state)->it + (ahead)))
#define _get(state) (*(state)->it++)
#define _get_ahead(state, ahead) ((state->it) += ahead)
#define _get_if_eq(state, c) ((_peek(state) == (c)) ? _get(state) : '\0')

static Json _parse_value(_ParseState *state);

static Json _error(_ParseState *state, char const *message) {
	JsonError *error = _alloc_for(state, JsonError);
	error->message = message;
	error->line = state->line;
	return (Json){
		.kind = JSON_ERROR,
		.error = error
	};
}

static void _skip_whitespace(_ParseState *state) {
	while (_is_whitespace(_peek(state))) {
		if (_get(state) == '\n') {
			state->line++;
		}
	}
}

static Json _parse_number(_ParseState *state) {
	char *initial_it = state->it;
	
	if (_get_if_eq(state, '-')) {
		if (!_is_digit(_peek(state))) {
			return _error(state, "Expected digit after '-'");
		}
	}
	
	bool small_integer = true;
	
	if (_get_if_eq(state, '0')) {
		char c = _peek(state);
		if (_is_digit(c)) {
			return _error(state, "Integer part of number follows leading zero");
		}
		else if (c != '.' && c != 'e' && c != 'E') {
			return (Json){JSON_NUMBER, .number = 0.0};
		}
	}
	else {
		while (_is_digit(_peek(state))) {
			_get(state);
		}
		if (state->it - initial_it > 15) {
			small_integer = false;
		}
	}
	
	if (_get_if_eq(state, '.')) {
		small_integer = false;
		if (!_is_digit(_peek(state))) {
			return _error(state, "Expected digit after '.'");
		}
		
		while (_is_digit(_peek(state))) {
			_get(state);
		}
	}
	
	if (_to_uppercase(_peek(state)) == 'E') {
		small_integer = false;
		_get(state);
		if (!_get_if_eq(state, '-')) {
			_get_if_eq(state, '+');
		}
		
		if (!_is_digit(_peek(state))) {
			return _error(state, "Expected numeric exponent");
		}
		
		while (_is_digit(_peek(state))) {
			_get(state);
		}
	}
	
	state->it = initial_it;
	
	double value;
	
	if (small_integer) {
		long long sign = _get_if_eq(state, '-') ? -1 : 1;
		long long integer_value = 0;
		while (_is_digit(_peek(state))) {
			integer_value *= 10;
			integer_value += (_get(state) - '0');
		}
		value = (double)(integer_value * sign);
	}
	else {
		value = strtod(state->it, &state->it);
	}
	
	return (Json){JSON_NUMBER, .number = value};
}

static Json _parse_string(_ParseState *state) {
	_get(state);
	
	size_t max_length = 0;
	
	char *initial_it = state->it;
	
	bool simple_string = true;
	
	while (_peek(state) != '"') {
		char c = _get(state);
		if (c == '\\') {
			simple_string = false;
			_get_if_eq(state, '"');
		}
		if (c >= '\0' && c < ' ') {
			return _error(state,
				c == '\n' ? "Unescaped line feed character in string" :
				c == '\r' ? "Unescaped carriage return character in string" :
				c == '\t' ? "Unescaped tab character in string" :
				"Unescaped control character in string"
			);
		}
		max_length++;
	}
	
	char *begin = (char*)_alloc(state, max_length + 1);
	size_t length = 0;
	
	state->it = initial_it;
	
	if (simple_string) {
		memcpy(begin, state->it, max_length);
		length = max_length;
		_get_ahead(state, max_length + 1);
	}
	else {
		unsigned int high_surrogate = 0;
		
		while (_peek(state) != '"') {
			if (_get_if_eq(state, '\\')) {
				switch (_get(state)) {
				case '"': begin[length++] = '"'; break;
				case '\\': begin[length++] = '\\'; break;
				case '/': begin[length++] = '/'; break;
				case 'b': begin[length++] = '\b'; break;
				case 'f': begin[length++] = '\f'; break;
				case 'n': begin[length++] = '\n'; break;
				case 'r': begin[length++] = '\r'; break;
				case 't': begin[length++] = '\t'; break;
				case 'u': {
					unsigned int code_point = 0;
					for (int i = 0; i < 4; i++) {
						char c = _get(state);
						code_point *= 16;
						if (_is_digit(c)) code_point += (c - '0');
						else if (c >= 'A' && c <= 'F') code_point += 10 + (c - 'A');
						else if (c >= 'a' && c <= 'f') code_point += 10 + (c - 'a');
						else {
							return _error(state, "Invalid \\u escape sequence");
						}
					}
					if (high_surrogate == 0) {
						if (code_point >= 0xD800 && code_point <= 0xDBFF) {
							high_surrogate = code_point;
							continue;
						}
						else {
							if (code_point >= 0xDC00 && code_point <= 0xDFFF) {
								return _error(state, "Unpaired low surrogate");
							}
							
							if (code_point <= 0x007F) {
								begin[length++] = (char)code_point;
							}
							else if (code_point <= 0x07FF) {
								begin[length++] = 0b11000000 | (code_point >> 6);
								begin[length++] = 0b10000000 | (code_point & 0b111111);
							}
							else {
								begin[length++] = 0b11100000 | (code_point >> 12);
								begin[length++] = 0b10000000 | ((code_point & 0b111111000000) >> 6);
								begin[length++] = 0b10000000 | (code_point & 0b111111);
							}
						}
					}
					else {
						unsigned int low_surrogate = code_point;
						if (low_surrogate < 0xDC00 || low_surrogate > 0xDFFF) {
							return _error(state, "Invalid surrogate pair");
						}
						code_point = 0x10000 + ((high_surrogate & 0b1111111111) << 10) | (low_surrogate & 0b1111111111);
						
						begin[length++] = 0b11110000 | (code_point >> 18);
						begin[length++] = 0b10000000 | ((code_point & 0b111111000000000000) >> 12);
						begin[length++] = 0b10000000 | ((code_point & 0b111111000000) >> 6);
						begin[length++] = 0b10000000 | (code_point & 0b111111);
						
						high_surrogate = 0;
					}
					break;
				}
				default: {
					return _error(state, "Invalid escape sequence");
				}
				}
			}
			else {
				begin[length++] = _get(state);
			}
			if (high_surrogate != 0) {
				return _error(state, "Unpaired high surrogate");
			}
		}
		
		_get(state);
	}
	begin[length] = '\0';
	
	JsonString *value = _alloc_for(state, JsonString);
	value->text = begin;
	value->size = length;
	
	return (Json){JSON_STRING, .string = value};
}

static Json _parse_array(_ParseState *state) {
	_get(state);
	
	_skip_whitespace(state);
	
	JsonArray *array = _alloc_for(state, JsonArray);
	
	if (!_get_if_eq(state, ']')) {
		array->size = 0;
		
		JsonArrayEntry *entry = _alloc_for(state, JsonArrayEntry);
		array->first = entry;
		while (true) {
			array->size++;
			entry->value = _alloc_for(state, Json);
			*entry->value = _parse_value(state);
			if (entry->value->kind == JSON_ERROR) {
				return *entry->value;
			}
			
			_skip_whitespace(state);
			
			if (_get_if_eq(state, ',')) {
				entry->next = _alloc_for(state, JsonArrayEntry);
				entry = entry->next;
			}
			else if (_get_if_eq(state, ']')) {
				entry->next = NULL;
				break;
			}
			else {
				return _error(state, "Expected ',' or ']'");
			}
		}
	}
	else {
		array->size = 0;
		array->first = NULL;
	}
	
	return (Json){JSON_ARRAY, .array = array};
}

static Json _parse_object(_ParseState *state) {
	_get(state);
	
	_skip_whitespace(state);
	
	JsonObject *object = _alloc_for(state, JsonObject);
	
	if (!_get_if_eq(state, '}')) {
		object->size = 0;
		
		JsonObjectEntry *entry = _alloc_for(state, JsonObjectEntry);
		object->first = entry;
		while (true) {
			object->size++;
			
			_skip_whitespace(state);
			
			if (_peek(state) != '"') {
				return _error(state, "Expected a string");
			}
			
			Json key = _parse_string(state);
			if (key.kind == JSON_ERROR) {
				return key;
			}
			
			_skip_whitespace(state);
			
			if (!_get_if_eq(state, ':')) {
				return _error(state, "Expected ':'");
			}
			
			Json value = _parse_value(state);
			if (value.kind == JSON_ERROR) {
				return value;
			}
			
			entry->key = key.string->text;
			entry->key_size = key.string->size;
			entry->value = _alloc_for(state, Json);
			*entry->value = value;
			
			_skip_whitespace(state);
			
			if (_get_if_eq(state, ',')) {
				entry->next = _alloc_for(state, JsonObjectEntry);
				entry = entry->next;
			}
			else if (_get_if_eq(state, '}')) {
				entry->next = NULL;
				break;
			}
			else {
				return _error(state, "Expected ',' or '}'");
			}
		}
	}
	else {
		object->size = 0;
		object->first = NULL;
	}
	
	return (Json){JSON_OBJECT, .object = object};
}

static Json _parse_value(_ParseState *state) {
	_skip_whitespace(state);
	
	Json result;
	
	if (state->depth++ > DEPTH_LIMIT) {
		result = _error(state, "Depth limit reached");
	}
	else {
		char c = _peek(state);
		if (c == '"') {
			result = _parse_string(state);
		}
		else if (_is_digit(c) || c == '-') {
			result = _parse_number(state);
		}
		else if (c == '{') {
			result = _parse_object(state);
		}
		else if (c == '[') {
			result = _parse_array(state);
		}
		else if (memcmp(state->it, "true", 4) == 0) {
			_get_ahead(state, 4);
			result = (Json){JSON_BOOLEAN, .boolean = true};
		}
		else if (memcmp(state->it, "false", 5) == 0) {
			_get_ahead(state, 5);
			result = (Json){JSON_BOOLEAN, .boolean = false};
		}
		else if (memcmp(state->it, "null", 4) == 0) {
			_get_ahead(state, 4);
			result = (Json){JSON_NULL};
		}
		else {
			result = _error(state, "Unexpected character");
		}
	}
	state->depth--;
	
	return result;
}

static Json *_parse(_ParseState *state) {
	state->line = 1;
	if (!_create_buffer(state)) {
		return NULL;
	}
	Json *json = _alloc_for(state, Json);
	*json = _parse_value(state);
	if (json->kind != JSON_ERROR) {
		_skip_whitespace(state);
		if (state->it < state->end) {
			*json = _error(state, "Extra data in input");
		}
	}
	return json;
}

Json *json_parse(char const *source_begin, size_t source_length) {
	_ParseState state;
	char *begin = (char*)malloc(source_length + 16);
	if (begin == NULL) {
		return NULL;
	}
	state.it = begin;
	state.end = begin + source_length;
	state.depth = 0;
	memcpy(state.it, source_begin, source_length);
	memset(state.it + source_length, 0, 16);
	Json *result = _parse(&state);
	free(begin);
	return result;
}

Json *json_parse_file(char const *path) {
	FILE *stream = fopen(path, "rb");
	if (stream == NULL) {
		return NULL;
	}
	
	fseek(stream, 0, SEEK_END);
	long source_length = ftell(stream);
	fseek(stream, 0, SEEK_SET);
	
	char *source_begin = (char*)malloc(source_length + 16);
	if (source_begin == NULL) {
		fclose(stream);
		return NULL;
	}
	if (fread(source_begin, 1, source_length, stream) != source_length) {
		fclose(stream);
		return NULL;
	}
	memset(source_begin + source_length, 0, 16);
	
	fclose(stream);
	
	_ParseState state = {
		.it = source_begin,
		.end = source_begin + source_length,
		.depth = 0,
	};
	Json *result = _parse(&state);
	
	free(source_begin);
	
	return result;
}
