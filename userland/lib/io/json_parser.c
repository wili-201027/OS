// PHASE 4.1: JSON PARSER - Implementación minimalista
// Parser para manifest.json (limitado pero funcional)

#include "json_parser.h"
#include "../libc/stdlib.h"
#include "../libc/string.h"
#include "../libc/ctype.h"

typedef struct {
    const char *str;
    int pos;
    int len;
} JsonParser;

static void skip_whitespace(JsonParser *p) {
    while(p->pos < p->len && isspace(p->str[p->pos])) {
        p->pos++;
    }
}

static char peek(JsonParser *p) {
    skip_whitespace(p);
    if(p->pos >= p->len) return '\0';
    return p->str[p->pos];
}

static char next(JsonParser *p) {
    char c = peek(p);
    p->pos++;
    return c;
}

static JsonValue* parse_value(JsonParser *p);

static JsonValue* parse_string(JsonParser *p) {
    JsonValue *val = malloc(sizeof(JsonValue));
    if(!val) return NULL;
    
    val->type = JSON_STRING;
    
    next(p);  // skip opening "
    
    int start = p->pos;
    while(p->pos < p->len && p->str[p->pos] != '"') {
        if(p->str[p->pos] == '\\') p->pos++;  // escape
        p->pos++;
    }
    
    int len = p->pos - start;
    val->value.string = malloc(len + 1);
    strncpy(val->value.string, &p->str[start], len);
    val->value.string[len] = '\0';
    
    if(peek(p) == '"') next(p);  // skip closing "
    
    return val;
}

static JsonValue* parse_number(JsonParser *p) {
    JsonValue *val = malloc(sizeof(JsonValue));
    if(!val) return NULL;
    
    val->type = JSON_NUMBER;
    
    int start = p->pos;
    if(p->str[p->pos] == '-') p->pos++;
    
    while(p->pos < p->len && isdigit(p->str[p->pos])) {
        p->pos++;
    }
    
    if(p->str[p->pos] == '.') {
        p->pos++;
        while(p->pos < p->len && isdigit(p->str[p->pos])) {
            p->pos++;
        }
    }
    
    char num_str[64];
    int len = p->pos - start;
    if(len >= 63) len = 63;
    strncpy(num_str, &p->str[start], len);
    num_str[len] = '\0';
    
    val->value.number = atof(num_str);
    return val;
}

static JsonValue* parse_array(JsonParser *p) {
    JsonValue *val = malloc(sizeof(JsonValue));
    if(!val) return NULL;
    
    val->type = JSON_ARRAY;
    val->value.array.count = 0;
    val->value.array.capacity = JSON_COLLECTION_INITIAL_CAPACITY;
    val->value.array.values = malloc(sizeof(JsonValue) * val->value.array.capacity);
    if (!val->value.array.values) {
        free(val);
        return NULL;
    }
    
    next(p);  // skip [
    
    while(peek(p) != ']' && peek(p) != '\0') {
        JsonValue *elem = parse_value(p);
        if(!elem) {
            break;
        }

        if (val->value.array.count >= val->value.array.capacity) {
            int new_capacity = val->value.array.capacity * 2;
            JsonValue *new_values = realloc(val->value.array.values, sizeof(JsonValue) * new_capacity);
            if (!new_values) {
                json_free(elem);
                json_free(val);
                return NULL;
            }
            val->value.array.values = new_values;
            val->value.array.capacity = new_capacity;
        }

        val->value.array.values[val->value.array.count++] = *elem;
        free(elem);
        
        if(peek(p) == ',') next(p);
    }
    
    if(peek(p) == ']') next(p);
    
    return val;
}

