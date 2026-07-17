/*
 *  STRAY ANIMAL RESCUE DISPATCH SYSTEM — Terminal Version
 *  Author: Sahil Kaushik
 *
 *  DSA: Singly Linked List | Stack (Linked List) | BST | malloc/free
 *  Compile: gcc main.c -o rescue
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME 50
#define MAX_DESC 150

/* ---- Data Structures ---- */

typedef struct {
    int  id;
    char location[MAX_NAME];
    char animalType[MAX_NAME];
    char description[MAX_DESC];
    int  urgencyScore;
} CallData;

/* Singly Linked List node — 'next' chains nodes sequentially */
typedef struct PendingNode {
    CallData            data;
    struct PendingNode *next;
} PendingNode;

/* Stack node (LIFO) — 'next' points to the node below */
typedef struct HistoryNode {
    CallData             data;
    struct HistoryNode  *next;
} HistoryNode;

/* BST node — 'left' < parent, 'right' >= parent (by animalType) */
typedef struct BSTNode {
    CallData         data;
    struct BSTNode  *left;
    struct BSTNode  *right;
} BSTNode;

/* ---- Global Pointers ---- */

PendingNode *pendingHead = NULL;
HistoryNode *historyTop  = NULL;
BSTNode     *searchRoot  = NULL;
int          nextId      = 1;

/* ---- Utility Functions ---- */

void safeReadLine(char *buffer, int size) {
    fgets(buffer, size, stdin);
    buffer[strcspn(buffer, "\n")] = '\0';
}

void printCallData(const CallData *c) {
    printf("  +---------------------------------------------+\n");
    printf("  | Call ID     : %-5d                         |\n", c->id);
    printf("  | Location    : %-30s|\n", c->location);
    printf("  | Animal Type : %-30s|\n", c->animalType);
    printf("  | Description : %-30s|\n", c->description);
    printf("  | Urgency     : %-2d / 10                       |\n", c->urgencyScore);
    printf("  +---------------------------------------------+\n");
}

void convertToLower(char *str) {
    int i;
    for (i = 0; str[i]; i++)
        if (str[i] >= 'A' && str[i] <= 'Z')
            str[i] += 32;
}

int caseInsensitiveCompare(const char *a, const char *b) {
    char copyA[MAX_NAME], copyB[MAX_NAME];
    strcpy(copyA, a); strcpy(copyB, b);
    convertToLower(copyA); convertToLower(copyB);
    return strcmp(copyA, copyB);
}

int caseInsensitiveContains(const char *haystack, const char *needle) {
    char lh[MAX_DESC], ln[MAX_NAME];
    strcpy(lh, haystack); strcpy(ln, needle);
    convertToLower(lh); convertToLower(ln);
    return (strstr(lh, ln) != NULL);
}

/* ============================================================
 *  SINGLY LINKED LIST — Pending Rescue Calls
 * ============================================================ */

void addPendingCall(CallData call) {
    PendingNode *newNode = (PendingNode *)malloc(sizeof(PendingNode));
    if (!newNode) { printf("  [ERROR] malloc failed!\n"); return; }

    newNode->data = call;
    newNode->next = NULL;

    if (pendingHead == NULL) {
        /* List empty — new node becomes head */
        pendingHead = newNode;
    } else {
        /* Traverse to last node, then link newNode after it */
        PendingNode *temp = pendingHead;
        while (temp->next != NULL)
            temp = temp->next;
        temp->next = newNode;
    }

    printf("\n  [OK] Call #%d logged. Urgency: %d/10\n", call.id, call.urgencyScore);
}

int countPendingCalls(void) {
    int count = 0;
    PendingNode *temp = pendingHead;
    while (temp) { count++; temp = temp->next; }
    return count;
}

void viewPendingCalls(void) {
    printf("\n  ============= PENDING RESCUE CALLS =============\n");
    if (!pendingHead) {
        printf("  (No pending calls.)\n");
        printf("  ================================================\n");
        return;
    }
    printf("  Total pending: %d call(s)\n\n", countPendingCalls());

    PendingNode *temp = pendingHead;
    int pos = 1;
    while (temp) {
        printf("  --- Pending Call #%d ---\n", pos);
        printCallData(&temp->data);
        printf("\n");
        temp = temp->next;
        pos++;
    }
    printf("  ================================================\n");
}

/* Forward declaration */
void pushToHistory(CallData call);

/*
 * dispatchHighestPriority — Core linked list algorithm.
 *
 * Uses two pointer pairs to traverse:
 *   (current, prev)   — the node being inspected and its predecessor.
 *   (maxNode, maxPrev) — the highest-urgency node and its predecessor.
 *
 * Deletion requires the PREVIOUS node's pointer:
 *   If maxNode is HEAD:   pendingHead = maxNode->next
 *   If maxNode is MIDDLE: maxPrev->next = maxNode->next
 *   (This re-links around maxNode, then we free() it.)
 */
