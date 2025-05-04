#include <stdio.h> 

TS_Str loadfile(TS_Linear_Allocator *a, FILE *f)
{
    TS_Str r  = {0};
    r.data = a->beg;
    r.len  = a->end - a->beg;
    r.len  = fread(r.data, 1, r.len, f);
    return r;
}

#include <stdio.h>

void print_str(char *data, ptrdiff_t len) {
    for (ptrdiff_t i = 0; i < len; i++) putchar(data[i]);
}

const char *heading_to_str(TS_Heading heading) {
    switch (heading) {
        case TS_Heading_File_Descriptor: return "FileDescriptor";
        case TS_Heading_Ext_Resource:    return "ExtResource";
        case TS_Heading_Sub_Resource:    return "SubResource";
        case TS_Heading_Node:            return "Node";
        case TS_Heading_Connection:      return "Connection";
        case TS_Heading_Nothing:         return "Nothing";
        default:                         return "Unknown";
    }
}

void print_ts_load_result(const TS_Load_Result *result) {
    for (int i = 0; i < result->chunks_len; ++i) {
        TS_Chunk chunk = result->chunks[i];
        switch (chunk.heading) {
            case TS_Heading_Nothing:
                break;

            case TS_Heading_File_Descriptor:
                printf("[gd_scene ");
                break;

            case TS_Heading_Node:
                printf("[node ");
                break;

            case TS_Heading_Sub_Resource:
                printf("[sub_resource ");
                break;

            case TS_Heading_Ext_Resource:
                printf("[ext_resource ");
                break;
        }

        // Print key-value pairs
        for (int j = 0; j < chunk.heading_pairs_len; ++j) {
            TS_Pair pair = chunk.heading_pairs[j];
            printf("%.*s=", (int)pair.key.len, pair.key.data);
            if (pair.value.len > 0 && pair.value.data[0] == '"') {
                printf("%.*s", (int)pair.value.len, pair.value.data); // quoted value
            } else {
                printf("%.*s", (int)pair.value.len, pair.value.data); // raw value
            }
            if (j != chunk.heading_pairs_len - 1)
            {
                printf(" "); // raw value
            }
        }

        printf("]\n");

        for (int j = 0; j < chunk.pairs_len; ++j) {
            TS_Pair pair = chunk.pairs[j];
            printf("%.*s = ", (int)pair.key.len, pair.key.data);
            if (pair.value.len > 0 && pair.value.data[0] == '"') {
                printf("%.*s", (int)pair.value.len, pair.value.data); // quoted value
            } else {
                printf("%.*s", (int)pair.value.len, pair.value.data); // raw value
            }
            printf("\n");
        }
        printf("\n");
    }
}

void test(char *what, TS_Linear_Allocator *alloc)
{
    FILE *file = fopen(what, "rb");
    TS_Str content = loadfile(alloc, file);
    fclose(file);

    TS_Load_Result res = ts_load1(content.data, content.len, 0);
    print_ts_load_result(&res);
}


int main(void)
{
    TS_Linear_Allocator alloc = {0};
    alloc.beg = malloc(1 << 30);
    alloc.end = alloc.beg + (1 << 30);

    test("test.tscn", &alloc);
    printf("========\n");
    test("mine.tscn", &alloc);

    return 0;
}