static JsonValue* parse_object(JsonParser *p) {
    JsonValue *val = malloc(sizeof(JsonValue));
    if(!val) return NULL;
    
    val->type = JSON_OBJECT;
    val->value.object.count = 0;
    val->value.object.capacity = JSON_COLLECTION_INITIAL_CAPACITY;
    val->value.object.keys = malloc(sizeof(char*) * val->value.object.capacity);
    val->value.object.values = malloc(sizeof(JsonValue) * val->value.object.capacity);
    if (!val->value.object.keys || !val->value.object.values) {
        free(val->value.object.keys);
        free(val->value.object.values);
        free(val);
        return NULL;
    }
    
    next(p);  // skip {
    
    while(peek(p) != '}' && peek(p) != '\0') {
        if(peek(p) != '"') {
            break;
        }

        JsonValue *key_val = parse_string(p);
        if(!key_val) {
            break;
        }

        if (val->value.object.count >= val->value.object.capacity) {
            int new_capacity = val->value.object.capacity * 2;
            char **new_keys = realloc(val->value.object.keys, sizeof(char*) * new_capacity);
            JsonValue *new_values = realloc(val->value.object.values, sizeof(JsonValue) * new_capacity);
            if (!new_keys || !new_values) {
                free(key_val->value.string);
                free(key_val);
                if (new_keys) val->value.object.keys = new_keys;
                if (new_values) val->value.object.values = new_values;
                json_free(val);
                return NULL;
            }
            val->value.object.keys = new_keys;
            val->value.object.values = new_values;
            val->value.object.capacity = new_capacity;
        }

        val->value.object.keys[val->value.object.count] = key_val->value.string;
        free(key_val);

        if(peek(p) == ':') next(p);

        JsonValue *value = parse_value(p);
        if(!value) {
            free(val->value.object.keys[val->value.object.count]);
            break;
        }

        val->value.object.values[val->value.object.count] = *value;
        free(value);
        val->value.object.count++;

        if(peek(p) == ',') next(p);
    }
    
    if(peek(p) == '}') next(p);
    
    return val;
}

static JsonValue* parse_value(JsonParser *p) {
    char c = peek(p);
    
    if(c == '"') return parse_string(p);
    if(c == '{') return parse_object(p);
    if(c == '[') return parse_array(p);
    if(c == '-' || isdigit(c)) return parse_number(p);
    if(strncmp(&p->str[p->pos], "true", 4) == 0) {
        p->pos += 4;
        JsonValue *val = malloc(sizeof(JsonValue));
        val->type = JSON_BOOL;
        val->value.boolean = 1;
        return val;
    }
    if(strncmp(&p->str[p->pos], "false", 5) == 0) {
        p->pos += 5;
        JsonValue *val = malloc(sizeof(JsonValue));
        val->type = JSON_BOOL;
        val->value.boolean = 0;
        return val;
    }
    if(strncmp(&p->str[p->pos], "null", 4) == 0) {
        p->pos += 4;
        JsonValue *val = malloc(sizeof(JsonValue));
        val->type = JSON_NULL;
        return val;
    }
    
    return NULL;
}

JsonValue* json_parse(const char *json_str) {
    if(!json_str) return NULL;
    
    JsonParser p;
    p.str = json_str;
    p.pos = 0;
    p.len = strlen(json_str);
    
    return parse_value(&p);
}

JsonValue* json_get_object(JsonValue *obj, const char *key) {
    if(!obj || obj->type != JSON_OBJECT || !key) return NULL;
    
    for(int i = 0; i < obj->value.object.count; i++) {
        if(strcmp(obj->value.object.keys[i], key) == 0) {
            return &obj->value.object.values[i];
        }
    }
    
    return NULL;
}

const char* json_get_string(JsonValue *obj, const char *key) {
    JsonValue *val = json_get_object(obj, key);
    if(!val || val->type != JSON_STRING) return NULL;
    return val->value.string;
}

double json_get_number(JsonValue *obj, const char *key) {
    JsonValue *val = json_get_object(obj, key);
    if(!val || val->type != JSON_NUMBER) return 0.0;
    return val->value.number;
}

int json_get_bool(JsonValue *obj, const char *key) {
    JsonValue *val = json_get_object(obj, key);
    if(!val || val->type != JSON_BOOL) return 0;
    return val->value.boolean;
}

JsonValue* json_get_array(JsonValue *obj, const char *key) {
    JsonValue *val = json_get_object(obj, key);
    if(!val || val->type != JSON_ARRAY) return NULL;
    return val;
}

int json_array_length(JsonValue *arr) {
    if(!arr || arr->type != JSON_ARRAY) return 0;
    return arr->value.array.count;
}

JsonValue* json_array_get(JsonValue *arr, int index) {
    if(!arr || arr->type != JSON_ARRAY) return NULL;
    if(index < 0 || index >= arr->value.array.count) return NULL;
    return &arr->value.array.values[index];
}

void json_free(JsonValue *val) {
    if(!val) return;
    
    switch(val->type) {
        case JSON_STRING:
            free(val->value.string);
            break;
        case JSON_ARRAY:
            for(int i = 0; i < val->value.array.count; i++) {
                    json_free(&val->value.array.values[i]);
            }
            free(val->value.array.values);
            break;
        case JSON_OBJECT:
            for(int i = 0; i < val->value.object.count; i++) {
                free(val->value.object.keys[i]);
                json_free(&val->value.object.values[i]);
            }
            free(val->value.object.keys);
            free(val->value.object.values);
            break;
        default:
            break;
    }
    
    free(val);
}
