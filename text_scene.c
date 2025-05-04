#include "text_scene.h"

#include <string.h>

#ifndef TEXT_SCENE_IGNORE_STDLIB
#include <stdlib.h>
#endif

#define assert(c) if (!(c)) __builtin_trap()

typedef struct
{
    char *data;
    ptrdiff_t len;
} TS_Str;

#define S(c) (TS_Str) {c, sizeof(c) - 1} // String
#define U(s, d) (TS_Str) {s, d}          // User Defined

typedef struct
{
    TS_Str current;
    _Bool in_bracket;
    _Bool error;
} TS_State;

typedef struct
{
    char *beg;
    char *end;
} TS_Linear_Allocator;

typedef struct {
    TS_Str   head;
    TS_Str   tail;
    _Bool ok;
} TS_Cut;

_Bool equals(TS_Str a, TS_Str b)
{
    return a.len==b.len && (!a.len || !memcmp(a.data, b.data, a.len));
}

TS_Str trimleft(TS_Str s)
{
    for (; s.len && *s.data<=' '; s.data++, s.len--) {}
    return s;
}

TS_Str trimright(TS_Str s)
{
    for (; s.len && s.data[s.len-1]<=' '; s.len--) {}
    return s;
}

TS_Str substring(TS_Str s, ptrdiff_t i)
{
    if (i) {
        s.data += i;
        s.len  -= i;
    }
    return s;
}

TS_Str span(char *beg, char *end)
{
    TS_Str r = {0};
    r.data = beg;
    r.len  = beg ? end-beg : 0;
    return r;
}

TS_Cut cut(TS_Str s, char c)
{
    TS_Cut r = {0};
    if (!s.len) return r;  // null pointer special case
    char *beg = s.data;
    char *end = s.data + s.len;
    char *cut = beg;
    for (; cut<end && *cut!=c; cut++) {}
    r.ok   = cut < end;
    r.head = span(beg, cut);
    r.tail = span(cut+r.ok, end);
    return r;
}

TS_Cut cut_either(TS_Str s, char c0, char c1) // cut at either c0 or c1
{
    TS_Cut r = {0};
    if (!s.len) return r;  // null pointer special case

    char *beg = s.data;
    char *end = s.data + s.len;
    char *cut = beg;

    for (; cut < end; cut++) {
        if (*cut == c0 || *cut == c1) break;
    }

    if (cut < end)
    {
        r.ok   = 1;
        r.head = span(beg, cut);
        r.tail = span(cut + 1, end);
    }

    return r;
}

#define new(a, t, n)  (t *)ts_alloc(a, n, sizeof(t), _Alignof(t))
static void *ts_alloc(TS_Linear_Allocator *a, ptrdiff_t count, ptrdiff_t size, ptrdiff_t align)
{
    ptrdiff_t pad = (uintptr_t)a->end & (align - 1);
    if (count > (a->end - a->beg - pad)/size) {
        return 0;
    }
    void *r = a->beg + pad;
    a->beg += pad + count*size;
    return memset(r, 0, count*size);
}

#ifndef TEXT_SCENE_IGNORE_STDLIB
static void *ts_malloc(ptrdiff_t size, void *ctx)
{
    (void) ctx;
    return malloc(size);
}

static void ts_free(void *ptr, void *ctx)
{
    (void) ctx;
    free(ptr);
}
#endif

static TS_Allocator ts_get_stdlib_allocator()
{
    TS_Allocator allocator = {0};

#ifndef TEXT_SCENE_IGNORE_STDLIB
    allocator.malloc = &ts_malloc;
    allocator.free   = &ts_free;
#endif

    return allocator;
}

typedef struct
{
    _Bool ok;
    TS_Chunk chunk;
} TS_Chunk_Result;

typedef struct
{
    _Bool ok;
    TS_Pair pair;
} TS_Pair_Result;

static TS_Chunk_Result ts_next_heading(TS_State *state)
{
    TS_Chunk_Result result = {0};

    TS_Cut c = {0};
    c.tail = state->current;

    c = cut(c.tail, '\n');
    TS_Str line = c.head;
    while (line.len && line.data[0] != '[')
    {
        c = cut(c.tail, '\n');
        line = c.head;
    }

    if (line.len && line.data[0] == '[')
    {
        line = substring(line, 1);
        TS_Cut header_cut = cut(line, ' ');

        _Bool found_header_match = 0;
        if (equals(S("gd_scene"), header_cut.head))
        {
            result.chunk.heading = TS_Heading_File_Descriptor;
            found_header_match = 1;
        }
        else if (equals(S("ext_resource"), header_cut.head))
        {
            result.chunk.heading = TS_Heading_Ext_Resource;
            found_header_match = 1;
        }
        else if (equals(S("sub_resource"), header_cut.head))
        {
            result.chunk.heading = TS_Heading_Sub_Resource;
            found_header_match = 1;
        }
        else if (equals(S("node"), header_cut.head))
        {
            result.chunk.heading = TS_Heading_Node;
            found_header_match = 1;
        }
        else if (equals(S("connection"), header_cut.head))
        {
            result.chunk.heading = TS_Heading_Node;
            found_header_match = 1;
        }

        if (found_header_match)
        {
            result.chunk.source.data = header_cut.head.data;
            result.chunk.source.len  = header_cut.head.len;
            result.ok = 1;
        }
    }

    state->current = c.tail;

    return result;
}

