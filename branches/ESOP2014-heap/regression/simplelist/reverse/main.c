#include "../heap_builtins.h"

/* reverse */

/* expect bottom */

struct list {
  struct list* value;
  struct list* next;
};

typedef struct list* list_t;

void main() {
  list_t root, new_root;
  list_t next;

  new_root = NULL;

  while (root != NULL) {
    assert(root != NULL);
    next = root->next;
    assert(root != NULL);
    root->next = new_root;
    new_root = root;
    root = next;
  }

  assert(__CPROVER_HEAP_path(new_root, NULL, "next"));
}


