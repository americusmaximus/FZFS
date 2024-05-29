/*
Copyright (c) 2024 Americus Maximus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlwapi.h>

#ifdef _WIN64
typedef unsigned long long ADDR;
#else
typedef unsigned int ADDR;
#endif

#define FILE_SHARE_NONE 0

#include <zlib.h>

#define VERSION "0.0.1"

#define ARCHIVE_FILE_MAGIC 0x53465a46 /* FZFS */

typedef struct ArchiveHeader
{
    UINT Magic;
    UINT Offset;
} ARCHIVEHEADER, * ARCHIVEHEADERPTR;

typedef struct ArchiveSegment
{
    UINT Size;
    UINT ItemCount;
    UINT ItemSize;
} ARCHIVESEGMENT, * ARCHIVESEGMENTPTR;

typedef struct ArchiveFile
{
    UINT Name;
    UINT Type;
    UINT Index;
    UINT Size;
    UINT Chunk;
} ARCHIVEFILE, * ARCHIVEFILEPTR;

INT UnZip(LPVOID dst, UINT* dstlen, LPVOID src, UINT srclen)
{
    z_stream stream;
    ZeroMemory(&stream, sizeof(z_stream));

    stream.avail_in = srclen;
    stream.next_in = (BYTE*)src;

    stream.avail_out = *dstlen;
    stream.next_out = (BYTE*)dst;

    stream.zalloc = NULL;
    stream.zfree = NULL;

    INT result = inflateInit(&stream);

    if (result == Z_OK)
    {
        INT code = inflate(&stream, Z_FINISH);

        if (code == Z_STREAM_END)
        {
            *dstlen = stream.total_out;

            result = inflateEnd(&stream);
        }
        else
        {
            inflateEnd(&stream);

            result = Z_BUF_ERROR;

            if (code != Z_OK) { return code; }
        }
    }

    return result;
}

BOOL AcquireArchiveFileSegment(HANDLE handle, LPVOID* content, UINT* count)
{
    ARCHIVESEGMENT segment;
    ZeroMemory(&segment, sizeof(ARCHIVESEGMENT));

    if (!ReadFile(handle, &segment, sizeof(ARCHIVESEGMENT), NULL, NULL)) { return FALSE; }

    LPVOID src = malloc(segment.Size);

    if (src == NULL) { return FALSE; }

    UINT length = segment.ItemSize * segment.ItemCount;
    LPVOID dst = malloc(length);

    if (dst == NULL) { free(src); return FALSE; }

    if (!ReadFile(handle, src, segment.Size, NULL, NULL)) { free(dst); free(src); return FALSE; }

    if (UnZip(dst, &length, src, segment.Size) != Z_OK) { free(dst); free(src); return FALSE; }

    if (count != NULL) { *count = segment.ItemCount; }

    *content = dst;

    free(src);

    return TRUE;
}

