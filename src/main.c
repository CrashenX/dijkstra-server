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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#define LISTEN_PORT 7777
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
 *   - A fd to read a binary directed graph from
 *     - The first 2 bytes should be the start vertex id
 *     - The second 2 bytes should be the stop vertex id
 *     - The third 2 bytes should be the number of edges that follow
 *     - Each edge is 6 bytes with no delimiters
 *       2 bytes: unsigned int [1-65535] (source vertex)
 *       2 bytes: unsigned int [1-65535] (sync vertex)
 *       2 bytes: unsigned int [1-65535] (edge cost from source to sync)
 *     - There are no delimiters between entries
 *     - Read access on the fd is granted
 *   - A reference to a vertex_t array of size VERT_IDX_MAX to store data
 *
 * Guarantees:
 *   - The contents of the file will be loaded into the vertex_t array
 *   - 0 will be returned on success
 *   - -1 will be returned on error
 */
int load_map( int fd
            , vertex_t *v
            , uint16_t *start
            , uint16_t *end
            )
{
    int rc = -1;
    uint16_t i = 0, n = 0;
    ssize_t r = 0;
    uint16_t num_vert = 0;
    edge_t *e = NULL;

    r = read(fd, start, sizeof(*start));
    if(sizeof(*start) != r) goto cleanup;
    r = read(fd, end, sizeof(*end));
    if(sizeof(*end) != r) goto cleanup;
    r = read(fd, &num_vert, sizeof(num_vert));
    if(sizeof(num_vert) != r) goto cleanup;
#ifdef DEBUG
    fprintf(stderr, "%d %d %d\n", *start, *end, num_vert);
#endif
    for(; n < num_vert; ++n) {
        r = read(fd, &i, sizeof(i));
        if(sizeof(i) != r) goto cleanup;
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
        r = read(fd, &v[i].tail->dest, sizeof(v[i].tail->dest));
        if(sizeof(v[i].tail->dest) != r) goto cleanup;
        r = read(fd, &v[i].tail->cost, sizeof(v[i].tail->cost));
        if(sizeof(v[i].tail->cost) != r) goto cleanup;
#ifdef DEBUG
        fprintf(stderr, "%d->%d:%d\n", i, v[i].tail->dest, v[i].tail->cost);
#endif
    }
    rc = 0;
cleanup:
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

/* Get's the path from start to end for the listed vertices, if any
 *
 * Requires:
 *   - An array of vertices is provided with an accessible .dist member
 *     - 0 dist indicates infinity
 *   - The array of vertices has been processed via dijkstras()
 *   - A start index into the array of vertices is provided
 *   - An end index into the array of vertices is provided
 *
 * Guarantess:
 *   - A string containing the path & distance will be returned if a path exists
 *   - NULL will be returned if no path exists
 */
char * gen_path( vertex_t *v
               , uint16_t start
               , uint16_t end
               )
{
    assert(VERT_IDX_MAX <= 65536);
    // We will pass through each vertex at most once (65536 - 1)
    // We will need at most 7 chars per vertices ('65535->')
    // Add an extra '\0' terminator for each vertex
    // This is a wasteful allocation, but we're talking 512K
    char *path = NULL, *prepend = NULL;
    size_t sz = VERT_IDX_MAX * sizeof(*path) * 8;
    uint16_t i = end;
    int h = 0, t = 0;
    if(0 == v[end].prev) return NULL;
    path = malloc(sz);
    memset(path, 0, sz);
    prepend = path + sz;
    do { // write string from back to front
        prepend -= 8;
        sprintf(prepend, "%d->", i);
        if(start == i) break;
        i = v[i].prev;
    } while(0 != i);
    if(start != i) {
        free(path);
        return NULL;
    }
    while(t < sz && h < sz) { // remove '\0' padding in string
        while(h < sz && 0 != path[h]) ++h;
        if(t < h) t = h+1;
        while(t < sz && 0 == path[t]) ++t;
        if(t > h && t < sz) {
            path[h] = path[t];
            path[t] = 0;
        }
    }
    sprintf(path+h-2, " (%d)\n", v[end].dist); // rm trailing '->'; add distance
    return path;
}

/* Reads a shortest path problem from the fd, solves it, & returns the solution
 *
 * Requires:
 *   - A valid client fd to read from
 *   - All messages sent over fd comply with the load_map contract
 *
 * Guarantees:
 *   - A string containing the shortest path and distance, if one exists
 *   - NULL if no path exists
 */
char * shortest_path(int fd)
{
    int i = 0, rc = -1;
    size_t sz = 0;
    vertex_t *v = NULL;
    char *path = NULL;
    uint32_t dist = 0;
    uint16_t start = 0, end = 0;

    sz = sizeof(*v) * VERT_IDX_MAX;
    v = malloc(sz);
    memset(v, 0, sz);

    rc = load_map(fd, v, &start, &end);
    if(0 != rc) goto cleanup;

    dist = dijkstras(v, start, end);
    path = gen_path(v, start, end);
    if(NULL == path) asprintf( &path
                             , "No path from '%d' to '%d'\n"
                             , start
                             , end
                             );

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
    return path;
}

int main()
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    int cli_fd = 0;
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(struct sockaddr_in);
    struct sockaddr_in sa;

    if(-1 == fd) {
        fprintf(stderr, "Socket Error: %s\n", strerror(errno));
        return 1;
    }
    memset(&sa, 0, sizeof(struct sockaddr_in));
    memset(&cli_addr, 0, sizeof(struct sockaddr_in));
    sa.sin_port = htons(LISTEN_PORT);
    sa.sin_addr = (struct in_addr) {0};
    if(-1 == bind(fd, (struct sockaddr*)&sa, sizeof(struct sockaddr_in))) {
        fprintf(stderr, "Bind Error: %s\n", strerror(errno));
        return 1;
    }
    if(-1 == listen(fd, 5)) {
        fprintf(stderr, "Listen Error: %s\n", strerror(errno));
        return 1;
    }
    while(-1 != (cli_fd = accept(fd, (struct sockaddr*)&cli_addr, &cli_len)))
    {
        char *path = shortest_path(cli_fd);
        if(!path) {
            fprintf(stderr, "Shortest path error\n");
            return -1;
        }
        write(cli_fd, path, strlen(path)+1);
        free(path);
        close(cli_fd);
    }
    return 0;
}
