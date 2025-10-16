#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

// https://www.json.org/json-de.html

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long int u64;
typedef signed int s32;
typedef unsigned char b8;
typedef double f64;

#define __TEXT__(...) #__VA_ARGS__

#define todo() do{ fprintf(stderr, "%s:%d:TODO\n", __FILE__, __LINE__); exit(1); }while(0)

#define da_reserve(xs, len) do {\
    u64 __FILE__##__LINE__##cap = (xs)->cap;\
    if(__FILE__##__LINE__##cap == 0) __FILE__##__LINE__##cap = 256;\
    while(__FILE__##__LINE__##cap < (len)) __FILE__##__LINE__##cap <<= 1;\
    if((xs)->cap != __FILE__##__LINE__##cap) {\
        (xs)->cap = __FILE__##__LINE__##cap;\
        (xs)->data = realloc((xs)->data, (xs)->cap * sizeof(*(xs)->data));\
        if(!(xs)->data) todo();\
    }\
}while(0)

#define da_append(xs, x) do{\
    da_reserve((xs), (xs)->len + 1);\
    (xs)->data[(xs)->len++] = (x);\
}while(0)

typedef struct {
    u8 *data;
    u64 len;
} str;

#define str_fmt "%.*s"
#define str_arg(s) (s32) (s).len, (s).data
#define str_from(d, l) (str) { (d), (l) }
#define str_fromd(cstr) (str) { (cstr), sizeof(cstr) - 1 }

b8 str_eq(str a, str b) {
    return a.len == b.len && memcmp(a.data, b.data, a.len) == 0;
}

typedef struct {
    u8 *data;
    u64 len;
    u64 cap;
} u8s;

typedef struct {
    u64 *data;
    u64 cap;
    u64 len;
} u64s;

typedef enum {
    KIND_NULL,
    KIND_BOOL,
    KIND_NUMBER,
    KIND_STR,
    KIND_ARRAY,
    KIND_OBJECT
} Kind;

typedef struct Value Value;

typedef struct {
    Value *data;
    u64 cap;
    u64 len;
} Values;

typedef struct {
    Value *data;
    u64 len;
    u64 cap;
} Array;

typedef struct Entry Entry; 

typedef struct {
    Entry *data;
    u64 len;
    u64 cap;
} Object; 

struct Value {
    Kind kind;
    union {
        b8 boolean;
        f64 number;
        str string;
        Array array;
        Object object;
    } as;
};

#define U64_MAX ((u64) 0xffffffffffffffff)
#define OBJECT_KEY_LEN_INVALID U64_MAX

struct Entry {
    str key;
    Value value;
};

u64 array_len(Value *array) {
    if(array->kind != KIND_ARRAY) todo();
    return array->as.array.len;
}

void array_append(Value *array, Value value) {
    if(array->kind != KIND_ARRAY) todo();
    da_append(&array->as.array, value);
}

Value array_get(Value *array, u64 index) {
    if(array->kind != KIND_ARRAY) todo();
    if(array->as.array.len <= index) todo();
    return array->as.array.data[index];
}


Value value_array(void) {
    return (Value) {
        .kind = KIND_ARRAY,
            .as.array = (Array) {0},
    };
}

Value value_object(void) {
    return (Value) {
        .kind = KIND_OBJECT,
        .as.object = (Object) {0},
    };
}

#define DJB2_START 5381

u64 djb2(u64 hash, u8 *data, u64 len) {

    for(u64 i=0;i<len;i++) {
        hash = ((hash << 5) + hash) + data[i];
    }

    return hash;
}

b8 __object_insert(Object *o, str key, Value v) {

    u64 index = djb2(DJB2_START, key.data, key.len) % o->cap;
    while(1) {
        if(o->data[index].key.len != OBJECT_KEY_LEN_INVALID) {

            if(str_eq(o->data[index].key, key)) {
                o->data[index].value = v;
                return 1;
            } else {
                index = (index + 1) % o->cap;
            }

        } else {

            o->data[index] = (Entry) {
                .key = key,
                    .value = v,
            };
            o->len++;

            return 0;
        }
    }

}

