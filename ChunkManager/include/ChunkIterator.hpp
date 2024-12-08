#ifndef CHUNKMANAGER_INCLUDE_CHUNKITERATOR
#define CHUNKMANAGER_INCLUDE_CHUNKITERATOR
#include <iostream>
#include <iterator>
#include <vector>

template <typename T> class MyIterator {
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using reference = T &;

  MyIterator(pointer ptr) : ptr(ptr) {}

  reference operator*() const { return *ptr; }
  pointer operator->() const { return ptr; }

  MyIterator &operator++() {
    ++ptr;
    return *this;
  }

  MyIterator operator++(int) {
    MyIterator temp = *this;
    ++(*this);
    return temp;
  }

  bool operator!=(const MyIterator &other) const { return ptr != other.ptr; }

  bool operator==(const MyIterator &other) const { return ptr == other.ptr; }

  MyIterator &operator--() {
    --ptr;
    return *this;
  }

  MyIterator operator--(int) {
    MyIterator temp = *this;
    --(*this);
    return temp;
  }

  MyIterator &operator+=(difference_type n) {
    ptr += n;
    return *this;
  }

  MyIterator &operator-=(difference_type n) {
    ptr -= n;
    return *this;
  }

  reference operator[](difference_type n) const { return *(ptr + n); }

  difference_type operator-(const MyIterator &other) const {
    return ptr - other.ptr;
  }

  bool operator<(const MyIterator &other) const { return ptr < other.ptr; }

  bool operator>(const MyIterator &other) const { return ptr > other.ptr; }

private:
  pointer ptr;
};

template <typename T> class MyContainer {
public:
  using iterator = MyIterator<T>;

  MyContainer(std::vector<T> &data) : data(data) {}

  iterator begin() { return iterator(&data[0]); }
  iterator end() { return iterator(&data[data.size()]); }

private:
  std::vector<T> &data;
};

int main() {
  std::vector<int> data = {1, 2, 3, 4, 5};
  MyContainer<int> container(data);

  for (auto it = container.begin(); it != container.end(); ++it) {
    std::cout << *it << " ";
  }
  std::cout << std::endl;

  return 0;
}

#endif /* CHUNKMANAGER_INCLUDE_CHUNKITERATOR */
