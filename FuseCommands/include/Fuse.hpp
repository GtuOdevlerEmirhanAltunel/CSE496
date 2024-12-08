#ifndef FUSECOMMANDS_INCLUDE_MAIN
#define FUSECOMMANDS_INCLUDE_MAIN

#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <fuse.h>

int simple_getattr(const char *path, struct stat *stbuf);
int simple_open(const char *path, struct fuse_file_info *fi);
int simple_read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi);
int simple_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi);

fuse_operations *get_fuse_operations() {
  static struct fuse_operations simple_operations = {
      .getattr = simple_getattr,
      .open = simple_open,
      .read = simple_read,
      .readdir = simple_readdir,
  };
  return &simple_operations;
}

#endif /* FUSECOMMANDS_INCLUDE_MAIN */
