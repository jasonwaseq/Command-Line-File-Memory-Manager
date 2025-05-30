// Jason Waseq
// CSE 130
// asgn1
// memory.c

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define LINE_BUFFER_SIZE 4096
#define IO_BUFFER_SIZE   4096
#define ERR_INVALID      "Invalid Command\n"
#define ERR_OPERATION    "Operation Failed\n"

/*
 * read_line reads from STDIN_FILENO until a newline or EOF is encountered.
 * It stores up to max_len-1 characters into buf and then null-terminates it.
 * It trims trailing whitespace (spaces, tabs, carriage returns).
 * If require_newline is nonzero and no newline is encountered before EOF,
 * the function returns -2 to indicate the header line was not properly terminated.
 * Returns the length of the resulting string (not counting the '\0'),
 * or -1 if an error occurred (e.g. read failure), or -2 if a required newline is missing.
 */
ssize_t read_line(char *buf, size_t max_len, int require_newline) {
    size_t count = 0;
    int got_newline = 0;
    char c;
    while (count < max_len - 1) { // leave room for '\0'
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r < 0) {
            return -1; // error reading
        }
        if (r == 0) {
            break; // EOF reached
        }
        if (c == '\n') {
            got_newline = 1;
            break; // newline encountered
        }
        buf[count++] = c;
    }
    if (require_newline && !got_newline) {
        return -2; // header line was not terminated properly
    }
    // Trim trailing whitespace: spaces, tabs, carriage returns.
    while (
        count > 0 && (buf[count - 1] == ' ' || buf[count - 1] == '\t' || buf[count - 1] == '\r')) {
        count--;
    }
    buf[count] = '\0';
    return (ssize_t) count;
}

/*
 * full_write performs a write loop to ensure that exactly n bytes
 * are written to the given file descriptor from the provided buffer.
 * Returns 0 on success or -1 on error.
 */
int full_write(int fd, const void *buf, size_t n) {
    size_t total = 0;
    const char *p = buf;
    while (total < n) {
        ssize_t w = write(fd, p + total, n - total);
        if (w < 0) {
            return -1;
        }
        total += w;
    }
    return 0;
}

/*
 * handle_get processes a get command.
 * It reads the filename (location) from stdin (requiring a trailing newline)
 * and confirms that it refers to an existing regular file.
 * It then checks that any remaining input contains only whitespace.
 * On success, it writes the contents of the file to stdout.
 * On error, it writes an error message and exits with code 1.
 */
void handle_get(void) {
    char location[LINE_BUFFER_SIZE];
    ssize_t res = read_line(location, sizeof(location), 1);
    if (res < 0 || res == 0) {
        write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
        exit(1);
    }
    if (strlen(location) >= PATH_MAX) {
        write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
        exit(1);
    }

    // Check that location refers to an existing regular file.
    {
        struct stat st;
        if (stat(location, &st) < 0) {
            if (errno == ENOENT)
                write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
            else
                write(STDERR_FILENO, ERR_OPERATION, sizeof(ERR_OPERATION) - 1);
            exit(1);
        }
        if (!S_ISREG(st.st_mode)) {
            write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
            exit(1);
        }
    }

    // Consume any remaining input. If any non-whitespace character is found, it's an error.
    {
        char extra_buf[1024];
        ssize_t extra_read;
        int non_whitespace_found = 0;
        while ((extra_read = read(STDIN_FILENO, extra_buf, sizeof(extra_buf))) > 0) {
            for (ssize_t i = 0; i < extra_read; i++) {
                if (!isspace((unsigned char) extra_buf[i])) {
                    non_whitespace_found = 1;
                    break;
                }
            }
            if (non_whitespace_found)
                break;
        }
        if (non_whitespace_found) {
            write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
            exit(1);
        }
    }

    int fd = open(location, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT)
            write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
        else
            write(STDERR_FILENO, ERR_OPERATION, sizeof(ERR_OPERATION) - 1);
        exit(1);
    }

    char buffer[IO_BUFFER_SIZE];
    ssize_t n;
    while ((n = read(fd, buffer, IO_BUFFER_SIZE)) > 0) {
        if (full_write(STDOUT_FILENO, buffer, n) < 0) {
            write(STDERR_FILENO, ERR_OPERATION, sizeof(ERR_OPERATION) - 1);
            close(fd);
            exit(1);
        }
    }
    if (n < 0) {
        write(STDERR_FILENO, ERR_OPERATION, sizeof(ERR_OPERATION) - 1);
        close(fd);
        exit(1);
    }
    close(fd);
    exit(0);
}

/*
 * handle_set processes a set command.
 * It reads the filename (location) and the string representing content_length,
 * each of which must be terminated by a newline.
 * It then opens (or creates) the file and writes exactly the first content_length bytes
 * read from stdin into the file (if more are provided, they are ignored).
 * A missing newline in any header line (location or content_length) results in error.
 * On success, "OK\n" is written to stdout.
 */
void handle_set(void) {
    char location[LINE_BUFFER_SIZE];
    ssize_t res = read_line(location, sizeof(location), 1);
    if (res < 0 || res == 0) {
        write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
        exit(1);
    }
    if (strlen(location) >= PATH_MAX) {
        write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
        exit(1);
    }

    char length_str[LINE_BUFFER_SIZE];
    res = read_line(length_str, sizeof(length_str), 1);
    if (res < 0 || res == 0) {
        write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
        exit(1);
    }

    char *endptr;
    long content_length = strtol(length_str, &endptr, 10);
    if (*endptr != '\0' || content_length < 0) {
        write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
        exit(1);
    }

    int fd = open(location, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        write(STDERR_FILENO, ERR_OPERATION, sizeof(ERR_OPERATION) - 1);
        exit(1);
    }

    char buffer[IO_BUFFER_SIZE];
    long total_read = 0;
    while (total_read < content_length) {
        size_t to_read = IO_BUFFER_SIZE;
        if (content_length - total_read < (long) IO_BUFFER_SIZE) {
            to_read = content_length - total_read;
        }
        ssize_t r = read(STDIN_FILENO, buffer, to_read);
        if (r < 0) {
            write(STDERR_FILENO, ERR_OPERATION, sizeof(ERR_OPERATION) - 1);
            close(fd);
            exit(1);
        }
        if (r == 0) { // EOF reached before reading content_length bytes
            break;
        }
        if (full_write(fd, buffer, r) < 0) {
            write(STDERR_FILENO, ERR_OPERATION, sizeof(ERR_OPERATION) - 1);
            close(fd);
            exit(1);
        }
        total_read += r;
    }
    close(fd);

    if (full_write(STDOUT_FILENO, "OK\n", 3) < 0) {
        write(STDERR_FILENO, ERR_OPERATION, sizeof(ERR_OPERATION) - 1);
        exit(1);
    }
    exit(0);
}

int main(void) {
    char command[LINE_BUFFER_SIZE];
    ssize_t res = read_line(command, sizeof(command), 1);
    if (res < 0 || res == 0) {
        write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
        exit(1);
    }

    if (strcmp(command, "get") == 0) {
        handle_get();
    } else if (strcmp(command, "set") == 0) {
        handle_set();
    } else {
        write(STDERR_FILENO, ERR_INVALID, sizeof(ERR_INVALID) - 1);
        exit(1);
    }

    return 0;
}
