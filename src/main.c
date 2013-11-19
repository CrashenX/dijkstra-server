/* Dijkstra Server Reference Solution
 *
 * Copyright (c) 2013 Jesse J. Cook
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define DEBUG
#define MAX_VERTICES 65536

struct edge {
    uint16_t dest;       // The index of the destination
    uint16_t cost;       // The cost to follow the edge to dest
    struct edge *next;   // Next edge in list
};
typedef struct edge edge_t;

// A vertex id of 0 is invalid. Therefore, 0 is used as NULL or empty
typedef struct {
    // Edge data
    edge_t *head;          // list of all outbound edges
    edge_t *tail;          // tail pointer for edge list
    // Traversal metadata
    uint32_t distance;     // current shortest distance to vertex
    char visited;          // vertex has been visited or not
    uint16_t previous;     // last vertex in shortest path here
    // Queue data
    uint16_t left;         // left child in min bin queue
    uint16_t right;        // right child in min bin queue
} vertex_t;

int load_map( char *file
            , vertex_t *v
            )
{
    int rc = -1;
    FILE *f = NULL;
    uint16_t i = 0;
    size_t read = 0;
    edge_t *e = NULL;

    f = fopen(file, "r");
    if(!f) goto cleanup;

    do {
        read = fread(&i, sizeof(i), 1, f);
        if(0 == read && feof(f)) rc = 0;
        if(1 != read) goto cleanup;

        e = malloc(sizeof(*e));
        if(!v[i].head) {
            v[i].head =  e;
            v[i].tail = v[i].head;
        }
        else {
            v[i].tail->next = e;
            v[i].tail = v[i].tail->next;
        }

        read = fread(&v[i].tail->dest, sizeof(v[i].tail->dest), 1, f);
        if(1 != read) goto cleanup;

        read = fread(&v[i].tail->cost, sizeof(v[i].tail->cost), 1, f);
        if(1 != read) goto cleanup;
#ifdef DEBUG
        fprintf( stderr
               , "%d->%d:%d\n"
               , i
               , v[i].tail->dest
               , v[i].tail->cost
               );
#endif
    } while(1);

cleanup:
    if(0 == read && feof(f)) rc = 0;
    if(f) fclose(f);
    return rc;
}

int main(void)
{
    int i = 0, rc = -1;
    size_t sz = 0;
    vertex_t *v = NULL;

    sz = sizeof(*v) * MAX_VERTICES;
    v = malloc(sz);
    memset(v, 0, sz);

    rc = load_map("../data/map.bin", v);
    if(0 != rc) goto cleanup;

    rc = 0;
cleanup:
    for(; i < MAX_VERTICES; ++i) {
        edge_t *e = v[i].head;
        edge_t *n = NULL;
        while(e != v[i].tail) {
            n = e->next;
            free(e);
            e = n;
        }
        free(v[i].tail);
    }
    free(v);
    return rc;
}

