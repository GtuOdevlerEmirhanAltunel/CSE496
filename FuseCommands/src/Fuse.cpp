#define FUSE_USE_VERSION 26

#include <Fuse.hpp>
#include <cstdlib>
#include <cstring>
#include <string>

const std::string file_name = "hello.txt";
const std::string file_content = "Hello, FUSE!\n";

int simple_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (strcmp(path + 1, file_name.c_str()) == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = file_content.size();
    stbuf->st_mtime = 1620000000;
  } else {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 0;
  }
  return 0;
}

#include <iostream>

int simple_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi) {
  (void)offset;
  (void)fi;

  if (strcmp(path, "/") != 0) {
    return -ENOENT;
  }

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  filler(buf, file_name.c_str(), NULL, 0);
  filler(buf, "test.txt", NULL, 0);
  return 0;
}

int simple_open(const char *path, struct fuse_file_info *fi) {
  (void)fi;

  if (strcmp(path + 1, file_name.c_str()) != 0) {
    return -ENOENT;
  }
  return 0;
}

int simple_read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
  (void)fi;

  if (strcmp(path + 1, file_name.c_str()) != 0) {
    return -ENOENT;
  }

  size_t len = file_content.size();
  if ((unsigned long)offset >= len) {
    return 0;
  }

  if (offset + size > len) {
    size = len - offset;
  }
  memcpy(buf, file_content.c_str() + offset, size);
  return size;
}

fuse_operations *get_fuse_operations() {
  static struct fuse_operations simple_operations = {
      .getattr = simple_getattr,
      .open = simple_open,
      .read = simple_read,
      .readdir = simple_readdir,
  };
  return &simple_operations;
}
