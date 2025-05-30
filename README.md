# Command-Line-File-Memory-Manager

## Project Description
This assignment tasked me with developing a standalone C utility, memory, that implements a simple “get/set” abstraction over files in a Linux directory. The program reads commands from standard input to either retrieve the full contents of an existing file or overwrite (or create) a file with a specified byte length, then cleanly exits with appropriate status codes. In building this tool, I employed low-level Linux system calls for file and directory handling, implemented buffered I/O to meet performance constraints, rigorously managed dynamic memory and file descriptors to avoid leaks, and parsed C-strings robustly to handle edge cases and invalid commands. This exercise reinforced core systems programming skills—particularly in memory management, error checking, and adherence to stringent coding standards and Makefile conventions 
.
