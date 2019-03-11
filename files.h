#pragma once

#include "base.h"
#include "string.h"

/* Reads an entire file into memory and stores the result in a given StringBuffer.
 *
 * Returns a String with the content if successful, 0 otherwise.
 */
String read_entire_file(const char* filepath);

/* Returns true if the given path is a readable, regular file. Symlinks are resolved. */
bool is_readable_regfile(const char* filepath);

/* Returns true if the given path is a readable directory. Symlinks are resolved. */
bool is_readable_dir(const char* filepath);
