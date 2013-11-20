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
#define VERT_IDX_MAX 65536 /* Valid indices: 1-65535; invalid index: 0 */

struct edge {
    uint16_t dest;       // The index of the destination
    uint16_t cost;       // The cost to follow the edge to dest
    struct edge *next;   // Next edge in list
};
typedef struct edge edge_t;

// A vertex id of 0 is invalid. Therefore, 0 is used as NULL or empty
typedef struct {
    // Edge data
    edge_t *head;   // list of all outbound edges
    edge_t *tail;   // tail pointer for edge list
    // Traversal metadata
    uint32_t dist;  // current shortest distance to vertex
    char visited;   // vertex has been visited or not
    uint16_t prev;  // last vertex in shortest path here
    uint16_t q_idx; // queue index; used by push/pop & heapify-{up,down}
} vertex_t;

/* Load the graph from a binary file
 *
 * Requires:
 *   - A binary directed graph file path
 *     - Each entry in the file is 6 bytes with no delimiters
 *       2 bytes: unsigned int [1-65535] (source vertex)
 *       2 bytes: unsigned int [1-65535] (sync vertex)
 *       2 bytes: unsigned int [1-65535] (edge cost from source to sync)
 *     - There are no delimiters between entries
 *     - The only data in the file are 6 byte entries
 *     - Read access on the file is granted
 *   - A reference to a vertex_t array of size VERT_IDX_MAX to store data
 *
 * Guarantees:
 *   - The contents of the file will be loaded into the vertex_t array
 *   - 0 will be returned on success
 *   - -1 will be returned on error
 */
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
        memset(e, 0, sizeof(*e));
        if(!v[i].head) {
            v[i].head = e;
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
        fprintf(stderr, "%d->%d:%d\n", i, v[i].tail->dest, v[i].tail->cost);
#endif
    } while(1);
cleanup:
    if(0 == read && feof(f)) rc = 0;
    if(f) fclose(f);
    return rc;
}

// Returns 1 if a < b (0 is treated as infinity); otherwise, returns 0
int lt( uint16_t a
      , uint16_t b
      )
{
    // a != infinity and ( a < b or b == infinity)
    if(a != 0 && (a < b || b == 0)) return 1;
    return 0;
}

// Returns 1 if a > b (0 is treated as infinity); otherwise, returns 0
int gt( uint16_t a
      , uint16_t b
      )
{
    // b != infinity and (a > b or a == infinity)
    if(b != 0 && (a > b || a == 0)) return 1;
    return 0;
}

/* Should be used to set the value of any position in the queue
 *
 * Requires:
 *   - A valid min bin heap (array) is provided
 *     - The value of the heap elements index into an array of vertices
 *   - An array of vertices is provided
 *   - The queue index to set
 *   - The vertex index to set the queue index to
 *
 * Guarantees:
 *   - The value at the queue index will be set to the vertex index
 *   - The value of the vertex's q_idx will be set to the queue index
 */
void q_set( vertex_t *v
          , uint16_t *q
          , uint16_t q_i
          , uint16_t v_i
          )
{
    q[q_i] = v_i;
    v[v_i].q_idx = q_i;
}

/* Should be used to clear the value of any position in the queue
 *
 * Requires:
 *   - A valid min bin heap (array) is provided
 *     - The value of the heap elements index into an array of vertices
 *   - An array of vertices is provided
 *   - The queue index to clear
 *
 * Guarantees:
 *   - The vertex referenced by the queue index will have it's q_idx cleared
 *   - The value at the queue index will cleared
 */
void q_clear( vertex_t *v
            , uint16_t *q
            , uint16_t q_i
            )
{
    v[q[q_i]].q_idx = 0;
    q[q_i] = 0;
}

// Swaps q[a] with q[b]
void swap( vertex_t *v
         , uint16_t *q
         , uint16_t a
         , uint16_t b
         )
{
    uint16_t s = 0;
    s = q[a];
    q_set(v, q, a, q[b]);
    q_set(v, q, b, s);
}

/* Heapify-up the element in the queue at the provided index
 *
 * Requires:
 *   - A valid min bin heap (array) is provided
 *     - The value of the heap elements index into an array of vertices
 *   - An array of vertices is provided with an accessible .dist member
 *     - 0 dist indicates infinity
 *   - The head of the heap starts at index 1
 *
 * Guarantees:
 *   - The value at the provided index will be heapify-up'd
 *   - The heap will still be a valid min bin heap
 *   - The new index for the value at the provided index will be returned
 */
int heapify_up( vertex_t *v
              , uint16_t *q
              , uint16_t i
              )
{
    int stop = 0;

    do { // heapify-up
        uint16_t p = i/2;
        stop = (i == 1) // reached top
            || !lt(v[q[i]].dist, v[q[p]].dist); // parent < child
        if(!stop) {
            swap(v, q, i, p);
            i = p;
        }
    } while(!stop);
    return i;
}

