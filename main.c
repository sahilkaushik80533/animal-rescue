/*
 *  STRAY ANIMAL RESCUE DISPATCH SYSTEM — Terminal Version (Expanded)
 *  Author: Sahil Kaushik
 *
 *  DSA Concepts Used:
 *    - Singly Linked List  (pending rescue calls queue)
 *    - Stack / LIFO        (dispatch history)
 *    - Binary Search Tree  (search index by animal type)
 *    - malloc / free       (manual dynamic memory management)
 *    - File Handling       (fopen, fprintf, fscanf — save/load)
 *    - Structs & Typedefs  (CallData, PendingNode, HistoryNode, BSTNode)
 *
 *  Compile:  gcc main.c -o rescue
 *  Run:      ./rescue
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rescue_dsa.h"

/* ================================================================
 *  GLOBAL POINTERS
 *  These are module-level variables that the whole program shares.
 * ================================================================ */

PendingNode *pendingHead = NULL;   /* Head of the pending-calls linked list */
HistoryNode *historyTop  = NULL;   /* Top of the dispatch-history stack     */
BSTNode     *searchRoot  = NULL;   /* Root of the BST search index         */
int          nextId      = 1;      /* Auto-increment ID counter            */

/* ================================================================
 *  UTILITY FUNCTIONS
 *  Small helper functions used throughout the program.
 * ================================================================ */

/*
 * safeReadLine — reads one line from stdin into buffer[].
 * Uses fgets (safe — prevents buffer overflow), then strips
 * the trailing newline character that fgets leaves behind.
 */
void safeReadLine(char *buffer, int size) {
    fgets(buffer, size, stdin);
    /* strcspn returns the index of the first '\n', we set it to '\0' */
    buffer[strcspn(buffer, "\n")] = '\0';
}

/*
 * convertToLower — converts a string to lowercase in-place.
 * Loops through each character; if it is between 'A' and 'Z',
 * we add 32 to convert it to the lowercase ASCII equivalent.
 */
void convertToLower(char *str) {
    int i;
    for (i = 0; str[i]; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = str[i] + 32;  /* 'A' is 65, 'a' is 97, diff = 32 */
        }
    }
}

/*
 * caseInsensitiveCompare — compares two strings ignoring case.
 * Makes lowercase copies of both strings, then uses strcmp().
 * Returns: <0 if a<b, 0 if equal, >0 if a>b.
 */
int caseInsensitiveCompare(const char *a, const char *b) {
    char copyA[MAX_NAME], copyB[MAX_NAME];
    strcpy(copyA, a);
    strcpy(copyB, b);
    convertToLower(copyA);
    convertToLower(copyB);
    return strcmp(copyA, copyB);
}

/*
 * caseInsensitiveContains — checks if 'needle' is found inside 'haystack'.
 * Both are lowered first so the search is case-insensitive.
 * Returns: 1 if found, 0 if not found.
 */
int caseInsensitiveContains(const char *haystack, const char *needle) {
    char lowerHay[MAX_DESC], lowerNee[MAX_NAME];
    strcpy(lowerHay, haystack);
    strcpy(lowerNee, needle);
    convertToLower(lowerHay);
    convertToLower(lowerNee);
    return (strstr(lowerHay, lowerNee) != NULL);
}

/*
 * printCallData — displays one CallData record inside a formatted box.
 * Uses printf with %-Xs left-justified padding for alignment.
 */
void printCallData(const CallData *c) {
    printf("  +---------------------------------------------+\n");
    printf("  | Call ID     : %-5d                         |\n", c->id);
    printf("  | Location    : %-30s|\n", c->location);
    printf("  | Animal Type : %-30s|\n", c->animalType);
    printf("  | Description : %-30s|\n", c->description);
    printf("  | Urgency     : %-2d / 10                       |\n", c->urgencyScore);
    printf("  +---------------------------------------------+\n");
}

/*
 * printBanner — prints a decorative title banner.
 * Used at the start of the program to show the project name.
 */
void printBanner(void) {
    printf("\n");
    printf("  ****************************************************\n");
    printf("  *                                                  *\n");
    printf("  *    STRAY ANIMAL RESCUE DISPATCH SYSTEM  v2.0     *\n");
    printf("  *                                                  *\n");
    printf("  *    DSA: Linked List | Stack | BST | File I/O     *\n");
    printf("  *    Author: Sahil Kaushik                         *\n");
    printf("  *                                                  *\n");
    printf("  ****************************************************\n");
    printf("\n");
}

/*
 * printSeparator — prints a line separator for visual clarity.
 */
void printSeparator(void) {
    printf("  ===================================================\n");
}

/* ================================================================
 *  INPUT VALIDATION FUNCTIONS
 *  These functions check user input for correctness before
 *  we accept it. Each one explains what went wrong if invalid.
 * ================================================================ */

/*
 * validateNonEmpty — checks that a string is not empty.
 * Returns: 1 if valid (non-empty), 0 if invalid (empty).
 */
int validateNonEmpty(const char *str, const char *fieldName) {
    if (strlen(str) == 0) {
        printf("  [VALIDATION ERROR] %s cannot be empty!\n", fieldName);
        return 0;  /* invalid */
    }
    return 1;  /* valid */
}

/*
 * validateMaxLength — checks that a string does not exceed maxLen.
 * Returns: 1 if valid, 0 if too long.
 */
int validateMaxLength(const char *str, int maxLen, const char *fieldName) {
    int length = strlen(str);
    if (length > maxLen - 1) {  /* -1 because we need space for '\0' */
        printf("  [VALIDATION ERROR] %s is too long! Max %d characters.\n",
               fieldName, maxLen - 1);
        return 0;  /* invalid */
    }
    return 1;  /* valid */
}