void dispatchHighestPriority(void) {
    printf("\n  ============= DISPATCHING RESCUE =============\n");
    if (!pendingHead) {
        printf("  (No pending calls to dispatch.)\n");
        printf("  ==============================================\n");
        return;
    }

    /* Phase 1: Walk the list, track the node with max urgency */
    PendingNode *maxNode = pendingHead;
    PendingNode *maxPrev = NULL;
    PendingNode *current = pendingHead->next;
    PendingNode *prev    = pendingHead;

    while (current) {
        if (current->data.urgencyScore > maxNode->data.urgencyScore) {
            maxNode = current;
            maxPrev = prev;
        }
        prev    = current;
        current = current->next;
    }

    printf("  Dispatching the highest-priority call:\n");
    printCallData(&maxNode->data);

    /* Phase 2: Remove maxNode by re-linking around it
     *   [A] -> [maxNode] -> [C]   becomes   [A] -> [C]  */
    if (maxPrev == NULL)
        pendingHead = maxNode->next;       /* removing head */
    else
        maxPrev->next = maxNode->next;     /* removing middle/tail */

    /* Phase 3: Copy data to history stack, then free the node */
    pushToHistory(maxNode->data);
    free(maxNode);
    maxNode = NULL;

    printf("\n  [OK] Dispatched! Remaining: %d call(s)\n", countPendingCalls());
    printf("  ==============================================\n");
}

/* ============================================================
 *  STACK — Rescue History (LIFO)
 * ============================================================ */

/*
 * pushToHistory — inserts at the TOP of the stack.
 *   newNode->next = old top;  historyTop = newNode;
 *   This is O(1) — no traversal needed.
 */
void pushToHistory(CallData call) {
    HistoryNode *newNode = (HistoryNode *)malloc(sizeof(HistoryNode));
    if (!newNode) { printf("  [ERROR] malloc failed!\n"); return; }

    newNode->data = call;
    newNode->next = historyTop;   /* point to old top */
    historyTop = newNode;         /* new node IS the new top */
}

void viewHistory(void) {
    printf("\n  ========== DISPATCH HISTORY (LIFO) ==========\n");
    if (!historyTop) {
        printf("  (History stack is empty.)\n");
        printf("  ====================================================\n");
        return;
    }

    HistoryNode *temp = historyTop;
    int order = 1;
    while (temp) {
        printf("  --- Entry #%d (most recent first) ---\n", order);
        printCallData(&temp->data);
        printf("\n");
        temp = temp->next;
        order++;
    }
    printf("  ====================================================\n");
}

/* ============================================================
 *  BINARY SEARCH TREE — Search Index (keyed on animalType)
 * ============================================================ */

/*
 * bstInsert — recursive insertion.
 *   Base case: root == NULL → create node here.
 *   Recursive: compare animalType → go left (smaller) or right (>=).
 *   Returns the (possibly updated) subtree root.
 */
BSTNode *bstInsert(BSTNode *root, CallData call) {
    if (root == NULL) {
        BSTNode *n = (BSTNode *)malloc(sizeof(BSTNode));
        if (!n) { printf("  [ERROR] malloc failed!\n"); return NULL; }
        n->data = call;
        n->left = n->right = NULL;
        return n;
    }

    if (caseInsensitiveCompare(call.animalType, root->data.animalType) < 0)
        root->left  = bstInsert(root->left, call);
    else
        root->right = bstInsert(root->right, call);

    return root;
}

/*
 * bstSearchByAnimal — BST key search (O(log n) average).
 *   Uses BST property to prune: left if query < node, right if query > node.
 *   On match, checks BOTH subtrees (duplicates go right during insert).
 */
void bstSearchByAnimal(BSTNode *root, const char *type, int *count) {
    if (!root) return;
    int cmp = caseInsensitiveCompare(type, root->data.animalType);

    if (cmp < 0) {
        bstSearchByAnimal(root->left, type, count);
    } else if (cmp > 0) {
        bstSearchByAnimal(root->right, type, count);
    } else {
        (*count)++;
        printf("  [Match %d]\n", *count);
        printCallData(&root->data);
        printf("\n");
        bstSearchByAnimal(root->left, type, count);
        bstSearchByAnimal(root->right, type, count);
    }
}

/*
 * bstSearchByLocation — in-order traversal, O(n) full scan.
 *   BST is keyed on animalType, so we can't prune for location;
 *   must visit every node (LEFT → NODE → RIGHT).
 */