/* Heapify-down the element in the queue at the provided index
 *
 * Requires:
 *   - A valid min bin heap (array) is provided
 *     - The value of the heap elements index into an array of vertices
 *   - An array of vertices is provided with an accessible .dist member
 *     - 0 dist indicates infinity
 *   - The head of the heap starts at index 1
 *
 * Guarantees:
 *   - The value at the provided index will be heapify-down'd
 *   - The heap will still be a valid min bin heap
 *   - The new index for the value at the provided index will be returned
 */
int heapify_down( vertex_t *v
                , uint16_t *q
                , uint16_t i
                )
{
    int stop = 0;

    do { // heapify-down
        int c1 = 2*i;
        int c2 = 2*i+1;
        stop = (c1 > VERT_IDX_MAX || c2 > VERT_IDX_MAX) // reached bottom
            || !( gt(v[q[i]].dist, v[q[c1]].dist)
               || gt(v[q[i]].dist, v[q[c2]].dist)
                ); // parent < child
        if(!stop) {
            int s = lt(v[q[c1]].dist, v[q[c2]].dist) ? c1 : c2;
            swap(v, q, i, s); // swap child with shortest distance
            i = s;
        }
    } while(!stop);
    return i;
}

/* Push the provided index into the min bin heap
 *
 * Requires:
 *   - A valid min bin heap (array) is provided
 *     - The value of the heap elements index into an array of vertices
 *   - An array of vertices is provided with an accessible .dist member
 *     - 0 dist indicates infinity
 *   - The head of the heap starts at index 1
 *   - A reference to the tail index of the heap is provided
 *   - The index to insert is provided
 *
 * Guarantees:
 *   - The new index will be inserted into the min bin heap
 *   - The heap will still be a valid min bin heap
 *   - The tail index will be updated
 */
int push( vertex_t *v
        , uint16_t *q
        , uint16_t *tail
        , uint16_t new
        ) {
    uint16_t i = 0;

    // Add the element to the bottom level of the heap.
    q_set(v, q, ++(*tail), new);
    i = *tail;

    heapify_up(v, q, i);
}

/* Pop the top of the min bin heap
 *
 * Requires:
 *   - A valid min bin heap (array) is provided
 *     - The value of the heap elements index into an array of vertices
 *   - A vertices array is provided with an accessible .dist member
 *     - 0 dist indicates infinity
 *   - The head of the heap starts at index 1
 *   - The tail index of the heap is provided
 *   - The tail index will be updated
 *
 * Guarantees:
 *   - The index at the root of the heap will be removed
 *   - The heap will still be a valid min bin heap
 */
void pop( vertex_t *v
        , uint16_t *q
        , uint16_t *tail
        ) {
    uint16_t i = 1;

    // Replace the root of the heap with the last element on the last level
    swap(v, q, i, *tail);
    q_clear(v, q, *tail);
    --(*tail);

    heapify_down(v, q, i);
}

/* Performs Dijkstra's Algorithm on the provided vertices
 *
 * Requires:
 *   - An array of vertices is provided with an accessible .dist member
 *     - 0 dist indicates infinity
 *   - A start index into the array of vertices is provided
 *   - An end index into the array of vertices is provided
 *
 * Guarantees:
 *   - The shortest path if one exists is found
 *   - The distance from start to end is returned (0 for no path)
 *   - The vertices are updated with path and distance information
 */
int dijkstras( vertex_t *v
             , uint16_t start
             , uint16_t end
             )
{
    uint16_t q[VERT_IDX_MAX] = {0}; // q holds index into v; v[0] is unused
    uint16_t tail = 0;
    push(v, q, &tail, start);

    while(0 != q[1]) {
        uint16_t s = q[1];
        if(s == end) break;
        pop(v, q, &tail);
        v[s].visited = 1;
        edge_t *e = v[s].head;
        while(e) {
            uint32_t cur = v[e->dest].dist;
            uint32_t dist = v[s].dist + e->cost;
            // 0 distance represents infinity
            if(0 == v[e->dest].visited && (0 == cur || dist < cur)) {
                v[e->dest].dist = dist;
                v[e->dest].prev = s;
                if(0 == v[e->dest].q_idx) push(v, q, &tail, e->dest); // add
                else heapify_up(v, q, v[e->dest].q_idx); // update location
            }
            e = e->next;
        }
    }
    return v[end].dist;
}

int main(void)
{
    int i = 0, rc = -1;
    size_t sz = 0;
    vertex_t *v = NULL;

    sz = sizeof(*v) * VERT_IDX_MAX;
    v = malloc(sz);
    memset(v, 0, sz);

    rc = load_map("../data/map.bin", v);
    if(0 != rc) goto cleanup;
    printf("%d\n", dijkstras(v, 1, 5));

    rc = 0;
cleanup:
    for(; i < VERT_IDX_MAX; ++i) {
        edge_t *e = v[i].head;
        while(e) {
            edge_t* c = e;
            e = e->next;
            free(c);
        }
    }
    free(v);
    return rc;
}