static TS_Pair_Result ts_next_heading_pair(TS_State *state)
{
    TS_Pair_Result result = {0};

    // If not currently inside a bracket, seek the next opening bracket
    if (!state->in_bracket) {
        while (state->current.len > 0 && *state->current.data != '[') {
            state->current.data++;
            state->current.len--;
        }

        if (state->current.len > 0 && *state->current.data == '[') {
            state->current.data++;
            state->current.len--;
            state->in_bracket = 1;
        }

        TS_Cut c = {0};
        c.tail = state->current;
        c = cut(c.tail, ' ');
        state->current = c.tail;
    }

    // If we're in a bracket, try parsing a key=value pair
    if (state->in_bracket) {
        // Skip whitespace
        while (state->current.len > 0 && *state->current.data == ' ') {
            state->current.data++;
            state->current.len--;
        }

        // Parse key
        char *key_start = state->current.data;
        ptrdiff_t key_len = 0;
        while (key_len < state->current.len &&
               key_start[key_len] != '=' &&
               key_start[key_len] != ' ') {
            key_len++;
        }

        if (key_len < state->current.len && key_start[key_len] == '=') {
            result.pair.key.data = key_start;
            result.pair.key.len = key_len;

            state->current.data += key_len + 1;
            state->current.len  -= key_len + 1;

            // Parse value
            if (state->current.len > 0 && *state->current.data == '"') {
                state->current.data++;
                state->current.len--;

                char *val_start = state->current.data;
                ptrdiff_t val_len = 0;
                while (val_len < state->current.len && val_start[val_len] != '"') {
                    val_len++;
                }

                result.pair.value.data = val_start;
                result.pair.value.len = val_len;

                if (val_len < state->current.len) {
                    state->current.data += val_len + 1;
                    state->current.len  -= val_len + 1;
                } else {
                    state->error = 1; // Unterminated quoted string
                }
            } else {
                char *val_start = state->current.data;
                ptrdiff_t val_len = 0;
                while (val_len < state->current.len &&
                       val_start[val_len] != ' ' &&
                       val_start[val_len] != ']') {
                    val_len++;
                }

                result.pair.value.data = val_start;
                result.pair.value.len = val_len;

                state->current.data += val_len;
                state->current.len  -= val_len;
            }

            result.ok = 1;
        }

        // If we reach the closing bracket, exit bracket mode
        while (state->current.len > 0 && *state->current.data == ' ') {
            state->current.data++;
            state->current.len--;
        }

        if (state->current.len > 0 && *state->current.data == ']') {
            state->current.data++;
            state->current.len--;
            state->in_bracket = 0;
        }
    }

    return result;
}

static TS_Pair_Result ts_next_pair(TS_State *state)
{
    TS_Pair_Result result = {0};

    TS_Cut c = {0};
    c.tail = state->current;

    TS_Str line = {0};
    while (c.tail.len) {
        c = cut(c.tail, '\n');

        // found a non header pair?
        if (c.head.data[0] != '[' && c.head.data[0] != ' ' && c.head.data[0] != '\r' && c.head.data[0] != '\t')
        {
            line = c.head;
            state->current = c.tail;
            break;
        }
    }

    if (line.len)
    {
        TS_Cut kv = cut(line, '=');
        kv.head = trimleft(kv.head);
        kv.head = trimright(kv.head);
        result.pair.key.data = kv.head.data;
        result.pair.key.len  = kv.head.len;

        kv = cut(kv.tail, '\n');
        kv.head = trimleft(kv.head);
        kv.head = trimright(kv.head);

        char *val = kv.head.data;
        ptrdiff_t len = kv.head.len;

        // Handle quoted value
        if (len > 1 && val[0] == '"' && val[len - 1] == '"') {
            val++;
            len -= 2;

            char *src = val;
            char *dst = val;
            int escaped = 0;

            for (ptrdiff_t i = 0; i < len; i++) {
                char ch = src[i];
                if (escaped) {
                    switch (ch) {
                    case 'n':  *dst++ = '\n'; break;
                    case 't':  *dst++ = '\t'; break;
                    case '"':  *dst++ = '"';  break;
                    case '\\': *dst++ = '\\'; break;
                    default:   *dst++ = ch;   break;
                    }
                    escaped = 0;
                } else if (ch == '\\') {
                    escaped = 1;
                } else {
                    *dst++ = ch;
                }
            }
            len = dst - val;
        }

        result.pair.value.data = val;
        result.pair.value.len  = len;

        result.ok = 1;
    }
    return result;
}

