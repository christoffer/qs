#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "files.h"

String
read_entire_file(const char* filepath)
{
    assert(filepath);

    FILE* fp;
    fp = fopen(filepath, "r");
    if (!fp) {
        fprintf(stderr, "Warning: Failed to open file %s\n", filepath);
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    u32 filesize = ftell(fp);
    rewind(fp);

    String result = string_new();
    result = string_ensure_fits_len(result, filesize);
    u32 bytes_read = fread(result, sizeof(char), filesize, fp);
    fclose(fp);

    if (bytes_read != filesize) {
        fprintf(stderr, "Warning: Failed to read file %s\n", filepath);
        string_free(result);
        return 0;
    } else {
        set_string_len(result, filesize);
        result[filesize] = '\0';
    }
    return result;
}

bool is_readable_regfile(const char* path)
{
    struct stat file_stat;
    // NOTE(christoffer) stat should follow symlinks, which we want
    bool is_regfile = (stat(path, &file_stat) == 0 && S_ISREG(file_stat.st_mode));
    bool is_readable = access(path, F_OK) != -1;
    return is_regfile && is_readable;
}

bool is_readable_dir(const char* path)
{
    struct stat file_stat;
    bool is_dir = (stat(path, &file_stat) == 0 && S_ISDIR(file_stat.st_mode));
    bool is_readable = access(path, F_OK) != -1;
    return is_dir && is_readable;
}
