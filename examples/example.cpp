#include "looqueue/queue.hpp"

int main() {
  loo::queue<int> queue{};
  auto elem = 1;
  queue.enqueue(&elem);
  auto dequeue = queue.dequeue();

  if (dequeue == &elem) {
    return 0;
  } else {
    return 1;
  }
}