TS_Load_Result ts_load(char *source)
{
    TS_Allocator allocator = ts_get_stdlib_allocator();
    return ts_load1(source, strlen(source), &allocator);
}

TS_Load_Result ts_load1(char *source, ptrdiff_t source_len, TS_Allocator *allocator)
{
    TS_Allocator heap;
    if (!allocator)
    {
        heap = ts_get_stdlib_allocator();
        allocator = &heap;
    }

    TS_Load_Result result = {0};
    ptrdiff_t heading_count = 0;
    ptrdiff_t heading_pair_count = 0;
    ptrdiff_t pair_count = 0;

    TS_Chunk_Result current = {0};
    TS_Pair_Result  current_pair = {0};
    TS_State state = {0};
    state.current = U(source, source_len);

    // count headers
    while ((current = ts_next_heading(&state)).ok)
    {
        heading_count += 1;
    }

    // count heading pairs
    if (!state.error)
    {
        state.current = U(source, source_len);
        while ((current_pair = ts_next_heading_pair(&state)).ok)
        {
            heading_pair_count += 1;
        }
    }

    // count pairs
    if (!state.error)
    {
        state.current = U(source, source_len);
        while ((current_pair = ts_next_pair(&state)).ok)
        {
            pair_count += 1;
        }
    }

    if (!state.error)
    {
        result.chunks    = allocator->malloc(heading_count * sizeof(TS_Chunk), allocator->ctx);
        result.all_pairs = allocator->malloc((heading_pair_count + pair_count) * sizeof(TS_Pair), allocator->ctx);
    }

    if (result.chunks && result.all_pairs)
    {
        TS_Linear_Allocator chunk_arena = {0};
        chunk_arena.beg = (char*) result.chunks;
        chunk_arena.end = (char*) result.chunks + heading_count * sizeof(TS_Chunk);

        TS_Linear_Allocator pair_arena = {0};
        pair_arena.beg = (char*) result.all_pairs;
        pair_arena.end = (char*) result.all_pairs + ((heading_pair_count + pair_count) * sizeof(TS_Pair));

        state.current = U(source, source_len);
        while ((current = ts_next_heading(&state)).ok)
        {
            TS_Chunk *chunk = new(&chunk_arena, TS_Chunk, 1);
            *chunk = current.chunk;

            if (result.chunks_len == 0)
            {
                result.chunks = chunk;
            }

            result.chunks_len += 1;
        }

        for (int i = 0; i < result.chunks_len; i++)
        {
           TS_Chunk *chunk = &result.chunks[i];

           // Create sub-context to scan heading pairs inside `[]`
           TS_State chunk_context = {0};
           chunk_context.current.data = chunk->source.data - 1;
           chunk_context.current.len = 1;

           char *iter = chunk->source.data;
           while (*iter && *iter != ']') iter++;
           if (*iter == ']') iter++;
           chunk_context.current.len = iter - chunk_context.current.data;

           while ((current_pair = ts_next_heading_pair(&chunk_context)).ok)
           {
               TS_Pair *heading_pair = new(&pair_arena, TS_Pair, 1);
               *heading_pair = current_pair.pair;
               if (!chunk->heading_pairs)
                   chunk->heading_pairs = heading_pair;
               chunk->heading_pairs_len += 1;
           }
        }

        for (int i = 0; i < result.chunks_len; i++)
        {
            TS_Chunk *chunk = &result.chunks[i];
            _Bool last_chunk = i == result.chunks_len - 1;

            // Funky source narrowing. Basically create a buffer from
            // The chunk [ to the next chunk [ or eof.
            TS_State chunk_context = {0};
            chunk_context.current.data = chunk->source.data;

            if (!last_chunk)
            {
                TS_Chunk *next_chunk = &result.chunks[i + 1];
                chunk_context.current.len = next_chunk->source.data - chunk->source.data;
            }
            else
            {
                chunk_context.current.len = source_len - (chunk->source.data - source);
            }

            chunk_context.current.data--;
            chunk_context.current.len++;
            // Narrowing End

            while ((current_pair = ts_next_pair(&chunk_context)).ok)
            {
                TS_Pair *pair = new(&pair_arena, TS_Pair, 1);
                *pair = current_pair.pair;

                if (!chunk->pairs)
                {
                    chunk->pairs = pair;
                }

                chunk->pairs_len += 1;
            }
        }

        result.ok = 1;
    }


    return result;
}

#undef S
#undef U
#undef new

