// PHASE 4.1: JSON PARSER SIMPLE - Parser JSON minimalista
// Para parsear manifest.json estandarizado

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

#define JSON_COLLECTION_INITIAL_CAPACITY 8

typedef struct JsonValue {
    JsonType type;
    union {
        int boolean;
        double number;
        char *string;
        struct {
            struct JsonValue *values;
            int count;
            int capacity;
        } array;
        struct {
            char **keys;
            struct JsonValue *values;
            int count;
            int capacity;
        } object;
    } value;
} JsonValue;

// Funciones públicas
JsonValue* json_parse(const char *json_str);
JsonValue* json_get_object(JsonValue *obj, const char *key);
const char* json_get_string(JsonValue *obj, const char *key);
double json_get_number(JsonValue *obj, const char *key);
int json_get_bool(JsonValue *obj, const char *key);
JsonValue* json_get_array(JsonValue *obj, const char *key);
int json_array_length(JsonValue *arr);
JsonValue* json_array_get(JsonValue *arr, int index);
void json_free(JsonValue *val);

#endif