void bstSearchByLocation(BSTNode *root, const char *keyword, int *count) {
    if (!root) return;
    bstSearchByLocation(root->left, keyword, count);
    if (caseInsensitiveContains(root->data.location, keyword)) {
        (*count)++;
        printf("  [Match %d]\n", *count);
        printCallData(&root->data);
        printf("\n");
    }
    bstSearchByLocation(root->right, keyword, count);
}

/* ============================================================
 *  MEMORY CLEANUP — free all nodes before exit
 * ============================================================ */

void freeAllPending(void) {
    PendingNode *cur = pendingHead, *nxt;
    while (cur) { nxt = cur->next; free(cur); cur = nxt; }
    pendingHead = NULL;
}

void freeAllHistory(void) {
    HistoryNode *cur = historyTop, *nxt;
    while (cur) { nxt = cur->next; free(cur); cur = nxt; }
    historyTop = NULL;
}

/* Post-order traversal: free children before parent */
void freeAllBST(BSTNode *root) {
    if (!root) return;
    freeAllBST(root->left);
    freeAllBST(root->right);
    free(root);
}

void freeAllMemory(void) {
    freeAllPending();
    freeAllHistory();
    freeAllBST(searchRoot);
    searchRoot = NULL;
    printf("  [OK] All memory freed.\n");
}

/* ============================================================
 *  MENU ACTIONS
 * ============================================================ */

void menuLogCall(void) {
    CallData c;
    printf("\n  ============= LOG A NEW RESCUE CALL =============\n");

    c.id = nextId++;
    printf("  Enter location: ");          safeReadLine(c.location, MAX_NAME);
    printf("  Enter animal type: ");       safeReadLine(c.animalType, MAX_NAME);
    printf("  Brief description: ");       safeReadLine(c.description, MAX_DESC);
    printf("  Urgency (1-10): ");          scanf("%d", &c.urgencyScore);
    while (getchar() != '\n');

    if (c.urgencyScore < 1)  c.urgencyScore = 1;
    if (c.urgencyScore > 10) c.urgencyScore = 10;

    addPendingCall(c);
    searchRoot = bstInsert(searchRoot, c);
    printf("  ================================================\n");
}

void menuSearchCalls(void) {
    int choice, found = 0;
    printf("\n  ============= SEARCH RESCUE CALLS =============\n");
    if (!searchRoot) {
        printf("  (No calls logged yet.)\n");
        printf("  ================================================\n");
        return;
    }

    printf("  1. By Animal Type (BST key search)\n");
    printf("  2. By Location    (BST traversal)\n");
    printf("  Choice: ");
    scanf("%d", &choice);
    while (getchar() != '\n');

    if (choice == 1) {
        char q[MAX_NAME];
        printf("  Animal type: "); safeReadLine(q, MAX_NAME);
        printf("\n  Results for \"%s\":\n\n", q);
        bstSearchByAnimal(searchRoot, q, &found);
    } else if (choice == 2) {
        char q[MAX_NAME];
        printf("  Location keyword: "); safeReadLine(q, MAX_NAME);
        printf("\n  Results for \"%s\":\n\n", q);
        bstSearchByLocation(searchRoot, q, &found);
    } else {
        printf("  Invalid option.\n");
    }

    if (found == 0 && (choice == 1 || choice == 2))
        printf("  (No matches found.)\n");
    else if (found > 0)
        printf("  Total matches: %d\n", found);

    printf("  ================================================\n");
}

/* ============================================================
 *  MAIN — Menu loop (while + switch)
 * ============================================================ */

int main(void) {
    int choice;

    printf("\n");
    printf("  ****************************************************\n");
    printf("  *    STRAY ANIMAL RESCUE DISPATCH SYSTEM  v1.0     *\n");
    printf("  *    DSA: Linked List | Stack | BST | malloc/free  *\n");
    printf("  ****************************************************\n");

    while (1) {
        printf("\n  ==================== MAIN MENU ====================\n");
        printf("  |  1. Log a New Rescue Call                       |\n");
        printf("  |  2. View All Pending Calls                      |\n");
        printf("  |  3. Dispatch Highest Priority Call               |\n");
        printf("  |  4. Search Calls (by Animal / Location)         |\n");
        printf("  |  5. View Dispatch History                       |\n");
        printf("  |  6. Exit                                        |\n");
        printf("  ===================================================\n");
        printf("  Enter your choice (1-6): ");
        scanf("%d", &choice);
        while (getchar() != '\n');

        switch (choice) {
            case 1: menuLogCall();              break;
            case 2: viewPendingCalls();         break;
            case 3: dispatchHighestPriority();  break;
            case 4: menuSearchCalls();          break;
            case 5: viewHistory();              break;
            case 6:
                printf("\n  Shutting down...\n");
                freeAllMemory();
                printf("  Goodbye!\n\n");
                return 0;
            default:
                printf("\n  Invalid choice.\n");
                break;
        }
    }
    return 0;
}
