# Server-Client-in-C


In our program, we have two main components: libnetfiles and netfileserver. Our netclient.c file is what
the the libnetfiles methods. Libnetfiles covers the client side while the netfilesever covers the server
side operations. Once both client and server are running, the client attempts to make a connection with
the server, and the server would try to accept the connection if it receives it. The server would bind it
first and create a thread for every socket being created. After this occurs the client would be able to
apply the following actions: Netopen, Netclose, Netread, and Netwrite. With the following actions, there
would be a file associated with every Netopen etc. for every client where they would try to open the file
first then apply read/write etc. upon the file. All of the client’s information would be stored in a struct
where each element would be stored in a node of a linked list, being able to make accessing clients
information in O(n). This linked list would also be called our file descriptor table, where we store a file
descriptor along with client information. We also delete from the linked list once the user says netclose
on one of the clients that were instantiated along with the file. Also, we defined values for the file
modes and Net operations.

With filemodes being implemented. File mode system consisting of: Transaction,
Exclusive and Unrestricted modes where we associated numbers with each of the following modes to be
able to apply them to each file.

Partner: Josh Vilson
