#ifndef TEXT_SCENE_H
#define TEXT_SCENE_H

#include <stdint.h>

typedef struct
{
    void *(*malloc)(ptrdiff_t, void *ctx);
    void  (*free)(void *, void *ctx);
    void   *ctx;
} TS_Allocator;

typedef enum
{
    TS_Heading_Nothing,
    TS_Heading_File_Descriptor,
    TS_Heading_Ext_Resource,
    TS_Heading_Sub_Resource,
    TS_Heading_Node,
    TS_Heading_Connection,
} TS_Heading;

typedef struct
{
    struct
    {
        char *data;
        ptrdiff_t len;
    } key;

    struct
    {
        char *data;
        ptrdiff_t len;
    } value;
} TS_Pair;

typedef struct
{
    struct {
        char *data;
        ptrdiff_t len;
    } source;
    TS_Heading heading;
    TS_Pair *heading_pairs;
    TS_Pair *pairs;

    ptrdiff_t heading_pairs_len;
    ptrdiff_t pairs_len;
} TS_Chunk;

typedef struct
{
    _Bool ok;
    TS_Chunk *chunks;
    ptrdiff_t chunks_len;

    TS_Pair *all_pairs;
    ptrdiff_t all_pairs_len;
} TS_Load_Result;

typedef struct
{
    _Bool ok;
    char *output;
    ptrdiff_t output_len;
} TS_Save_Result;

TS_Load_Result ts_load(char *source);
TS_Load_Result ts_load1(char *source, ptrdiff_t source_len, TS_Allocator *allocator);
TS_Save_Result ts_save(TS_Chunk *chunks, ptrdiff_t chunks_len, char *savepath);
TS_Save_Result ts_save1(TS_Chunk *chunks, ptrdiff_t chunks_len, TS_Allocator *allocator);

#endif // TEXT_SCENE_H

#ifdef TS_IMPLEMENTATION
#include "text_scene.c"
#endif