void __object_init(Object *o, u64 cap) {
    o->cap = cap;
    o->data = malloc(o->cap * sizeof(*o->data));
    if(!o->data) todo();

    for(u64 i=0;i<o->cap;i++) {
        o->data[i].key.len = OBJECT_KEY_LEN_INVALID;
    }
}

b8 object_set(Value *object, str key, Value v) {
    if(object->kind != KIND_OBJECT) todo();

    Object *o = &object->as.object;
    if(o->cap <= o->len) {

        if(o->cap == 0) {
            __object_init(o, 16);
        } else {
            Object new_o;
            __object_init(&new_o, o->cap * 2);

            for(u64 i=0;i<o->cap;i++) {
                Entry *e = &o->data[i];
                if(e->key.len == OBJECT_KEY_LEN_INVALID) {
                    continue;
                }
                __object_insert(&new_o, e->key, e->value); 
            }

            *o = new_o;
        }

    }

    return __object_insert(o, key, v);

}


Value object_get(Value *object, str key) {
    if(object->kind != KIND_OBJECT) todo();

    Object *o = &object->as.object;
    if(o->len == 0) todo();

    u64 index = djb2(DJB2_START, key.data, key.len) % o->cap;

    while(1) {
        if(o->data[index].key.len == OBJECT_KEY_LEN_INVALID) {
            todo();
        } else {
            if(str_eq(key, o->data[index].key)) {
                return o->data[index].value;
            } else {
                index = (index + 1) % o->cap;
            }
        }
    }
}

b8 object_next(Value *object, u64 *index, Entry **entry) {
    if(object->kind != KIND_OBJECT) todo();

    Object *o = &object->as.object;
    if(o->len == 0) return 0;

    *index = *index + 1;
    while(o->data[*index].key.len == OBJECT_KEY_LEN_INVALID) {
        *index = *index + 1;
    }

    if(*index < o->cap) {
        *entry = &o->data[*index];
        return 1;
    } else {
        return 0;
    }

}

Value value_number(f64 n) {
    return (Value) {
        .kind = KIND_NUMBER,
            .as.number = n
    };
}

Value value_str(str string) {
    return (Value) {
        .kind = KIND_STR,
            .as.string = string
    };
}
#define value_strd(cstr) value_str(str_fromd(cstr))

Value value_null(void) {
    return (Value) {
        .kind = KIND_NULL,
    };
}

Value value_boolean(b8 b) {
    return (Value) {
        .kind = KIND_BOOL,
        .as.boolean = b,
    };
}

void value_print(Value v) {
    switch(v.kind) {
        case KIND_NULL: printf("null"); break;
        case KIND_BOOL: printf("%s", v.as.boolean ? "true" : "false"); break;
        case KIND_NUMBER: printf("%.02f", v.as.number); break;
        case KIND_STR: printf("\""str_fmt"\"", str_arg(v.as.string)); break;
        case KIND_ARRAY: {
            printf("[");
            for(u64 i=0;i<array_len(&v);i++) {
                if(i != 0) printf(","); 
                value_print(array_get(&v, i));
            }
            printf("]");
        } break;
        case KIND_OBJECT: {
            printf("{");
            Entry *e;
            u64 index = U64_MAX;
            u64 count = 0;
            while(object_next(&v, &index, &e)) {
                if(count++ != 0) printf(","); 
                printf("\""str_fmt"\":", str_arg(e->key));
                value_print(e->value);
            }
            printf("}");
        }
    }
}

typedef struct {
    u8 *data;
    u64 len;
} Memory;

typedef struct {

    u64 len;
    u64 pos;

    u8 buf[1024];

    u16 buf_pos;
    u16 buf_len;
    HANDLE fd;
} File;

