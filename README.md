dijkstra-server
===============

Reference solution to Dijkstra's algorithm running as a server

Requirements
------------
* The application will take a directed acyclic graph, a starting vertex, and a
  destination vertex and calculate the shortest path from the start to the
  destination
* The application will listen and accept connections on TCP 127.0.0.1:7777
* Upon establishing a connection with a client the application will read the
  starting vertex, destination vertex, and graph from the client file
  descriptor in the format specified under the Input section and write the
  shortest path and distance out over the client file descriptor in the format
  specified under the Output section

Input Format
------------
* The binary input data is split into two byte fields
* Each field is a sixteen bit unsigned integer in the set: {1,2,...,65535}
 * Zero is an invalid input; it can be assumed that no field will be set to 0
* There are no delimiters between the fields
* The first and second byte represent a starting vertex
* The third and forth byte represent a destination vertex
* The fifth and sixth byte represent the number of edges that follow
* Each edge is directed
* Each edge is split into three fields
 * The first field a vertex and predecessor to the next field
 * The second field a vertex and the successor to the previous field
 * The third field is the cost to travel from the predecessor to the successor
        # Decimal Representation of Input Data
        1  5   9 # start, destination, # edges that follow
        1  2  14
        1  3   9
        1  4   7
        2  5   9
        3  2   2
        3  6  11
        4  3  10
        4  6  15
        6  5   6

        # Hexadecimal Representation of Input Data
        0100 0500 0900
        0100 0200 0e00
        0100 0300 0900
        0100 0400 0700
        0200 0500 0900
        0300 0200 0200
        0300 0600 0b00
        0400 0300 0a00
        0400 0600 0f00
        0600 0500 0600
* The output will written to the client file descriptor in the following format
        start_vertex->vertex->destination_vertex (distance)


TODO
----
- [ ] Talk about endianess
- [ ] Benchmarks
- [ ] Test data
- [ ] Better instructions
- [ ] Languages
- [ ] Fix README formatting
