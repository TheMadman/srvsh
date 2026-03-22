# Server Shell - srvsh

Server Shell - `srvsh` - is a program that interprets a shell script, using a special syntax, to connect programs together. It also provides a library for writing `srvsh` commands, which are totally stand-alone applications.

For a file called `script.srv` with the following content:

```
ls -l
```

It starts the program `ls` with the argument `-l`.

Modifying the script like so:

```
ls -l
cat script.srv
```

Begins both programs _concurrently._ Server shell does not wait for the first program to finish before starting the second program.

## Server/Client Programs

If all that `srvsh` did was start programs, it wouldn't be very interesting. The interesting feature is that it also sets up IPC connections between programs, based on a special syntax.

POSIX shells provide syntax to create pipelines of commands, where the standard input from the command line (or a file) is fed into the first command, and it produces standard output. Its standard output is fed into the standard input of the second command, which produces standard output, and so on:

```bash
cat foo.txt bar.txt | grep "Hello" | wc
```

The programs do not need to be written to have knowledge of where their standard input is coming from, or where their standard output is going to. They just have to read from file descriptor 0, set up by the shell for them, and write to file descriptor 1, also set up by the shell.

`srvsh` does not support pipelines, or redirecting standard input/standard output. What it does instead, is open new file descriptors to map a parent/child or server/client relationship, between a single parent/server and multiple child/client processes, with a syntax that looks like this:

```
server {
    client-1
    client-2
    # etc.
}
```

In this case, every process has a file descriptor 3, which is used to both read from and write to the server process. `server`'s server process is the shell itself, which currently doesn't read or write any data at all. `client-1` and `client-2` both have a file descriptor 3, which allows them to read and write data to `server`.

The server processes have file descriptors open for each child. In this case, `server` has a file descriptor 4 for `client-1`, and a file descriptor 5 for `client-2`, where it can read and write data to each of the clients.

A client library is also provided to conveniently work with the connections established by the shell. A server can discover how many clients it has by calling the library function `cli_count()`.

The shell supports arbitrarily nesting these commands:

```
server {
    middleware-1 {
        client-1
        client-2
    }
    client-3
}
```

The library provides the interface defined in [srvsh.h](src/srvsh/srvsh.h).

## Using libsrvsh To Write Programs

The `libsrvsh` library provides interfaces for conveniently writing and polling clients and servers. These are not mandatory; the raw interface will be documented later.

The interface that `libsrvsh` provides is based around a very primitive binary protocol header, containing an integer "opcode" and an integer length. It also provides an interface for loading an "opcode database", allowing you to use meaningful string names for opcodes. The opcode database is a plain-text file containing records on newlines, with a name and a value separated by whitespace:

```
# Lines beginning with a # are comment lines and are ignored
my_message_name   1

# the value is optional; if it is missing, it is the previous
# value incremented.
my_other_message
another_name
# etc.
```

The path to this file can be set with the environment variable `OPCODE_DATABASE`, which is the recommended way to set the opcode database to open. Using the environment variable allows you to set a single path that all applications in your script will use. Writing applications which use the opcode database is simple:

```c
#include <srvsh.h>

int my_message_name = -1;

int main()
{
    opcode_db *db = open_opcode_db();
    if (!db)
        return 1;

    my_message_name = get_opcode(db, "my_message_name");
    if (my_message_name < 0)
        return 1;

    // close the database when finished loading
    close_opcode_db(db);

    // use my_message_name variable in writeop*/sendop*/pollop* calls

    return 0;
}
```

The `open_opcode_db_at(const char *path)` function is also provided, which may be useful for programs that speak multiple protocols (for example, one for clients, and another for the server).

The path provided in `OPCODE_DATABASE` and as a parameter to `open_opcode_db_at(const char *path)` will be checked for a directory ending with `.d` as well, and will load files from that directory in file-name order. This makes it possible to extend the database by installing new files. If you create a directory structure like the following:
```
/etc/my-system/opcodes
/etc/my-system/opcodes.d/00-my-module
/etc/my-system/opcodes.d/10-third-party-module
```

Then setting `OPCODE_DATABASE=/etc/my-system/opcodes` will load those files in that order.