typedef enum {
    READER_KIND_MEMORY,
    READER_KIND_FILE,
} Reader_Kind;

typedef struct {
    Reader_Kind kind;
    union {
        Memory memory;
        File file;
    } as;
} Reader;

#define reader_from_memoryd(cstr) (Reader) { .kind = READER_KIND_MEMORY, .as.memory = (Memory) { .data = (cstr), .len = sizeof(cstr) - 1 } }

Reader reader_from_file(u8 *name, u64 name_len) {

    u16 path[MAX_PATH];
    s32 n = MultiByteToWideChar(
            CP_UTF8,
            0,
            (char *) name,
            (s32) name_len,
            path,
            MAX_PATH);
    if(n == 0) {
        todo();
    }
    path[n] = 0;

    HANDLE fd = CreateFileW(path,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    if(fd == INVALID_HANDLE_VALUE) {
        todo();
    }

    u32 hi;
    u32 lo = GetFileSize(fd, (DWORD *) &hi);
    if(lo == INVALID_FILE_SIZE) {
        CloseHandle(fd);
        todo();
    }
    u64 len = (u64) lo | ((u64) hi << 32);
    u64 pos = 0;

    return (Reader) {
        .kind = READER_KIND_FILE,
        .as.file = (File) {
            .fd  = fd,
            .len = len,
            .pos = pos,

            .buf_pos = 0,
            .buf_len = 0,
        },
    };
}
#define reader_from_filec(cstr) reader_from_file((cstr), strlen(cstr))
#define reader_from_filed(cstr) reader_from_file((cstr), sizeof(cstr) - 1)
#define reader_from_files(s) reader_from_file((s).data, (s).len)

b8 reader_peek_u8(Reader *r, u8 *b) {
    switch(r->kind) {
        case READER_KIND_MEMORY: {
            Memory *m = &r->as.memory;

            if(0 < m->len) {
                *b = m->data[0];
                return 1;
            } else {
                return 0;
            }
        } break;
        case READER_KIND_FILE: {
            File *f = &r->as.file;
            if(f->buf_len <= f->buf_pos) {

                if(f->len <= f->pos) {
                    CloseHandle(f->fd);
                    return 0;
                }
                
                u32 m;
                if(!ReadFile(f->fd, f->buf, sizeof(f->buf), (DWORD *) &m, NULL)) {
                    todo();
                }
                f->pos += m;
                f->buf_pos = 0;
                f->buf_len = m;

            }
    
            *b = f->buf[f->buf_pos];
            return 1;

        } break;
        default: todo();
    }
}

b8 reader_read_u8(Reader *r, u8 *b) {
    switch(r->kind) {
        case READER_KIND_MEMORY: {
            Memory *m = &r->as.memory;

            if(0 < m->len) {
                *b = m->data[0];
                m->data += 1;
                m->len  -= 1;
                return 1;
            } else {
                return 0;
            }
        } break;
        case READER_KIND_FILE: {
            File *f = &r->as.file;
            if(f->buf_len <= f->buf_pos) {

                if(f->len <= f->pos) {
                    CloseHandle(f->fd);
                    return 0;
                }
                
                u32 m;
                if(!ReadFile(f->fd, f->buf, sizeof(f->buf), (DWORD *) &m, NULL)) {
                    todo();
                }
                f->pos += m;
                f->buf_pos = 0;
                f->buf_len = m;

            }
    
            *b = f->buf[f->buf_pos++];
            return 1;

        } break;
        default: todo();
    }
}


void reader_skip_any(Reader *r, u8 *bs, u64 len) {
    u8 b;
    while(reader_peek_u8(r, &b)) {

        b8 keep = 1;
        for(u64 i=0;keep && i<len;i++) {
            if(bs[i] == b) {
                keep = 0;
            }
        }

        if(!keep) {
            if(!reader_read_u8(r, &b)) todo();
        } else {
            break;
        }

    }
}

b8 reader_expect_str(Reader *r, str s) {
    u64 i=0;
    u8 b;
    while(i<s.len && reader_peek_u8(r, &b) && b == s.data[i]) {
        if(!reader_read_u8(r, &b)) todo();
        i += 1;
    }

    return i == s.len;
}
#define reader_expect_strd(r, cstr) reader_expect_str((r), str_fromd(cstr))

u8 whitespace[] = { ' ', '\n', '\r', '\t' };
u64 whitespace_len = sizeof(whitespace) / sizeof(*whitespace);

void rune_encode(u32 rune, u8s *bs) {
    if(rune <= 0x7f) {
        da_append(bs, (u8) rune);
    } else if(0x80 <= rune && rune <= 0x7ff) {
        u8 fst = 0xc0 | ((rune & 0x700) >> 6) | ((rune & 0xc0) >> 6);
        da_append(bs, fst);
        u8 snd = 0x80 | (rune & 0x3f);
        da_append(bs, snd);
    } else if(0x800 <= rune && rune <= 0xffff) {
        u8 fst = 0xe0 | ((rune & 0xf000) >> 12);
        da_append(bs, fst);
        u8 snd = 0x80 | ((rune & 0x0f00) >> 6) | ((rune & 0xc0) >> 6);
        da_append(bs, snd);
        u8 trd = 0x80 | (rune & 0x3f);
        da_append(bs, trd);
    } else if(0x10000 <= rune && rune <= 0x10ffff) {
        u8 fst = 0xf0 | ((rune & 0x100000) >> 17) | ((rune & 0xc0000) >> 18);
        da_append(bs, fst);
        u8 snd = 0x80 | ((rune & 0x30000) >> 12) | ((rune & 0xf000) >> 12);
        da_append(bs, snd);
        u8 trd = 0x80 | ((rune & 0xf00) >> 6) | ((rune & 0xc0) >> 6);
        da_append(bs, trd);
        u8 fth = 0x80 | (rune & 0x3f);
        da_append(bs, fth);
    } else {
        todo();
    }
}

b8 reader_read_json_string(Reader *r, str *s) {

    u8 b;
    if(!reader_read_u8(r, &b)) {
        todo();
    }

    u8s bs = {0};
    while(1) {
        if(!reader_read_u8(r, &b)) {
            return 0;
        }
        if(b == '\"') {
            break;
        } else if(b == '\\') {
            if(!reader_read_u8(r, &b)) {
                return 0;
            }
            switch(b) {
                case '/': da_append(&bs, '/'); break;
                case '\\': da_append(&bs, '\\'); break;
                case 'b': da_append(&bs, '\b'); break;
                case 'f': da_append(&bs, '\f'); break;
                case 'n': da_append(&bs, '\n'); break;
                case 'r': da_append(&bs, '\r'); break;
                case 't': da_append(&bs, '\t'); break;
                case 'u': {
                    u32 n = 0;
                    for(u8 i=0;i<4;i++) {
                        if(!reader_read_u8(r, &b)) {
                            return 0;
                        }

                        n *= 16;
                        if('0' <= b && b <= '9') {
                            n += b - '0';
                        } else if('a' <= b && b <= 'f') {
                            n += b - 'W';
                        } else if('A' <= b && b <= 'F') {
                            n += b - '7';
                        } else {
                            return 0;
                        }
                    }

                    rune_encode(n, &bs);
                } break;
                default:
                          return 0;
            }
        } else {
            da_append(&bs, b);
        }
    }

    *s = str_from(bs.data, bs.len);
    return 1;
}

b8 reader_read_json_number(Reader *r, f64 *number) {

    u64 off = 0;

    typedef enum {
        IDLE,
        AFTER_MINUS,
        LEFT,
        ZERO,
        RIGHT,
        EXPONENT_IDLE,
        EXPONENT,
    } State;
    State state = IDLE;

    b8 negative = 0;
    b8 exponent_negative = 0;
    u64 left = 0;
    u64 right = 0;
    u64 right_count = 0;
    u64 exponent = 1;

    b8 done = 0;
    while(!done) {
        u8 b;
        if(!reader_peek_u8(r, &b)) {
            break;
        }

        switch(state) {
            case IDLE: {
                if(b == '-') {
                    negative = 1;
                    state = AFTER_MINUS;
                } else if(b == '0') {
                    state = ZERO;
                } else if('1' <= b && b <= '9') {
                    left = b - '0';
                    state = LEFT;
                } else {
                    done = 1;
                }
            } break;
            case ZERO: {
                if(b == '.') {
                    state = RIGHT;
                } else if(b == 'e' || b == 'E') {
                    state = EXPONENT_IDLE;
                } else {
                    done = 1;
                }
            } break;
            case AFTER_MINUS: {
                if(b == '0') {
                    state = ZERO;
                } else if('1' <= b && b <= '9') {
                    left = b - '0';
                    state = LEFT;
                } else {
                    done = 1;
                }
            } break;
            case LEFT: {
                if(b == 'e' || b == 'E') {
                    todo();
                } else if('0' <= b && b <= '9') {
                    left *= 10;
                    left += b - '0';
                    // state = LEFT;
                } else if(b == '.') {
                    state = RIGHT;
                } else {
                    done = 1;
                }
            } break;
            case RIGHT: {
                if(b == 'e' || b == 'E') {
                    state = EXPONENT_IDLE;
                } else if('0' <= b && b <= '9') {
                    right *= 10;
                    right += b - '0';
                    right_count += 1;
                    // state = LEFT;
                } else {
                    done = 1;
                }
            } break;
            case EXPONENT_IDLE: {
                if(b == '-') {
                    exponent = 0;
                    exponent_negative = 1;
                    state = EXPONENT;
                } else if(b == '+') {
                    exponent = 0;
                    state = EXPONENT;
                } else if('0' <= b && b <= '9') {
                    exponent = b - '0';
                    state = EXPONENT;
                } else {
                    done = 1;
                }
            } break;
            case EXPONENT: {
                if('0' <= b && b <= '9') {
                    exponent *= 10;
                    exponent += b - '0';
                    // state = EXPONENT;
                } else {
                    done = 1;
                }

            } break;
            default: todo();
        }

        if(!done) {
            if(!reader_read_u8(r, &b)) {
                todo();
            }
        }
    }

    f64 rgt = (f64) right;
    for(u64 i=0;i<right_count;i++) {
        rgt *= 0.1;
    }

    f64 n = (f64) left + rgt;

    if(exponent != 1) {
        for(u64 i=0;i<exponent;i++) {
            if(exponent_negative) {
                n *= 0.1;
            } else {
                n *= 10;
            }
        }
    }

    if(negative) {
        n *= -1;
    }

    *number = n;

    switch(state) {
        case IDLE: return 0;
        case AFTER_MINUS: return 0;
        case LEFT: return 1;
        case ZERO: return 1;
        case RIGHT: return 1;
        case EXPONENT_IDLE: return 0;
        case EXPONENT: return 1;
        default: todo();
    }
}

b8 reader_read_json_value(Reader *r, Value *v);

b8 reader_read_json_object(Reader *r, Value *object) {

    u8 b;
    if(!reader_read_u8(r, &b) || b != '{') {
        return 0;
    }

    while(1) {
        reader_skip_any(r, whitespace, whitespace_len);

        if(!reader_peek_u8(r, &b)) {
            todo();
            return 0;
        }

        if(b == '}') {
            if(!reader_read_u8(r, &b)) { 
                todo();
            }
            break;
        } else {
            str key;
            if(!reader_read_json_string(r, &key)) {
                todo();
                return 0;
            }

            reader_skip_any(r, whitespace, whitespace_len);

            if(!reader_read_u8(r, &b) || b != ':') {
                return 0;
            }

            reader_skip_any(r, whitespace, whitespace_len);

            Value v;
            if(!reader_read_json_value(r, &v)) {
                todo();
                return 0;
            }

            reader_skip_any(r, whitespace, whitespace_len);

            object_set(object, key, v);

            if(!reader_read_u8(r, &b)) {
                todo();
                return 0;
            }
            if(b == '}') {
                break;
            } else if(b == ',') {
                // continue
            } else {
                todo();
                return 0;
            }
        }
    }


    return 1;

}

b8 reader_read_json_array(Reader *r, Value *array) {

    u8 b;
    if(!reader_read_u8(r, &b) || b != '[') {
        return 0;
    }

    while(1) {
        reader_skip_any(r, whitespace, whitespace_len);

        if(!reader_peek_u8(r, &b)) {
            return 0;
        }

        if(b == ']') {
            if(!reader_read_u8(r, &b)) { 
                todo();
            }
            break;
        } else {
            Value v;
            if(!reader_read_json_value(r, &v)) {
                return 0;
            }

            array_append(array, v);
            reader_skip_any(r, whitespace, whitespace_len);

            if(!reader_read_u8(r, &b)) {
                return 0;
            }
            if(b == ']') {
                break;
            } else if(b == ',') {
                // continue
            } else {
                return 0;
            }
        }
    }


    return 1;

}

b8 reader_read_json_value(Reader *r, Value *v) {

    reader_skip_any(r, whitespace, whitespace_len);

    u8 b;
    if(!reader_peek_u8(r, &b)) {
        return 0;
    }
    if('0' <= b && b <= '9') {
        f64 n;
        if(!reader_read_json_number(r, &n)) {
            return 0;
        }
        *v = value_number(n);
    } else {
        switch(b) {
            case 'n': {
                if(!reader_expect_strd(r, "null")) {
                    return 0;
                }
                *v = value_null();
            } break;
            case 'f': {
                if(!reader_expect_strd(r, "false")) {
                    return 0;
                }
                *v = value_boolean(0);
            } break;
            case 't': {
                if(!reader_expect_strd(r, "true")) {
                    return 0;
                }
                *v = value_boolean(1);
            } break;
            case '\"': {
                str s;
                if(!reader_read_json_string(r, &s)) {
                    return 0;
                }
                *v = value_str(s);
            } break;
            case '-': {
                f64 n;
                if(!reader_read_json_number(r, &n)) {
                    return 0;
                }
                *v = value_number(n);
            } break;
            case '[': {
                *v = value_array();
                if(!reader_read_json_array(r, v)) {
                    return 0;
                }
            } break;
            case '{': {
                *v = value_object();
                if(!reader_read_json_object(r, v)) {
                    return 0;
                }

            } break;
            default: {
                printf("'%c' (0x%02x)\n", b, b);
                todo();
            }
        }
    }



    return 1;
}

b8 reader_read_toml_value(Reader *r, Value *v) {
    
    *v = value_object();

    

    return 1;

}

int main(void) {
    Value tests;
    Reader reader = reader_from_filed("config.json");
    if(!reader_read_json_value(&reader, &tests)) {
        todo();
    }
    if(tests.kind != KIND_ARRAY) todo();
    
    for(u64 i=0;i<array_len(&tests);i++) {
        Value test = array_get(&tests, i);
        if(test.kind != KIND_OBJECT) todo();

        Value toml = object_get(&test, str_fromd("toml"));
        Value json = object_get(&test, str_fromd("json"));
    
        if(toml.kind != KIND_STR) todo();

        reader = reader_from_files(toml.as.string);
        Value toml_value;
        if(!reader_read_toml_value(&reader, &toml_value)) {
            todo();
        }
    }

    return 0;
}
