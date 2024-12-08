#include <Fuse.hpp>

int main(int argc, char *argv[]) {
  return fuse_main(argc, argv, get_fuse_operations(), NULL);
}