BOOL UnpackArchiveFile(HANDLE archive, ARCHIVEFILEPTR file, HANDLE handle, UINT* offsets)
{
    UINT size = file->Size;
    UINT chunk = file->Index;
    UINT offset = 0;

    LPVOID content = malloc(file->Size);

    if (content == NULL) { return FALSE; }

    ZeroMemory(content, file->Size);

    while (offset != size)
    {
        if (size < offset) { free(content); return FALSE; }

        UINT length = offsets[chunk + 1] - offsets[chunk];

        LPVOID temp = malloc(length);

        if (temp == NULL) { free(content); return FALSE; }

        ZeroMemory(temp, length);

        if (SetFilePointer(archive, offsets[chunk], NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) { free(temp); free(content); return FALSE; }

        if (!ReadFile(archive, temp, length, NULL, NULL)) { free(temp); free(content); return FALSE; }

        UINT unpacked = file->Chunk;
        if (UnZip((LPVOID)((ADDR)content + (ADDR)offset), &unpacked, (LPVOID)temp, length) != Z_OK) { free(temp); free(content); return FALSE; }

        free(temp);

        chunk = chunk + 1;
        offset = offset + unpacked;
    }

    WriteFile(handle, content, file->Size, NULL, NULL);

    free(content);

    return TRUE;
}

BOOL CreateFolders(LPCSTR dir, LPCSTR file)
{
    LPSTR start = (LPSTR)file;

    LPSTR finish = strchr(start, '\\');

    while (finish != NULL)
    {
        CHAR path[MAX_PATH];
        ZeroMemory(path, MAX_PATH * sizeof(CHAR));

        CHAR folder[MAX_PATH];
        strncpy_s(folder, MAX_PATH, file, (ADDR)(finish - file));

        if (PathCombineA(path, dir, folder) == NULL) { printf("Unable to create full path for %s and %s\r\n", dir, folder); return FALSE; }

        if (!CreateDirectoryA(path, NULL))
        {
            if (GetLastError() != ERROR_ALREADY_EXISTS) { printf("Unable to create folder(s) for %s\r\n", path); return FALSE; }
        }

        start = (LPSTR)((ADDR)finish + (ADDR)sizeof(CHAR));

        finish = strchr(start, '\\');
    }

    return TRUE;
}

VOID UnpackArchiveFile(HANDLE handle, LPCSTR file, LPCSTR dir)
{
    ARCHIVEHEADER header;
    ZeroMemory(&header, sizeof(ARCHIVEHEADER));

    DWORD read = 0;
    if (!ReadFile(handle, &header, sizeof(ARCHIVEHEADER), &read, NULL) || read < sizeof(ARCHIVEHEADER))
    {
        printf("Unable to read file %s\r\n", file); return;
    }

    if (header.Magic != ARCHIVE_FILE_MAGIC)
    {
        printf("File %s is not a valid FZFS archive file.\r\n", file); return;
    }

    if (SetFilePointer(handle, header.Offset, NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
    {
        printf("Unable to read file %s, the file is invalid or truncated.\r\n", file); return;
    }

    UINT count = 0;
    ARCHIVEFILEPTR files = NULL;
    if (!AcquireArchiveFileSegment(handle, (LPVOID*)&files, &count))
    {
        printf("Unable to read file %s, the file is invalid or truncated.\r\n", file); return;
    }

    LPSTR names = NULL;
    if (!AcquireArchiveFileSegment(handle, (LPVOID*)&names, NULL))
    {
        printf("Unable to read file %s, the file is invalid or truncated.\r\n", file); free(files); return;
    }

    UINT* offsets = NULL;
    if (!AcquireArchiveFileSegment(handle, (LPVOID*)&offsets, NULL))
    {
        printf("Unable to read file %s, the file is invalid or truncated.\r\n", file); free(names); free(files); return;
    }

    ADDR offset = 0;
    for (UINT x = 0; x < count; x++)
    {
        LPSTR name = (CHAR*)(names + offset);

        if (files[x].Type != 8) { printf("Skipping unsupported file %s with type 0x%x.\r\n", name, files[x].Type); continue; }

        printf("Unpacking %s\r\n", name);
        offset = offset + strlen(name) + 1;

        if (strchr(name, '\\') != NULL)
        {
            if (!CreateFolders(dir, name)) { printf("Unable to create folder(s) for %s\r\n", name); break; }
        }

        CHAR path[MAX_PATH];
        ZeroMemory(path, MAX_PATH * sizeof(CHAR));

        if (PathCombineA(path, dir, name) == NULL) { printf("Unable to create full path for %s and %s\r\n", dir, name); break; }

        HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_NONE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (h == INVALID_HANDLE_VALUE) { printf("Unable to create %s\r\n", path); break; }

        if (!UnpackArchiveFile(handle, &files[x], h, offsets))
        {
            printf("Unable to unpack file %s\r\n", name); CloseHandle(h); break;
        }

        CloseHandle(h);
    }

    free(offsets);
    free(names);
    free(files);
}

INT main(INT argc, CHAR** argv)
{
    printf("FZFS Version %s\r\n", VERSION);

    if (argc < 2)
    {
        printf("Usage: fzfs.exe <file name> [<directory>]\r\n\r\n\t-- <directory> argument is optional, current working directory is used if not provided.\r\n");

        return ERROR_BAD_ARGUMENTS;
    }

    HANDLE handle = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (handle == INVALID_HANDLE_VALUE)
    {
        printf("Unable to open file %s\r\n", argv[1]);

        return ERROR_FILE_INVALID;
    }

    CHAR dir[MAX_PATH];
    ZeroMemory(dir, MAX_PATH * sizeof(CHAR));

    if (argc < 3) { GetCurrentDirectoryA(MAX_PATH, dir); }
    else { strcpy_s(dir, MAX_PATH * sizeof(CHAR), argv[2]); }

    {
        DWORD attributes = GetFileAttributesA(dir);

        if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            printf("Directory %s does not exists.\r\n", dir);

            CloseHandle(handle);

            return ERROR_DIRECTORY;
        }
    }

    if (GetFileSize(handle, NULL) <= sizeof(ARCHIVEHEADER))
    {
        printf("File %s is not a valid FZFS archive file.\r\n", argv[1]);

        CloseHandle(handle);

        return ERROR_FILE_INVALID;
    }

    UnpackArchiveFile(handle, argv[1], dir);

    CloseHandle(handle);

    return ERROR_SUCCESS;
}