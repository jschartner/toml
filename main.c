#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#  include <windows.h>
#elif __APPLE__ || linux
#  include <linux/limits.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  include <fcntl.h>
#else
#endif

// https://www.json.org/json-de.html
// https://toml.io/en/v1.0.0

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long int u64;
typedef signed int s32;
typedef unsigned char b8;
typedef double f64;

#define __TEXT__(...) #__VA_ARGS__

#define todo() do{ asm("int3"); fprintf(stderr, "%s:%d:TODO\n", __FILE__, __LINE__); exit(1); }while(0)

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
#define str_from(d, l) { (d), (l) }
#define str_fromd(cstr) { (cstr), sizeof(cstr) - 1 }

b8 str_eq(str a, str b) {
    return a.len == b.len && memcmp(a.data, b.data, a.len) == 0;
}

b8 str_index_of(str a, str b, u64 *index) {
    for(u64 i=0;i<a.len;i++) {
        str s = str_from(a.data + i, a.len - i);
        if(b.len < s.len) {
            s.len = b.len;
        }
        if(str_eq(s, b)) {
            i = *index;
            return 1;
        }
    }

    return 0;
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

Value *array_get(Value *array, u64 index) {
    if(array->kind != KIND_ARRAY) todo();
    if(array->as.array.len <= index) todo();
    return &array->as.array.data[index];
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


b8 object_get(Value *object, str key, Value **v) {
    if(object->kind != KIND_OBJECT) todo();

    Object *o = &object->as.object;
    if(o->len == 0) {
        return 0;
    }

    u64 index = djb2(DJB2_START, key.data, key.len) % o->cap;

    while(1) {
        if(o->data[index].key.len == OBJECT_KEY_LEN_INVALID) {
            return 0;
        } else {
            if(str_eq(key, o->data[index].key)) {
                *v = &o->data[index].value;
                return 1; 
            } else {
                index = (index + 1) % o->cap;
            }
        }
    }

    todo();
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

b8 value_eq(Value a, Value b) {
    if(a.kind != b.kind) {
        return 0;
    }
    Kind kind = a.kind;

    switch(kind) {
        case KIND_NULL: todo();
        case KIND_BOOL: {
            return a.as.boolean == b.as.boolean;
        } break;
        case KIND_NUMBER: {
            return a.as.number == b.as.number;
        }; 
        case KIND_STR: {
            return str_eq(a.as.string, b.as.string);
        } break;
        case KIND_ARRAY: todo();
        case KIND_OBJECT: {

            u64 index;
            Entry *entry;

            // a in b
            index = U64_MAX;
            while(object_next(&a, &index, &entry)) {
                Value *value;
                if(!object_get(&b, entry->key, &value)) return 0;
                if(!value_eq(entry->value, *value)) return 0;
            }

            // b in a
            index = U64_MAX;
            while(object_next(&b, &index, &entry)) {
                Value *value;
                if(!object_get(&a, entry->key, &value)) return 0;
                if(!value_eq(entry->value, *value)) return 0;
            }

            return 1;

        } break;
    }

    todo();
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
                value_print(*array_get(&v, i));
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
#ifdef _WIN32
    HANDLE fd;
#elif __APPLE__ || linux
    s32 fd;
#else
#endif
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
    u64 off;
} Reader;

#define reader_from_memoryd(cstr) (Reader) { .kind = READER_KIND_MEMORY, .as.memory = (Memory) { .data = (cstr), .len = sizeof(cstr) - 1, .pos = 0} }

Reader reader_from_file(u8 *name, u64 name_len) {

#ifdef _WIN32
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
        .off = 0,
    };
#elif __APPLE__ || linux
    u8 buf[PATH_MAX];
    memcpy(buf, name, name_len);
    buf[name_len] = 0;

    s32 fd = open((char *) buf, O_RDONLY);
    if(fd < 0) {
        todo();
    }

    struct stat stats;
    if(stat((char *) buf, &stats) < 0) {
        close(fd);
        todo();
    }

    u64 len = (u64) stats.st_size;
    u64 pos  = 0;

    return (Reader) {
        .kind = READER_KIND_FILE,
        .as.file = (File) {
            .fd  = fd,
            .len = len,
            .pos = pos,

            .buf_pos = 0,
            .buf_len = 0,
        },
        .off = 0,
    };
#else
    todo();
#endif
}
#define reader_from_filec(cstr) reader_from_file((cstr), strlen(cstr))
#define reader_from_filed(cstr) reader_from_file((cstr), sizeof(cstr) - 1)
#define reader_from_files(s) reader_from_file((s).data, (s).len)

b8 reader_peek_nth_u8(Reader *r, u8 *b, u64 n) {
    switch(r->kind) {
        case READER_KIND_MEMORY: {
            Memory *m = &r->as.memory;

            if(n < m->len) {
                *b = m->data[0];
                return 1;
            } else {
                return 0;
            }
        } break;
        case READER_KIND_FILE: {
            File *f = &r->as.file;
            if(f->buf_len <= f->buf_pos) {

#ifdef _WIN32
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
#elif __APPLE__ || linux
                if(f->len <= f->pos) {
                    close(f->fd);
                    return 0;
                }

                ssize_t m = read(f->fd, f->buf, sizeof(f->buf));
                if(m <= 0) {
                    todo();
                }
                f->pos += m;
                f->buf_pos = 0;
                f->buf_len = m;
#else
                todo();
#endif 

            }
    
            if(r->off + n < f->len) {
                if(f->buf_pos + n < f->buf_len) {
                    *b = f->buf[f->buf_pos + n];
                    return 1;
                } else {
                    todo();
                }

            } else {
                return 0;
            }
        } break;
        default: todo();
    }
}

b8 reader_peek_u8(Reader *r, u8 *b) {
    return reader_peek_nth_u8(r, b, 0);
}

b8 reader_read_u8(Reader *r, u8 *b) {
    switch(r->kind) {
        case READER_KIND_MEMORY: {
            Memory *m = &r->as.memory;

            if(0 < m->len) {
                *b = m->data[0];
                m->data += 1;
                m->len  -= 1;
                r->off += 1;
                return 1;
            } else {
                return 0;
            }
        } break;
        case READER_KIND_FILE: {
            File *f = &r->as.file;
            if(f->buf_len <= f->buf_pos) {

#ifdef _WIN32
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
#elif __APPLE__ || linux
                if(f->len <= f->pos) {
                    close(f->fd);
                    return 0;
                }

                ssize_t m = read(f->fd, f->buf, sizeof(f->buf));
                if(m <= 0) {
                    todo();
                }
                f->pos += m;
                f->buf_pos = 0;
                f->buf_len = m;
#else
                todo();
#endif 

            }

            *b = f->buf[f->buf_pos++];
            r->off += 1;
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

b8 reader_peek_str(Reader *r, str s) {
    u64 i=0;
    u8 b;
    while(i<s.len && reader_peek_nth_u8(r, &b, i) && b == s.data[i]) {
        i += 1;
    }

    return i == s.len;
}
#define reader_peek_strd(r, cstr) reader_peek_str((r), (str) str_fromd(cstr))

b8 reader_expect_str(Reader *r, str s) {
    u64 i=0;
    u8 b;
    while(i<s.len && reader_peek_u8(r, &b) && b == s.data[i]) {
        if(!reader_read_u8(r, &b)) todo();
        i += 1;
    }

    return i == s.len;
}
#define reader_expect_strd(r, cstr) reader_expect_str((r), (str) str_fromd(cstr))

u8 json_whitespace[] = { ' ', '\n', '\r', '\t' };
u64 json_whitespace_len = sizeof(json_whitespace) / sizeof(*json_whitespace);

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
    if(!reader_read_u8(r, &b) || b != '\"') {
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
                case '"': da_append(&bs, '\"'); break;
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

    *s = (str) str_from(bs.data, bs.len);
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
                    state = EXPONENT_IDLE;
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
        reader_skip_any(r, json_whitespace, json_whitespace_len);

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

            reader_skip_any(r, json_whitespace, json_whitespace_len);

            if(!reader_read_u8(r, &b) || b != ':') {
                return 0;
            }

            reader_skip_any(r, json_whitespace, json_whitespace_len);

            Value v;
            if(!reader_read_json_value(r, &v)) {
                todo();
                return 0;
            }

            reader_skip_any(r, json_whitespace, json_whitespace_len);

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
        reader_skip_any(r, json_whitespace, json_whitespace_len);

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
            reader_skip_any(r, json_whitespace, json_whitespace_len);

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

    reader_skip_any(r, json_whitespace, json_whitespace_len);

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
            } break;
        }
    }



    return 1;
}

b8 reader_skip_until_including_any(Reader *r, str *s, u64 s_len) {

    while(1) {
        b8 found = 0;
        for(u64 i=0;!found && i<s_len;i++) {
            if(reader_expect_str(r, s[i])) {
                found = 1;
            }
        }
        if(found) {
            break;
        }

        u8 b;
        if(reader_read_u8(r, &b)) {

        } else {
            return 0; 
        }
    }

    return 1;
}

u8 toml_whitespace[] = { ' ', '\t' };
u64 toml_whitespace_len = sizeof(toml_whitespace) / sizeof(*toml_whitespace);

str toml_newlines[] = {
    str_fromd("\r\n"),
    str_fromd("\n"),
};
u64 toml_newlines_len = sizeof(toml_newlines)/sizeof(*toml_newlines);

u8 toml_whitespace_and_new_line[] = { ' ', '\t', '\r', '\n' };
u64 toml_whitespace_and_new_line_len = sizeof(toml_whitespace_and_new_line) / sizeof(*toml_whitespace_and_new_line);

b8 u8_eq_any(u8 b, u8 *bs, u64 bs_len) {
    for(u64 i=0;i<bs_len;i++) {
        if(bs[i] == b) {
            return 1;
        }
    }
    return 0;
}

typedef enum {
    TOML_KEY_RETURN_DONE,
    TOML_KEY_RETURN_DOT,
    TOML_KEY_RETURN_ERROR,
} Toml_Key_Return;

Toml_Key_Return reader_read_toml_key(Reader *r, str *s) {
    // A-Za-z0-9_-

    // TODO: remove prefix
    typedef enum {
        TOML_KEY_MODE_NO_QOUTE,
        TOML_KEY_MODE_SINGLE_QOUTE,
        TOML_KEY_MODE_DOUBLE_QOUTE,
    } Toml_Key_Mode;
    Toml_Key_Mode mode = TOML_KEY_MODE_NO_QOUTE;

    u8 b;
    if(!reader_peek_u8(r, &b)) {
        return TOML_KEY_RETURN_ERROR;
    }

    u64 off = r->off;

    b8 running = 1;
    u8s bs = {0};
    Toml_Key_Return ret = TOML_KEY_RETURN_DONE;
    while(running) {
        switch(mode) {
            case TOML_KEY_MODE_NO_QOUTE: {
                if(!reader_peek_u8(r, &b)) todo();
                if( ('A' <= b && b <= 'Z') || 
                        ('a' <= b && b <= 'z') || 
                        ('0' <= b && b <= '9') || 
                        b == '_' || b == '-') {
                    if(!reader_read_u8(r, &b)) todo();
                    da_append(&bs, b); 
                } else if(b == '\'') {
                    if(!reader_read_u8(r, &b)) todo();
                    mode = TOML_KEY_MODE_SINGLE_QOUTE;
                } else if(b == '\"') {
                    if(!reader_read_u8(r, &b)) todo();
                    mode = TOML_KEY_MODE_DOUBLE_QOUTE;
                } else if(b == '.') {
                    if(!reader_read_u8(r, &b)) todo();
                    ret = TOML_KEY_RETURN_DOT;
                    running = 0;
                } else {
                    if(u8_eq_any(b, toml_whitespace, toml_whitespace_len)) {
                        if(!reader_read_u8(r, &b)) todo();
                    } else {
                        running = 0;
                    }
                }
            } break;
            case TOML_KEY_MODE_SINGLE_QOUTE: {
                if(!reader_read_u8(r, &b)) todo();
                if(b == '\'') {
                    mode = TOML_KEY_MODE_NO_QOUTE;
                } else {
                    da_append(&bs, b); 
                }
            } break;
            case TOML_KEY_MODE_DOUBLE_QOUTE: {
                if(!reader_read_u8(r, &b)) todo();
                if(b == '\"') {
                    mode = TOML_KEY_MODE_NO_QOUTE;
                } else {
                    da_append(&bs, b); 
                }
            } break;
        }
    }

    if(r->off == off) {
        return TOML_KEY_RETURN_ERROR;
    }

    *s = (str) str_from(bs.data, bs.len);
    return ret;
}

typedef enum {
    TOML_HANDLE_STRING_RESULT_OKAY,
    TOML_HANDLE_STRING_RESULT_SKIP_WHITESPACE,
    TOML_HANDLE_STRING_RESULT_ERROR,
} Toml_Handle_String_Result;

Toml_Handle_String_Result reader_handle_toml_basic_string(Reader *r, u8s *bs, u8 b, b8 is_multi_line) {
    if(b == '\"') {
        todo();
    }

    if(b == '\\') {
        if(!reader_read_u8(r, &b)) {
            return TOML_HANDLE_STRING_RESULT_ERROR;
        }
        switch(b) {
            case '\\': da_append(bs, '\\'); break;
            case '"': da_append(bs, '\"'); break;
            case 'b': da_append(bs, '\b'); break;
            case 'f': da_append(bs, '\f'); break;
            case 'n': da_append(bs, '\n'); break;
            case 'r': da_append(bs, '\r'); break;
            case 't': da_append(bs, '\t'); break;
            case '\n': {
                if(is_multi_line) {
                    return TOML_HANDLE_STRING_RESULT_SKIP_WHITESPACE;
                } else {
                    return 0;
                }
            } break; 
            case 'u': {
                u32 n = 0;
                for(u8 i=0;i<4;i++) {
                    if(!reader_read_u8(r, &b)) {
                        return TOML_HANDLE_STRING_RESULT_ERROR;
                    }

                    n *= 16;
                    if('0' <= b && b <= '9') {
                        n += b - '0';
                    } else if('a' <= b && b <= 'f') {
                        n += b - 'W';
                    } else if('A' <= b && b <= 'F') {
                        n += b - '7';
                    } else {
                        return TOML_HANDLE_STRING_RESULT_ERROR;
                    }
                }

                rune_encode(n, bs);
            } break;
            case 'U': {
                u32 n = 0;
                for(u8 i=0;i<8;i++) {
                    if(!reader_read_u8(r, &b)) {
                        return TOML_HANDLE_STRING_RESULT_ERROR;
                    }

                    n *= 16;
                    if('0' <= b && b <= '9') {
                        n += b - '0';
                    } else if('a' <= b && b <= 'f') {
                        n += b - 'W';
                    } else if('A' <= b && b <= 'F') {
                        n += b - '7';
                    } else {
                        return TOML_HANDLE_STRING_RESULT_ERROR;
                    }
                }

                rune_encode(n, bs);
            } break;
            default:
                      return TOML_HANDLE_STRING_RESULT_ERROR;
        }
    } else {
        da_append(bs, b);
    } 

    return TOML_HANDLE_STRING_RESULT_OKAY;
}

b8 reader_read_toml_string(Reader *r, str *s) {

    typedef enum {
        BASIC_STRING,
        MULTI_LINE_BASIC_STRING,
        LITERAL_STRING,
        MULTI_LINE_LITERAL_STRING,
    } Mode;
    u64 count = 0;
    b8 skip_whitespace = 0;

    u8 b;
    if(!reader_read_u8(r, &b)) {
        todo();
        // return 0;
    }
    Mode mode;
    if(b == '\'') mode = LITERAL_STRING;
    else if(b == '\"') mode = BASIC_STRING;
    else return 0;

    u8s bs = {0};
    b8 running = 1;
    while(running) {

        switch(mode) {
            case MULTI_LINE_LITERAL_STRING: {
                if(!reader_peek_u8(r, &b)) {
                    todo();
                    if(2 <= count) {
                        for(u64 i=0;i<(count - 2);i++) da_append(&bs, '\'');
                        running = 0;
                        break;
                    } else {
                        return 0;
                    }
                }
                if(b == '\'') {
                    if(!reader_read_u8(r, &b)) todo();
                    if(count == 6) {
                        return 0;
                    }
                    count += 1;
                } else {
                    if(count == 0) {

                    } else if(count < 3) {
                        for(u64 i=0;i<count;i++) da_append(&bs, '\'');
                        count = 0;
                    } else if(3 <= count) {
                        for(u64 i=0;i<(count - 3);i++) da_append(&bs, '\'');
                        running = 0;
                        break;
                    }

                    if(!reader_read_u8(r, &b)) todo();
                    da_append(&bs, b);
                }

            } break;
            case LITERAL_STRING: {
                if(!reader_read_u8(r, &b)) todo();
                if(b == '\'') {
                    if(bs.len == 0 && reader_peek_u8(r, &b) && b == '\'') {
                        if(!reader_read_u8(r, &b)) todo();
                        if(reader_peek_u8(r, &b)) {
                            if(b == '\n') {
                                if(!reader_read_u8(r, &b)) todo();
                            } else if(b == '\r') {
                                if(reader_peek_nth_u8(r, &b, 1) && b == '\n') {
                                    if(!reader_read_u8(r, &b)) todo();
                                    if(!reader_read_u8(r, &b)) todo();
                                }
                            }
                        }

                        mode = MULTI_LINE_LITERAL_STRING;
                        count = 0;
                    } else {
                        running = 0;
                    }
                } else {
                    da_append(&bs, b);
                }
            } break;
            case BASIC_STRING: {
                if(!reader_read_u8(r, &b)) todo();
                if(b == '\"') {
                    if(bs.len == 0 && reader_peek_u8(r, &b) && b == '\"') {
                        if(!reader_read_u8(r, &b)) todo();
                        if(reader_peek_u8(r, &b)) {
                            if(b == '\n') {
                                if(!reader_read_u8(r, &b)) todo();
                            } else if(b == '\r') {
                                if(reader_peek_nth_u8(r, &b, 1) && b == '\n') {
                                    if(!reader_read_u8(r, &b)) todo();
                                    if(!reader_read_u8(r, &b)) todo();
                                }
                            }
                        }

                        mode = MULTI_LINE_BASIC_STRING;
                        count = 0;
                        skip_whitespace = 0;
                    } else {
                        running = 0;
                    }
                } else {
                    if(reader_handle_toml_basic_string(r, &bs, b, 0) == TOML_HANDLE_STRING_RESULT_ERROR) {
                        return 0;
                    }
                }
            } break;
            case MULTI_LINE_BASIC_STRING: {
                if(!reader_peek_u8(r, &b)) {
                    todo();
                    if(2 <= count) {
                        for(u64 i=0;i<(count - 2);i++) da_append(&bs, '\"');
                        running = 0;
                        break;
                    } else {
                        return 0;
                    }
                }
                if(skip_whitespace) {
                    if(u8_eq_any(b, toml_whitespace_and_new_line, toml_whitespace_and_new_line_len)) {
                        if(!reader_read_u8(r, &b)) todo();
                        break;
                    } else {
                        skip_whitespace = 0;
                    }
                }
                if(b == '\"') {
                    if(!reader_read_u8(r, &b)) todo();
                    count += 1;
                } else {
                    if(count == 0) {

                    } else if(count < 3) {
                        for(u64 i=0;i<count;i++) da_append(&bs, '\"');
                        count = 0;
                    } else if(3 <= count) {
                        for(u64 i=0;i<(count - 3);i++) da_append(&bs, '\"');
                        running = 0;
                        break;
                    }

                    if(!reader_read_u8(r, &b)) todo();
                    switch(reader_handle_toml_basic_string(r, &bs, b, 1)) {
                        case TOML_HANDLE_STRING_RESULT_OKAY: {
                        } break;
                        case TOML_HANDLE_STRING_RESULT_SKIP_WHITESPACE: {
                            skip_whitespace = 1; 
                        } break;
                        case TOML_HANDLE_STRING_RESULT_ERROR: {
                            return 0;
                        } break;
                    }
                }
            } break;

            default:
                todo();
        }

    }

    *s = (str) str_from(bs.data, bs.len);
    return 1;
}

b8 reader_read_toml_number(Reader *r, f64 *number) {

    typedef enum {
        IDLE,
        AFTER_SIGN,
        LEFT,
        ZERO,
        ZERO_START,
        RIGHT,
        EXPONENT_IDLE,
        EXPONENT,
        HEX,
        OCTAL,
        BINARY,
    } State;
    State state = IDLE;

    b8 prev_was_underscore = 0;

    b8 negative = 0;
    b8 exponent_negative = 0;
    u64 left = 0;
    u64 right = 0;
    u64 right_count = 0;
    u64 exponent = 1;

    u64 off = r->off;

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
                    state = AFTER_SIGN;
                } else if(b == '+') {
                    // negative = 0;
                    state = AFTER_SIGN;
                } else if(b == '0') {
                    state = ZERO_START;
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
            case ZERO_START: {
                if(b == '.') {
                    state = RIGHT;
                } else if(b == 'e' || b == 'E') {
                    state = EXPONENT_IDLE;
                } else if(b == 'x') {
                    state = HEX;
                } else if(b == 'o') {
                    state = OCTAL;
                } else if(b == 'b') {
                    state = BINARY;
                } else {
                    done = 1;
                }
            } break;
            case AFTER_SIGN: {
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
                    state = EXPONENT_IDLE;
                } else if(b == '_') {
                    if(prev_was_underscore) return 0;
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
                    if(right_count == 0) return 0;
                    state = EXPONENT_IDLE;
                } else if(b == '_') {
                    if(prev_was_underscore) return 0;
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
            case HEX: {
                if(b == '_') {
                    if(off + 2 == r->off) return 0;
                    if(prev_was_underscore) return 0;
                } else if('0' <= b && b <= '9') {
                    left *= 16;
                    left += b - '0';
                } else if('a' <= b && b <= 'f') {
                    left *= 16;
                    left += b - 'W';
                } else if('A' <= b && b <= 'F') {
                    left *= 16;
                    left += b - '7';
                } else {
                    done = 1;
                }
            } break;
            case OCTAL: {
                if(b == '_') {
                    if(off + 2 == r->off) return 0;
                    if(prev_was_underscore) return 0;
                } else if('0' <= b && b <= '7') {
                    left *= 8;
                    left += b - '0';
                } else {
                    done = 1;
                }
            } break;
            case BINARY: {
                if(b == '_') {
                    if(off + 2 == r->off) return 0;
                    if(prev_was_underscore) return 0;
                } else if('0' <= b && b <= '1') {
                    left *= 2;
                    left += b - '0';
                } else {
                    done = 1;
                }
            } break;
            default: todo();
        }

        prev_was_underscore = b == '_';

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
        case AFTER_SIGN: return 0;
        case LEFT: return 1;
        case ZERO: return 1;
        case ZERO_START: return 1;
        case RIGHT: return 0 < right_count;
        case EXPONENT_IDLE: return 0;
        case EXPONENT: return 1;
        case HEX: 
        case OCTAL:
        case BINARY:
            return (off + 2) < r->off;
        default: todo();
    }
}

b8 reader_read_toml_value(Reader *r, Value *v) {

    u8 b;
    if(!reader_peek_u8(r, &b)) {
        return 0;
    }
    if('0' <= b && b <= '9') {
        f64 n;
        if(!reader_read_toml_number(r, &n)) {
            return 0;
        }
        *v = value_number(n);
    } else {
        switch(b) {
            case '\"': 
            case '\'': {
                str s;
                if(!reader_read_toml_string(r, &s)) {
                    return 0;
                }

                *v = value_str(s);
            } break;
            case '#': {
                return 0;
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
            case '-':
            case '+': {
                if(reader_peek_strd(r, "+inf")) {
                    if(!reader_expect_strd(r, "+inf")) todo();
                    *v = value_number(1.0 / 0.0);
                    break;
                }
                if(reader_peek_strd(r, "-inf")) {
                    if(!reader_expect_strd(r, "-inf")) todo();
                    *v = value_number(-1.0 / 0.0);
                    break;
                }
                if(reader_peek_strd(r, "+nan")) {
                    if(!reader_expect_strd(r, "+nan")) todo();
                    *v = value_number(0.0 / 0.0);
                    break;
                }
                if(reader_peek_strd(r, "-nan")) {
                    if(!reader_expect_strd(r, "-nan")) todo();
                    *v = value_number(-0.0 / 0.0);
                    break;
                }

                f64 n;
                if(!reader_read_toml_number(r, &n)) {
                    return 0;
                }
                *v = value_number(n);
            } break;
            case 'n': {
                if(!reader_expect_strd(r, "nan")) {
                    return 0;
                }
                *v = value_number(0.0 /0.0);
            } break;
            case '.': {
                return 0;
            } break;
            case 'i': {
                if(!reader_expect_strd(r, "inf")) {
                    return 0;
                }
                *v = value_number(1.0 /0.0);
            } break;
            default: {
                printf("'%c' (0x%02x)\n", b, b);
                todo();
            } break;
        }
    }

    return 1;

}

b8 reader_read_toml_file(Reader *r, Value *object) {
    if(object->kind != KIND_OBJECT) todo();

    while(1) {
        reader_skip_any(r, toml_whitespace, toml_whitespace_len);

        u8 b;
        if(!reader_peek_u8(r, &b)) {
            break;
        }
        switch(b) {
            case '\r': {
                if(!reader_read_u8(r, &b)) return 0;
                if(!reader_read_u8(r, &b) || b != '\n') return 0;
            } break;
            case '\n': {
                if(!reader_read_u8(r, &b)) return 0;
            } break;
            case '#': {
                if(!reader_read_u8(r, &b)) return 0;
                if(!reader_skip_until_including_any(r, toml_newlines, toml_newlines_len)) return 0;
            } break;
            default: {
                Value *obj = object;

                b8 running = 1;
                str key;
                while(running) {
                    switch(reader_read_toml_key(r, &key)) {
                        case TOML_KEY_RETURN_DONE: {
                            running = 0;
                        } break;
                        case TOML_KEY_RETURN_DOT: {

                            if(object_get(obj, key, &obj)) {
                            } else {
                                Value value = value_object();
                                object_set(obj, key, value);
                                if(!object_get(obj, key, &obj)) todo();
                            }
                            if(obj->kind != KIND_OBJECT) {
                                return 0;
                            }

                        } break;
                        case TOML_KEY_RETURN_ERROR: {
                            return 0;
                        } break;
                    }
                }

                reader_skip_any(r, toml_whitespace, toml_whitespace_len);
                if(!reader_read_u8(r, &b) || b != '=') return 0;
                reader_skip_any(r, toml_whitespace, toml_whitespace_len);

                Value value;
                if(!reader_read_toml_value(r, &value)) {
                    return 0;
                }

                if(object_set(obj, key, value)) {
                    return 0;
                }

                reader_skip_any(r, toml_whitespace, toml_whitespace_len);
                if(reader_peek_u8(r, &b)) {
                    if(b == '#' || b == '\r' || b == '\n') {
                        if(!reader_skip_until_including_any(r, toml_newlines, toml_newlines_len)) return 0;
                    } else {
                        return 0;
                    }
                }

            } break;
        }

    }


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
        Value *test = array_get(&tests, i);
        if(test->kind != KIND_OBJECT) todo();

        Value *toml;
        if(!object_get(test, (str) str_fromd("toml"), &toml)) todo();
        Value *json;
        b8 json_found = object_get(test, (str) str_fromd("json"), &json);

        if(toml->kind != KIND_STR) todo();

        Reader toml_reader = reader_from_files(toml->as.string);
        printf("INFO: parsing '"str_fmt"' ...\n", str_arg(toml->as.string));
        Value toml_value = value_object();
        b8 read_toml = reader_read_toml_file(&toml_reader, &toml_value);
    
        if(json_found) { 
            if(json->kind == KIND_NULL) {
                if(read_toml) {
                    fprintf(stderr, str_fmt":%llu:ERROR: Could read toml although, it should be invalid\n", str_arg(toml->as.string), toml_reader.off);
                    todo();
                } else {
                    // fine
                }
            } else {
                if(read_toml) {
                    if(json->kind != KIND_STR) todo();
                    reader = reader_from_files(json->as.string);
                    Value json_value;
                    if(!reader_read_json_value(&reader, &json_value)) {
                        todo();
                    }

                    if(!value_eq(toml_value, json_value)) {
                        value_print(toml_value); printf("\n");
                        value_print(json_value); printf("\n");

                        todo();
                    }

                } else {
                    fprintf(stderr, str_fmt":%llu:ERROR: Cannot parse toml\n", str_arg(toml->as.string), toml_reader.off);
                    todo();
                }
            }
        } else {
            if(read_toml) {
                // fine

                // There is no json equivalent for this :D
            } else {
                fprintf(stderr, str_fmt":%llu:ERROR: Cannot parse toml\n", str_arg(toml->as.string), toml_reader.off);
                todo();
            }
        }
    }

    return 0;
}