/*
 * validateNoSpecialChars — checks that a string contains only
 * letters, digits, spaces, commas, periods, and hyphens.
 * This prevents pipe characters '|' from breaking our file format.
 * Returns: 1 if valid, 0 if invalid.
 */
int validateNoSpecialChars(const char *str, const char *fieldName) {
    int i;
    for (i = 0; str[i] != '\0'; i++) {
        char ch = str[i];
        /* Allow letters, digits, spaces, commas, periods, hyphens */
        if (!((ch >= 'A' && ch <= 'Z') ||
              (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') ||
              ch == ' ' || ch == ',' || ch == '.' || ch == '-')) {
            printf("  [VALIDATION ERROR] %s contains invalid character '%c'.\n",
                   fieldName, ch);
            printf("  Only letters, digits, spaces, commas, periods, hyphens allowed.\n");
            return 0;  /* invalid */
        }
    }
    return 1;  /* valid */
}

/*
 * validateUrgency — checks that the urgency score is between 1 and 10.
 * Returns: 1 if valid, 0 if out of range.
 */
int validateUrgency(int score) {
    if (score < 1 || score > 10) {
        printf("  [VALIDATION ERROR] Urgency must be between 1 and 10.\n");
        return 0;  /* invalid */
    }
    return 1;  /* valid */
}

/*
 * validateTextField — runs all text validations on a single field.
 * Combines: non-empty check + max length check + special char check.
 * Returns: 1 if all pass, 0 if any fail.
 */
int validateTextField(const char *str, int maxLen, const char *fieldName) {
    /* Step 1: Check non-empty */
    if (!validateNonEmpty(str, fieldName)) {
        return 0;
    }
    /* Step 2: Check max length */
    if (!validateMaxLength(str, maxLen, fieldName)) {
        return 0;
    }
    /* Step 3: Check for special characters */
    if (!validateNoSpecialChars(str, fieldName)) {
        return 0;
    }
    /* All checks passed */
    printf("  [OK] %s accepted: \"%s\"\n", fieldName, str);
    return 1;
}

/* ================================================================
 *  SINGLY LINKED LIST — Pending Rescue Calls
 *  Each node holds one CallData struct and a 'next' pointer.
 *  New calls are appended to the END of the list (tail insertion).
 * ================================================================ */

/*
 * addPendingCall — creates a new node with malloc, copies the call data,
 * and appends it to the end of the linked list.
 *
 * If list is empty:  pendingHead = newNode
 * If list has nodes:  traverse to the last node, set last->next = newNode
 */
void addPendingCall(CallData call) {
    /* Step 1: Allocate memory for a new node */
    PendingNode *newNode = (PendingNode *)malloc(sizeof(PendingNode));
    if (!newNode) {
        printf("  [ERROR] malloc failed! Cannot allocate memory.\n");
        return;
    }

    /* Step 2: Copy the call data into the new node */
    newNode->data = call;
    newNode->next = NULL;  /* New node will be the last, so next = NULL */

    /* Step 3: Insert into the list */
    if (pendingHead == NULL) {
        /* List is empty — this new node becomes the head */
        pendingHead = newNode;
        printf("  [STATUS] First call added to the pending list.\n");
    } else {
        /* List is not empty — traverse to find the last node */
        PendingNode *temp = pendingHead;
        while (temp->next != NULL) {
            temp = temp->next;  /* Move to the next node */
        }
        /* temp now points to the last node — link newNode after it */
        temp->next = newNode;
        printf("  [STATUS] Call appended to end of pending list.\n");
    }

    /* Step 4: Confirm to the user */
    printf("  [OK] Call #%d logged successfully. Urgency: %d/10\n",
           call.id, call.urgencyScore);
}

/*
 * countPendingCalls — counts how many nodes are in the linked list.
 * Traverses from head to NULL, incrementing a counter.
 * Returns: the number of pending calls.
 */
int countPendingCalls(void) {
    int count = 0;
    PendingNode *temp = pendingHead;
    while (temp != NULL) {
        count++;
        temp = temp->next;
    }
    return count;
}

/*
 * viewPendingCalls — displays all pending rescue calls.
 * Traverses the linked list from head to tail, printing each node.
 */
void viewPendingCalls(void) {
    printf("\n");
    printSeparator();
    printf("  ============= PENDING RESCUE CALLS =============\n");
    printSeparator();

    if (pendingHead == NULL) {
        printf("  (No pending calls at this time.)\n");
        printf("  Tip: Use option 1 to log a new rescue call.\n");
        printSeparator();
        return;
    }

    /* Count and display total */
    int total = countPendingCalls();
    printf("  Total pending calls: %d\n\n", total);

    /* Traverse and print each node */
    PendingNode *temp = pendingHead;
    int position = 1;
    while (temp != NULL) {
        printf("  --- Pending Call [Position %d of %d] ---\n", position, total);
        printCallData(&temp->data);
        printf("\n");
        temp = temp->next;
        position++;
    }

    printSeparator();
}

/*
 * findCallById — searches the pending list for a call with a specific ID.
 * Uses a simple while loop to traverse node by node.
 * Returns: pointer to the CallData if found, NULL if not found.
 */
CallData *findCallById(int targetId) {
    PendingNode *temp = pendingHead;
    while (temp != NULL) {
        if (temp->data.id == targetId) {
            return &temp->data;  /* Found it — return pointer to data */
        }
        temp = temp->next;
    }
    return NULL;  /* Not found */
}

/*
 * removeCallById — removes a specific call from the pending list by ID.
 * Uses two pointers: 'prev' and 'current' to re-link around the target.
 * Returns: 1 if removed, 0 if not found.
 */
int removeCallById(int targetId) {
    PendingNode *current = pendingHead;
    PendingNode *prev = NULL;

    while (current != NULL) {
        if (current->data.id == targetId) {
            /* Found the node to remove */
            if (prev == NULL) {
                /* Removing the head node */
                pendingHead = current->next;
            } else {
                /* Removing a middle or tail node */
                prev->next = current->next;
            }
            free(current);  /* Release the memory */
            printf("  [OK] Call #%d removed from pending list.\n", targetId);
            return 1;  /* Success */
        }
        prev = current;
        current = current->next;
    }

    printf("  [NOT FOUND] Call #%d is not in the pending list.\n", targetId);
    return 0;  /* Not found */
}

/* Forward declaration so we can call this before it is defined */
void pushToHistory(CallData call);

/*
 * dispatchHighestPriority — Core linked-list algorithm.
 *
 * ALGORITHM: Selection-based extraction
 *   1. Walk the entire list, tracking the node with the maximum urgency.
 *   2. Re-link around that node to remove it from the list.
 *   3. Push the removed call's data onto the history stack.
 *   4. Free the memory of the removed node.
 *
 * We use two pointer PAIRS:
 *   (current, prev)    — the node we are inspecting and its predecessor
 *   (maxNode, maxPrev) — the best candidate and its predecessor
 *
 * Deletion needs the PREVIOUS pointer because:
 *   - If maxNode is HEAD:    pendingHead = maxNode->next
 *   - If maxNode is MIDDLE:  maxPrev->next = maxNode->next
 *   This "re-links around" maxNode, then we free() it.
 */
void dispatchHighestPriority(void) {
    printf("\n");
    printSeparator();
    printf("  ============= DISPATCHING RESCUE =============\n");
    printSeparator();

    /* Check if there are any pending calls */
    if (pendingHead == NULL) {
        printf("  (No pending calls to dispatch.)\n");
        printf("  Tip: Log some calls first using option 1.\n");
        printSeparator();
        return;
    }

    printf("  [STATUS] Scanning pending list for highest priority...\n");

    /* Phase 1: Walk the list, track the node with maximum urgency */
    PendingNode *maxNode = pendingHead;
    PendingNode *maxPrev = NULL;
    PendingNode *current = pendingHead->next;
    PendingNode *prev    = pendingHead;

    int nodesChecked = 1;  /* We already set maxNode to head */

    while (current != NULL) {
        nodesChecked++;
        if (current->data.urgencyScore > maxNode->data.urgencyScore) {
            maxNode = current;
            maxPrev = prev;
        }
        prev    = current;
        current = current->next;
    }

    printf("  [STATUS] Checked %d node(s). Highest urgency: %d/10\n",
           nodesChecked, maxNode->data.urgencyScore);
    printf("  [STATUS] Dispatching the following call:\n\n");
    printCallData(&maxNode->data);

    /* Phase 2: Remove maxNode by re-linking around it
     *   [A] -> [maxNode] -> [C]   becomes   [A] -> [C]
     */
    if (maxPrev == NULL) {
        /* maxNode is the HEAD — update the head pointer */
        pendingHead = maxNode->next;
        printf("\n  [STATUS] Removed head node from list.\n");
    } else {
        /* maxNode is in the MIDDLE or TAIL — re-link previous to skip it */
        maxPrev->next = maxNode->next;
        printf("\n  [STATUS] Re-linked list around dispatched node.\n");
    }

    /* Phase 3: Save to history stack, then free the node's memory */
    pushToHistory(maxNode->data);
    free(maxNode);
    maxNode = NULL;  /* Avoid dangling pointer */

    int remaining = countPendingCalls();
    printf("  [OK] Dispatch complete! Remaining pending: %d call(s)\n", remaining);
    printSeparator();
}

/* ================================================================
 *  STACK — Rescue Dispatch History (LIFO — Last In, First Out)
 *  The most recently dispatched call is always on top.
 * ================================================================ */

/*
 * pushToHistory — pushes a call onto the TOP of the history stack.
 * This is an O(1) operation — we just insert at the head.
 *   newNode->next = old top;
 *   historyTop = newNode;
 */
void pushToHistory(CallData call) {
    /* Allocate a new stack node */
    HistoryNode *newNode = (HistoryNode *)malloc(sizeof(HistoryNode));
    if (!newNode) {
        printf("  [ERROR] malloc failed! Cannot push to history.\n");
        return;
    }

    /* Copy the call data and link to old top */
    newNode->data = call;
    newNode->next = historyTop;   /* Point to the old top of stack */
    historyTop = newNode;         /* This new node IS the new top  */

    printf("  [HISTORY] Call #%d pushed to history stack.\n", call.id);
}

/*
 * countHistory — counts how many entries are on the history stack.
 * Traverses from top to bottom, incrementing a counter.
 */
int countHistory(void) {
    int count = 0;
    HistoryNode *temp = historyTop;
    while (temp != NULL) {
        count++;
        temp = temp->next;
    }
    return count;
}

/*
 * viewHistory — displays the dispatch history from top to bottom.
 * Since it's a stack, the most recently dispatched call appears first.
 */
void viewHistory(void) {
    printf("\n");
    printSeparator();
    printf("  ========== DISPATCH HISTORY (LIFO Stack) ==========\n");
    printSeparator();

    if (historyTop == NULL) {
        printf("  (History stack is empty — no dispatches yet.)\n");
        printf("  Tip: Dispatch a call using option 3.\n");
        printSeparator();
        return;
    }

    int total = countHistory();
    printf("  Total dispatched calls in history: %d\n\n", total);

    HistoryNode *temp = historyTop;
    int order = 1;
    while (temp != NULL) {
        printf("  --- History Entry #%d (most recent first) ---\n", order);
        printCallData(&temp->data);
        printf("\n");
        temp = temp->next;
        order++;
    }

    printSeparator();
}

/*
 * undoLastDispatch — pops the top entry from the history stack
 * and adds it back to the pending list.
 *
 * Stack POP operation:
 *   1. Save historyTop->data
 *   2. Move historyTop to historyTop->next
 *   3. Free the old top node
 *   4. Re-add the data to the pending linked list
 */
void undoLastDispatch(void) {
    printf("\n");
    printSeparator();
    printf("  ============= UNDO LAST DISPATCH =============\n");
    printSeparator();

    if (historyTop == NULL) {
        printf("  (Nothing to undo — history stack is empty.)\n");
        printSeparator();
        return;
    }

    /* Step 1: Save the data from the top of the stack */
    CallData undoneCall = historyTop->data;

    /* Step 2: Pop the top node off the stack */
    HistoryNode *oldTop = historyTop;
    historyTop = historyTop->next;  /* Move top pointer down */
    free(oldTop);                   /* Free the old top node */
    oldTop = NULL;

    printf("  [STATUS] Popped Call #%d from history stack.\n", undoneCall.id);
    printf("  [STATUS] Returning it to the pending list...\n");

    /* Step 3: Add it back to the pending linked list */
    addPendingCall(undoneCall);

    printf("  [OK] Call #%d has been restored to pending.\n", undoneCall.id);
    printSeparator();
}

/* ================================================================
 *  BINARY SEARCH TREE — Search Index (keyed on animalType)
 *  Allows fast search by animal type using the BST property:
 *    left child < parent < right child (alphabetically).
 * ================================================================ */

/*
 * bstInsert — recursively inserts a call into the BST.
 *
 * Base case: root == NULL → create a new node here and return it.
 * Recursive case: compare the animal type:
 *   - If new call < root → go LEFT
 *   - If new call >= root → go RIGHT
 * Returns: the (possibly updated) subtree root.
 */
BSTNode *bstInsert(BSTNode *root, CallData call) {
    /* Base case: empty spot found — create node here */
    if (root == NULL) {
        BSTNode *newNode = (BSTNode *)malloc(sizeof(BSTNode));
        if (!newNode) {
            printf("  [ERROR] malloc failed for BST node!\n");
            return NULL;
        }
        newNode->data  = call;
        newNode->left  = NULL;
        newNode->right = NULL;
        return newNode;
    }

    /* Recursive case: compare and go left or right */
    int cmp = caseInsensitiveCompare(call.animalType, root->data.animalType);
    if (cmp < 0) {
        root->left = bstInsert(root->left, call);   /* Go left  */
    } else {
        root->right = bstInsert(root->right, call);  /* Go right */
    }

    return root;
}

/*
 * bstCountNodes — counts the total number of nodes in the BST.
 * Uses post-order traversal: count left + count right + 1 (self).
 */
int bstCountNodes(BSTNode *root) {
    if (root == NULL) {
        return 0;
    }
    return 1 + bstCountNodes(root->left) + bstCountNodes(root->right);
}

/*
 * bstSearchByAnimal — BST key search for a specific animal type.
 *
 * Uses the BST ordering property to prune branches:
 *   - If query < node's animal type → only search LEFT subtree
 *   - If query > node's animal type → only search RIGHT subtree
 *   - If query == node's animal type → MATCH! Check both subtrees
 *     (because duplicates go to the right during insertion)
 *
 * Average case: O(log n).  Worst case: O(n) if tree is degenerate.
 */
void bstSearchByAnimal(BSTNode *root, const char *type, int *count) {
    if (root == NULL) {
        return;  /* Base case: reached a leaf — stop */
    }

    int cmp = caseInsensitiveCompare(type, root->data.animalType);

    if (cmp < 0) {
        /* Query is smaller — only search left subtree */
        bstSearchByAnimal(root->left, type, count);
    } else if (cmp > 0) {
        /* Query is larger — only search right subtree */
        bstSearchByAnimal(root->right, type, count);
    } else {
        /* Match found! Print it and check both subtrees for duplicates */
        (*count)++;
        printf("  [Match %d]\n", *count);
        printCallData(&root->data);
        printf("\n");
        bstSearchByAnimal(root->left, type, count);
        bstSearchByAnimal(root->right, type, count);
    }
}

/*
 * bstSearchByLocation — in-order traversal for location keyword search.
 *
 * The BST is keyed on animalType, NOT location. So we cannot use the
 * BST property to prune — we must visit EVERY node.
 * In-order traversal: LEFT → NODE → RIGHT.
 * Time complexity: O(n) — must check all nodes.
 */
void bstSearchByLocation(BSTNode *root, const char *keyword, int *count) {
    if (root == NULL) {
        return;  /* Base case */
    }
    /* Visit left subtree first */
    bstSearchByLocation(root->left, keyword, count);

    /* Check current node */
    if (caseInsensitiveContains(root->data.location, keyword)) {
        (*count)++;
        printf("  [Match %d]\n", *count);
        printCallData(&root->data);
        printf("\n");
    }

    /* Visit right subtree */
    bstSearchByLocation(root->right, keyword, count);
}

/*
 * bstSearchByDescription — in-order traversal for description keyword.
 * Same as location search — full O(n) scan because BST is not keyed
 * on description.
 */
void bstSearchByDescription(BSTNode *root, const char *keyword, int *count) {
    if (root == NULL) {
        return;
    }
    bstSearchByDescription(root->left, keyword, count);

    if (caseInsensitiveContains(root->data.description, keyword)) {
        (*count)++;
        printf("  [Match %d]\n", *count);
        printCallData(&root->data);
        printf("\n");
    }

    bstSearchByDescription(root->right, keyword, count);
}

/* ================================================================
 *  MEMORY CLEANUP — Free all dynamically allocated nodes
 *  MUST be called before the program exits to avoid memory leaks.
 * ================================================================ */

/*
 * freeAllPending — frees every node in the pending linked list.
 * Uses two pointers: 'cur' for the current node and 'nxt' to
 * remember the next node BEFORE we free 'cur'.
 */
void freeAllPending(void) {
    PendingNode *cur = pendingHead;
    PendingNode *nxt;
    int freed = 0;
    while (cur != NULL) {
        nxt = cur->next;  /* Save next BEFORE freeing cur */
        free(cur);
        cur = nxt;
        freed++;
    }
    pendingHead = NULL;
    printf("  [CLEANUP] Freed %d pending node(s).\n", freed);
}

/*
 * freeAllHistory — frees every node on the history stack.
 * Same two-pointer technique as freeAllPending.
 */
void freeAllHistory(void) {
    HistoryNode *cur = historyTop;
    HistoryNode *nxt;
    int freed = 0;
    while (cur != NULL) {
        nxt = cur->next;
        free(cur);
        cur = nxt;
        freed++;
    }
    historyTop = NULL;
    printf("  [CLEANUP] Freed %d history node(s).\n", freed);
}

/*
 * freeAllBST — frees every node in the BST using post-order traversal.
 * Post-order means: free LEFT child, free RIGHT child, THEN free self.
 * This ensures we never access a freed node's children.
 */
void freeAllBST(BSTNode *root) {
    if (root == NULL) {
        return;
    }
    freeAllBST(root->left);    /* Free entire left subtree first  */
    freeAllBST(root->right);   /* Free entire right subtree next  */
    free(root);                /* Now safe to free this node       */
}

/*
 * freeAllMemory — master cleanup function.
 * Calls all three sub-cleanup functions and resets pointers.
 */
void freeAllMemory(void) {
    printf("\n  [CLEANUP] Freeing all dynamically allocated memory...\n");
    freeAllPending();
    freeAllHistory();
    freeAllBST(searchRoot);
    searchRoot = NULL;
    printf("  [OK] All memory freed successfully.\n");
}

/* ================================================================
 *  FILE I/O — Save and Load calls to/from a text file
 *  Uses pipe '|' as delimiter: id|location|animal|description|urgency
 * ================================================================ */

/*
 * saveCallsToFile — writes all pending calls to a text file.
 * Each line has the format: id|location|animalType|description|urgency
 *
 * Uses fopen("w") to create/overwrite the file, then fprintf for
 * each node in the pending list. Finally fclose() to flush and close.
 */
void saveCallsToFile(void) {
    printf("\n");
    printSeparator();
    printf("  ============= SAVE CALLS TO FILE =============\n");
    printSeparator();

    if (pendingHead == NULL) {
        printf("  (No pending calls to save.)\n");
        printSeparator();
        return;
    }

    /* Open file for writing — "w" creates or overwrites */
    FILE *fp = fopen(LOG_FILE, "w");
    if (fp == NULL) {
        printf("  [ERROR] Could not open file '%s' for writing!\n", LOG_FILE);
        printSeparator();
        return;
    }

    printf("  [STATUS] Writing to file '%s'...\n", LOG_FILE);

    /* Traverse the linked list and write each call */
    PendingNode *temp = pendingHead;
    int written = 0;
    while (temp != NULL) {
        fprintf(fp, "%d|%s|%s|%s|%d\n",
                temp->data.id,
                temp->data.location,
                temp->data.animalType,
                temp->data.description,
                temp->data.urgencyScore);
        written++;
        temp = temp->next;
    }

    fclose(fp);  /* Close the file — flushes the buffer to disk */
    printf("  [OK] Saved %d call(s) to '%s'.\n", written, LOG_FILE);
    printSeparator();
}

/*
 * loadCallsFromFile — reads calls from a text file and adds them
 * to the pending list and BST.
 *
 * Uses fopen("r") to open for reading, fgets to read each line,
 * and strtok to split by '|' delimiter.
 */
void loadCallsFromFile(void) {
    printf("\n");
    printSeparator();
    printf("  ============= LOAD CALLS FROM FILE =============\n");
    printSeparator();

    /* Open file for reading */
    FILE *fp = fopen(LOG_FILE, "r");
    if (fp == NULL) {
        printf("  [ERROR] Could not open file '%s' for reading!\n", LOG_FILE);
        printf("  Tip: Save some calls first using option 7.\n");
        printSeparator();
        return;
    }

    printf("  [STATUS] Reading from file '%s'...\n", LOG_FILE);

    char line[MAX_LINE];
    int loaded = 0;
    int skipped = 0;

    while (fgets(line, MAX_LINE, fp) != NULL) {
        /* Remove trailing newline */
        line[strcspn(line, "\n\r")] = '\0';

        /* Skip empty lines */
        if (strlen(line) == 0) {
            skipped++;
            continue;
        }

        /* Parse the pipe-delimited line */
        CallData call;
        char *tok;

        /* Parse ID */
        tok = strtok(line, "|");
        if (tok == NULL) { skipped++; continue; }
        call.id = atoi(tok);

        /* Parse location */
        tok = strtok(NULL, "|");
        if (tok == NULL) { skipped++; continue; }
        strncpy(call.location, tok, MAX_NAME - 1);
        call.location[MAX_NAME - 1] = '\0';

        /* Parse animal type */
        tok = strtok(NULL, "|");
        if (tok == NULL) { skipped++; continue; }
        strncpy(call.animalType, tok, MAX_NAME - 1);
        call.animalType[MAX_NAME - 1] = '\0';

        /* Parse description */
        tok = strtok(NULL, "|");
        if (tok == NULL) { skipped++; continue; }
        strncpy(call.description, tok, MAX_DESC - 1);
        call.description[MAX_DESC - 1] = '\0';

        /* Parse urgency score */
        tok = strtok(NULL, "|\n\r");
        if (tok == NULL) { skipped++; continue; }
        call.urgencyScore = atoi(tok);

        /* Validate urgency range */
        if (call.urgencyScore < 1)  call.urgencyScore = 1;
        if (call.urgencyScore > 10) call.urgencyScore = 10;

        /* Update next ID counter to avoid duplicates */
        if (call.id >= nextId) {
            nextId = call.id + 1;
        }

        /* Add to pending list and BST */
        addPendingCall(call);
        searchRoot = bstInsert(searchRoot, call);
        loaded++;
    }

    fclose(fp);

    printf("\n  [SUMMARY] Loaded: %d call(s). Skipped: %d line(s).\n",
           loaded, skipped);
    printSeparator();
}

/* ================================================================
 *  STATISTICS — Compute and display statistics about the calls
 *  Uses simple loops and counters — no advanced math needed.
 * ================================================================ */

/*
 * displayStatistics — shows summary statistics about pending calls.
 * Computes: total count, highest urgency, lowest urgency,
 * average urgency, and a count per animal type.
 */
void displayStatistics(void) {
    printf("\n");
    printSeparator();
    printf("  ============= SYSTEM STATISTICS =============\n");
    printSeparator();

    int pendingCount  = countPendingCalls();
    int historyCount  = countHistory();
    int bstNodeCount  = bstCountNodes(searchRoot);

    printf("  Pending calls in queue : %d\n", pendingCount);
    printf("  Dispatched (in history): %d\n", historyCount);
    printf("  BST search index nodes : %d\n", bstNodeCount);
    printf("\n");

    /* ---- Pending list statistics ---- */
    if (pendingCount == 0) {
        printf("  (No pending calls — statistics unavailable.)\n");
        printSeparator();
        return;
    }

    int highestUrgency = 0;
    int lowestUrgency  = 11;  /* Start higher than max possible */
    int totalUrgency   = 0;

    /* First pass: find min, max, and sum of urgency scores */
    PendingNode *temp = pendingHead;
    while (temp != NULL) {
        int urg = temp->data.urgencyScore;
        if (urg > highestUrgency) highestUrgency = urg;
        if (urg < lowestUrgency)  lowestUrgency  = urg;
        totalUrgency += urg;
        temp = temp->next;
    }

    /* Calculate average using integer division */
    int averageUrgency = totalUrgency / pendingCount;

    printf("  --- Urgency Statistics ---\n");
    printf("  Highest urgency score : %d / 10\n", highestUrgency);
    printf("  Lowest urgency score  : %d / 10\n", lowestUrgency);
    printf("  Average urgency score : %d / 10\n", averageUrgency);
    printf("  Sum of all urgencies  : %d\n", totalUrgency);
    printf("\n");

    /* ---- Count per animal type (simple nested loop approach) ---- */
    printf("  --- Calls by Animal Type ---\n");

    /*
     * We use a simple approach: for each node, check if we already
     * counted its animal type by looking at all previous nodes.
     * If it's the first occurrence, count all matching nodes.
     */
    PendingNode *outer = pendingHead;
    while (outer != NULL) {
        /* Check if this animal type was already counted */
        int alreadyCounted = 0;
        PendingNode *check = pendingHead;
        while (check != outer) {
            if (caseInsensitiveCompare(check->data.animalType,
                                       outer->data.animalType) == 0) {
                alreadyCounted = 1;
                break;
            }
            check = check->next;
        }

        if (!alreadyCounted) {
            /* Count all nodes with this animal type */
            int typeCount = 0;
            PendingNode *counter = pendingHead;
            while (counter != NULL) {
                if (caseInsensitiveCompare(counter->data.animalType,
                                           outer->data.animalType) == 0) {
                    typeCount++;
                }
                counter = counter->next;
            }
            printf("  %-20s : %d call(s)\n", outer->data.animalType, typeCount);
        }

        outer = outer->next;
    }

    printf("\n");
    printSeparator();
}

/* ================================================================
 *  EXPORT REPORT — Write a formatted report to a text file
 * ================================================================ */

/*
 * exportReport — writes a detailed summary report of the system
 * state to "rescue_report.txt".
 * Uses fprintf for formatted output to a file.
 */
void exportReport(void) {
    printf("\n");
    printSeparator();
    printf("  ============= EXPORT REPORT =============\n");
    printSeparator();

    FILE *fp = fopen("rescue_report.txt", "w");
    if (fp == NULL) {
        printf("  [ERROR] Could not create report file!\n");
        printSeparator();
        return;
    }

    fprintf(fp, "========================================\n");
    fprintf(fp, "  STRAY ANIMAL RESCUE - STATUS REPORT\n");
    fprintf(fp, "  Author: Sahil Kaushik\n");
    fprintf(fp, "========================================\n\n");

    /* Write pending calls to report */
    fprintf(fp, "--- PENDING CALLS ---\n");
    int pendCount = 0;
    PendingNode *pTemp = pendingHead;
    while (pTemp != NULL) {
        pendCount++;
        fprintf(fp, "  Call #%d | %s | %s | %s | Urgency: %d/10\n",
                pTemp->data.id,
                pTemp->data.location,
                pTemp->data.animalType,
                pTemp->data.description,
                pTemp->data.urgencyScore);
        pTemp = pTemp->next;
    }
    fprintf(fp, "Total pending: %d\n\n", pendCount);

    /* Write dispatch history to report */
    fprintf(fp, "--- DISPATCH HISTORY ---\n");
    int histCount = 0;
    HistoryNode *hTemp = historyTop;
    while (hTemp != NULL) {
        histCount++;
        fprintf(fp, "  Call #%d | %s | %s | Urgency: %d/10\n",
                hTemp->data.id,
                hTemp->data.location,
                hTemp->data.animalType,
                hTemp->data.urgencyScore);
        hTemp = hTemp->next;
    }
    fprintf(fp, "Total dispatched: %d\n\n", histCount);

    fprintf(fp, "--- SUMMARY ---\n");
    fprintf(fp, "Total calls logged    : %d\n", pendCount + histCount);
    fprintf(fp, "Pending calls         : %d\n", pendCount);
    fprintf(fp, "Dispatched calls      : %d\n", histCount);
    fprintf(fp, "BST index nodes       : %d\n", bstCountNodes(searchRoot));
    fprintf(fp, "========================================\n");

    fclose(fp);
    printf("  [OK] Report exported to 'rescue_report.txt'.\n");
    printSeparator();
}

/* ================================================================
 *  MENU ACTION FUNCTIONS
 *  Each function handles one menu option with full input validation.
 * ================================================================ */

/*
 * menuLogCall — handles the "Log a New Rescue Call" menu option.
 * Prompts the user for each field, validates input, then adds
 * the call to both the pending list and the BST search index.
 */
void menuLogCall(void) {
    CallData c;

    printf("\n");
    printSeparator();
    printf("  ============= LOG A NEW RESCUE CALL =============\n");
    printSeparator();

    /* Assign auto-incremented ID */
    c.id = nextId;
    printf("  [INFO] Assigning Call ID: %d\n\n", c.id);

    /* ---- Get and validate LOCATION ---- */
    printf("  Step 1: Enter the location where the animal was spotted.\n");
    printf("          (Max %d characters, letters/digits/spaces only)\n", MAX_NAME - 1);
    while (1) {
        printf("  Location: ");
        safeReadLine(c.location, MAX_NAME);
        if (validateTextField(c.location, MAX_NAME, "Location")) {
            break;  /* Valid — exit the loop */
        }
        printf("  Please try again.\n");
    }

    /* ---- Get and validate ANIMAL TYPE ---- */
    printf("\n  Step 2: Enter the type of animal (e.g., Dog, Cat, Bird).\n");
    printf("          (Max %d characters, letters/digits/spaces only)\n", MAX_NAME - 1);
    while (1) {
        printf("  Animal Type: ");
        safeReadLine(c.animalType, MAX_NAME);
        if (validateTextField(c.animalType, MAX_NAME, "Animal Type")) {
            break;
        }
        printf("  Please try again.\n");
    }

    /* ---- Get and validate DESCRIPTION ---- */
    printf("\n  Step 3: Provide a brief description of the situation.\n");
    printf("          (Max %d characters)\n", MAX_DESC - 1);
    while (1) {
        printf("  Description: ");
        safeReadLine(c.description, MAX_DESC);
        if (validateNonEmpty(c.description, "Description")) {
            printf("  [OK] Description accepted.\n");
            break;
        }
        printf("  Please try again.\n");
    }

    /* ---- Get and validate URGENCY SCORE ---- */
    printf("\n  Step 4: Rate the urgency (1 = low, 10 = critical).\n");
    while (1) {
        printf("  Urgency (1-10): ");
        int result = scanf("%d", &c.urgencyScore);
        while (getchar() != '\n');  /* Clear the input buffer */

        if (result != 1) {
            printf("  [VALIDATION ERROR] Please enter a number.\n");
            continue;
        }
        if (validateUrgency(c.urgencyScore)) {
            printf("  [OK] Urgency accepted: %d/10\n", c.urgencyScore);
            break;
        }
        printf("  Please try again.\n");
    }

    /* ---- Confirm and add ---- */
    printf("\n  [STATUS] Reviewing your input...\n");
    printf("  [STATUS] Call ID: %d\n", c.id);
    printf("  [STATUS] Location: %s\n", c.location);
    printf("  [STATUS] Animal: %s\n", c.animalType);
    printf("  [STATUS] Description: %s\n", c.description);
    printf("  [STATUS] Urgency: %d/10\n", c.urgencyScore);

    /* Increment the ID counter */
    nextId++;

    /* Add to the pending linked list */
    addPendingCall(c);

    /* Also insert into the BST for search */
    searchRoot = bstInsert(searchRoot, c);

    printf("\n  [DONE] Call #%d has been logged successfully!\n", c.id);
    printSeparator();
}

/*
 * menuSearchCalls — handles the "Search Calls" menu option.
 * Offers three search modes: by animal type, location, or description.
 */
void menuSearchCalls(void) {
    int choice;
    int found = 0;

    printf("\n");
    printSeparator();
    printf("  ============= SEARCH RESCUE CALLS =============\n");
    printSeparator();

    /* Check if there are any calls to search */
    if (searchRoot == NULL) {
        printf("  (No calls logged yet — nothing to search.)\n");
        printf("  Tip: Log some calls first using option 1.\n");
        printSeparator();
        return;
    }

    int totalNodes = bstCountNodes(searchRoot);
    printf("  BST contains %d searchable record(s).\n\n", totalNodes);

    printf("  Search options:\n");
    printf("    1. By Animal Type  (BST key search — fast)\n");
    printf("    2. By Location     (BST traversal — full scan)\n");
    printf("    3. By Description  (BST traversal — full scan)\n");
    printf("    4. Cancel search\n");
    printf("\n  Enter your choice (1-4): ");
    scanf("%d", &choice);
    while (getchar() != '\n');

    if (choice == 1) {
        /* Search by animal type — uses BST key ordering */
        char query[MAX_NAME];
        printf("\n  Enter the animal type to search for: ");
        safeReadLine(query, MAX_NAME);

        if (strlen(query) == 0) {
            printf("  [ERROR] Search query cannot be empty.\n");
            printSeparator();
            return;
        }

        printf("\n  [STATUS] Searching BST for animal type \"%s\"...\n\n", query);
        bstSearchByAnimal(searchRoot, query, &found);

    } else if (choice == 2) {
        /* Search by location — full BST traversal */
        char query[MAX_NAME];
        printf("\n  Enter a location keyword to search for: ");
        safeReadLine(query, MAX_NAME);

        if (strlen(query) == 0) {
            printf("  [ERROR] Search query cannot be empty.\n");
            printSeparator();
            return;
        }

        printf("\n  [STATUS] Scanning all records for location \"%s\"...\n\n", query);
        bstSearchByLocation(searchRoot, query, &found);

    } else if (choice == 3) {
        /* Search by description — full BST traversal */
        char query[MAX_NAME];
        printf("\n  Enter a description keyword to search for: ");
        safeReadLine(query, MAX_NAME);

        if (strlen(query) == 0) {
            printf("  [ERROR] Search query cannot be empty.\n");
            printSeparator();
            return;
        }

        printf("\n  [STATUS] Scanning all records for description \"%s\"...\n\n",
               query);
        bstSearchByDescription(searchRoot, query, &found);

    } else if (choice == 4) {
        printf("  [INFO] Search cancelled.\n");
        printSeparator();
        return;
    } else {
        printf("  [ERROR] Invalid option. Please enter 1, 2, 3, or 4.\n");
        printSeparator();
        return;
    }

    /* Show result summary */
    if (found == 0) {
        printf("  [RESULT] No matches found.\n");
    } else {
        printf("  [RESULT] Total matches found: %d\n", found);
    }

    printSeparator();
}

/*
 * menuViewCallById — lets the user look up a single call by its ID.
 * Traverses the pending list to find the matching node.
 */
void menuViewCallById(void) {
    int targetId;

    printf("\n");
    printSeparator();
    printf("  ============= VIEW CALL BY ID =============\n");
    printSeparator();

    if (pendingHead == NULL) {
        printf("  (No pending calls to view.)\n");
        printSeparator();
        return;
    }

    printf("  Enter the Call ID to look up: ");
    scanf("%d", &targetId);
    while (getchar() != '\n');

    printf("  [STATUS] Searching pending list for Call #%d...\n", targetId);

    CallData *result = findCallById(targetId);
    if (result != NULL) {
        printf("  [FOUND] Call #%d details:\n\n", targetId);
        printCallData(result);
    } else {
        printf("  [NOT FOUND] Call #%d is not in the pending list.\n", targetId);
        printf("  It may have been dispatched already. Check the history.\n");
    }

    printSeparator();
}

/* ================================================================
 *  MAIN FUNCTION — Menu loop using while(1) + switch
 *  This is the entry point of the program.
 * ================================================================ */

int main(void) {
    int choice;

    /* Display the startup banner */
    printBanner();

    printf("  [STATUS] System initialized. Ready to accept rescue calls.\n");
    printf("  [INFO] You can save/load calls to preserve data between runs.\n");

    /* ---- Main menu loop ---- */
    while (1) {
        printf("\n");
        printf("  ==================== MAIN MENU ====================\n");
        printf("  |                                                  |\n");
        printf("  |  1.  Log a New Rescue Call                       |\n");
        printf("  |  2.  View All Pending Calls                      |\n");
        printf("  |  3.  Dispatch Highest Priority Call               |\n");
        printf("  |  4.  Search Calls (Animal / Location / Desc)     |\n");
        printf("  |  5.  View Dispatch History                       |\n");
        printf("  |  6.  Undo Last Dispatch                          |\n");
        printf("  |  7.  Save Calls to File                          |\n");
        printf("  |  8.  Load Calls from File                        |\n");
        printf("  |  9.  View System Statistics                      |\n");
        printf("  |  10. View a Call by ID                           |\n");
        printf("  |  11. Export Full Report                          |\n");
        printf("  |  12. Exit                                        |\n");
        printf("  |                                                  |\n");
        printf("  ===================================================\n");
        printf("  Enter your choice (1-12): ");

        int result = scanf("%d", &choice);
        while (getchar() != '\n');  /* Clear the input buffer */

        /* Check if scanf successfully read an integer */
        if (result != 1) {
            printf("\n  [ERROR] Invalid input. Please enter a number 1-12.\n");
            continue;  /* Go back to the top of the while loop */
        }

        /* Handle the user's choice with a switch statement */
        switch (choice) {
            case 1:
                menuLogCall();
                break;

            case 2:
                viewPendingCalls();
                break;

            case 3:
                dispatchHighestPriority();
                break;

            case 4:
                menuSearchCalls();
                break;

            case 5:
                viewHistory();
                break;

            case 6:
                undoLastDispatch();
                break;

            case 7:
                saveCallsToFile();
                break;

            case 8:
                loadCallsFromFile();
                break;

            case 9:
                displayStatistics();
                break;

            case 10:
                menuViewCallById();
                break;

            case 11:
                exportReport();
                break;

            case 12:
                /* Exit the program */
                printf("\n  [STATUS] Shutting down the system...\n");
                freeAllMemory();
                printf("\n  ****************************************************\n");
                printf("  *    Thank you for using the Rescue System!        *\n");
                printf("  *    All memory has been freed. Goodbye!            *\n");
                printf("  ****************************************************\n\n");
                return 0;

            default:
                printf("\n  [ERROR] Invalid choice '%d'.\n", choice);
                printf("  Please enter a number between 1 and 12.\n");
                break;
        }
    }

    /* This line is never reached, but good practice */
    return 0;
}
