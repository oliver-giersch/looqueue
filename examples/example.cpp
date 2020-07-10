#include "looqueue/queue.hpp"

int main() {
  loo::queue<unsigned > queue{};
  auto elem = 0xDEADBEEF;
  queue.enqueue(&elem);
  auto dequeue = queue.dequeue();

  if (dequeue == &elem) {
    return 0;
  } else {
    return 1;
  }
}
