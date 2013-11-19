/* Input Data Generator for Dijkstra Server Reference Solution
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

int main(void)
{
    uint16_t buffer[] = { 1, 2, 14
                        , 1, 3, 9
                        , 1, 4, 7
                        , 2, 5, 9
                        , 3, 2, 2
                        , 3, 6, 11
                        , 4, 3, 10
                        , 4, 6, 15
                        , 6, 5, 6
                        };

    FILE *f = fopen("../data/map.bin", "w");
    int i = 0;
    for(; i < sizeof(buffer) / sizeof(*buffer); ++i) {
        fwrite(buffer+i, sizeof(*buffer), 1, f);
    }
    fclose(f);

    return 0;
}
