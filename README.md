# json3
A tiny, fast, standard conformant JSON parser.

This was written for my own personal use, but you can use it if you want I guess.

It's currently only capable of parsing JSON text from a string into an iterable tree structure, but someday it might be able to do more.

I tried very hard to make it as conformant as possible to the [RFC 8259 standard](https://tools.ietf.org/html/rfc8259).

Requires at least C99.

## How use?

Take a look for yourself:

```c
#include <string.h>
#include <stdio.h>

#include "json3.h"

int main() {
  char const *text = "{\"name\": \"Sock\", \"species\": \"cat\", \"age\": 100,\
\"likes to pee on carpet\": true, \"friends\": [\"the sun\", \"the moon\", \"the stars\"]}";
  
  Json *json = json_parse(text, strlen(text));
  if (json_is_error(json)) {
    fprintf(stderr, "JSON error on line %i: %s\n", (int)json->error->line, json->error->message);
    return 1;
  }
  
  char const *name;
  char const *species;
  double age;
  bool likes_to_pee_on_carpet; // <stdbool.h> is included by "json3.h" if you're not using c++
  char const *friends[3];
  
  for (
    JsonObjectEntry *object_entry = json->object->first;
    object_entry != NULL;
    object_entry = object_entry->next
  ) {
    char const *key = object_entry->key;
    if (strcmp(key, "name") == 0)
      name = object_entry->value->string->text;
    else if (strcmp(key, "species") == 0)
      species = object_entry->value->string->text;
    else if (strcmp(key, "age") == 0)
      age = object_entry->value->number;
    else if (strcmp(key, "likes to pee on carpet") == 0)
      likes_to_pee_on_carpet = object_entry->value->boolean;
    else if (strcmp(key, "friends") == 0) {
      int friend_index = 0;
      for (
        JsonArrayEntry *array_entry = object_entry->value->array->first;
        array_entry != NULL;
        array_entry = array_entry->next
      ) {
        friends[friend_index++] = array_entry->value->string->text;
      }
    }
  }
  
  printf("%s is a %s who is age %g and %s to pee on the carpet.\n", name, species, age, likes_to_pee_on_carpet ? "likes" : "does not like");
  printf("%s is friends with %s, %s, and %s.\n", name, friends[0], friends[1], friends[2]);
  
  free(json);
}
```
