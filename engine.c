/*
 *  STRAY ANIMAL RESCUE DISPATCH SYSTEM — C DSA Engine (Expanded)
 *  Author: Sahil Kaushik
 *
 *  This is the "engine" version of the system. It is called by the
 *  Python web server (app.py) via subprocess pipes.
 *
 *  HOW IT WORKS:
 *    - Python sends pipe-delimited data through stdin.
 *    - This C program processes it using DSA algorithms.
 *    - Results are printed to stdout for Python to read back.
 *
 *  COMMANDS (passed as argv):
 *    echo "data" | ./engine dispatch
 *    echo "data" | ./engine search animal Dog
 *    echo "data" | ./engine search location Sector
 *    echo "data" | ./engine search description injured
 *    echo "data" | ./engine sort
 *    echo "data" | ./engine stats
 *    echo "data" | ./engine validate location|animal|desc|urgency  value
 *
 *  INPUT FORMAT:   id|location|animalType|description|urgencyScore
 *  OUTPUT FORMAT:  DISPATCH_ID|<id>  /  MATCH_ID|<id>  /  SORTED|<id>|<urg>
 *
 *  DSA Concepts:
 *    - Singly Linked List (pending calls, dispatch extraction)
 *    - Stack / LIFO (history tracking)
 *    - Binary Search Tree (search index by animal type)
 *    - malloc / free (manual memory management)
 *    - File Handling (optional logging with fprintf)
 *    - Structs & Typedefs (CallData, PendingNode, HistoryNode, BSTNode)
 *
 *  Compile: gcc engine.c -o engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  CONSTANTS AND DATA STRUCTURES
 *  We define our own here (engine.c is standalone — does not use
 *  the rescue_dsa.h header so it can compile independently).
 * ================================================================ */

#define MAX_NAME  50       /* Max chars for location / animal type     */
#define MAX_DESC  200      /* Max chars for description                */
#define MAX_LINE  512      /* Max chars for a line read from stdin     */
#define MAX_CALLS 1000     /* Upper limit on total calls               */
#define ENGINE_LOG "engine_log.txt"  /* Log file for engine operations */

/*
 * CallData — a struct holding all information for one rescue call.
 * Uses fixed-size char arrays (no dynamic strings) for simplicity.
 */
typedef struct {
    int  id;                     /* Unique numeric ID                  */
    char location[MAX_NAME];     /* Where the animal was found         */
    char animalType[MAX_NAME];   /* Type of animal (Dog, Cat, etc.)    */
    char description[MAX_DESC];  /* Brief description of the situation */
    int  urgencyScore;           /* 1 (low) to 10 (critical)           */
} CallData;

/*
 * PendingNode — a singly linked list node.
 * 'data' holds the rescue call, 'next' points to the next node.
 */
typedef struct PendingNode {
    CallData            data;    /* The call data for this node        */
    struct PendingNode *next;    /* Pointer to the next node           */
} PendingNode;

/*
 * HistoryNode — a stack node (LIFO structure).
 * 'next' points to the node below this one on the stack.
 */
typedef struct HistoryNode {
    CallData             data;   /* The call data for this entry       */
    struct HistoryNode  *next;   /* Pointer to the node below          */
} HistoryNode;

/*
 * BSTNode — a binary search tree node.
 * Keyed on animalType: left < parent, right >= parent.
 */
typedef struct BSTNode {
    CallData         data;       /* The call data for this node        */
    struct BSTNode  *left;       /* Left child (alphabetically less)   */
    struct BSTNode  *right;      /* Right child (alphabetically >= )   */
} BSTNode;

/* ================================================================
 *  GLOBAL POINTERS
 *  Module-level variables shared across all functions.
 * ================================================================ */

PendingNode *pendingHead = NULL;  /* Head of the pending linked list  */
HistoryNode *historyTop  = NULL;  /* Top of the history stack         */
BSTNode     *searchRoot  = NULL;  /* Root of the BST search index     */

/* ================================================================
 *  UTILITY FUNCTIONS
 *  Small helper functions used throughout the engine.
 * ================================================================ */

/*
 * toLowerInPlace — converts a string to lowercase character by character.
 * If a character is between 'A' (65) and 'Z' (90), add 32 to get
 * the lowercase equivalent ('a' = 97).
 */
void toLowerInPlace(char *s) {
    int i;
    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = s[i] + 32;
        }
    }
}

/*
 * ciCompare — case-insensitive string comparison.
 * Makes safe copies using strncpy, converts to lowercase, then
 * uses strcmp to compare.
 * Returns: <0 if a<b, 0 if equal, >0 if a>b.
 */
int ciCompare(const char *a, const char *b) {
    char copyA[MAX_NAME], copyB[MAX_NAME];
    strncpy(copyA, a, MAX_NAME - 1);
    copyA[MAX_NAME - 1] = '\0';
    strncpy(copyB, b, MAX_NAME - 1);
    copyB[MAX_NAME - 1] = '\0';
    toLowerInPlace(copyA);
    toLowerInPlace(copyB);
    return strcmp(copyA, copyB);
}

/*
 * ciContains — case-insensitive substring search.
 * Checks if 'needle' appears anywhere inside 'haystack'.
 * Returns: 1 if found, 0 if not.
 */
int ciContains(const char *haystack, const char *needle) {
    char lowerHay[MAX_DESC], lowerNee[MAX_NAME];
    strncpy(lowerHay, haystack, MAX_DESC - 1);
    lowerHay[MAX_DESC - 1] = '\0';
    strncpy(lowerNee, needle, MAX_NAME - 1);
    lowerNee[MAX_NAME - 1] = '\0';
    toLowerInPlace(lowerHay);
    toLowerInPlace(lowerNee);
    return (strstr(lowerHay, lowerNee) != NULL);
}

/* ================================================================
 *  INPUT VALIDATION FUNCTIONS
 *  Used by the 'validate' command to check fields from the web UI.
 * ================================================================ */

/*
 * validateFieldNonEmpty — checks if a string is not empty.
 * Returns: 1 if valid, 0 if empty.
 */
int validateFieldNonEmpty(const char *str) {
    if (str == NULL || strlen(str) == 0) {
        return 0;  /* Invalid — empty */
    }
    return 1;  /* Valid */
}

/*
 * validateFieldLength — checks if a string fits within maxLen.
 * Returns: 1 if valid, 0 if too long.
 */
int validateFieldLength(const char *str, int maxLen) {
    if ((int)strlen(str) > maxLen - 1) {
        return 0;  /* Too long */
    }
    return 1;  /* Valid */
}

/*
 * validateFieldChars — checks if a string contains only safe characters.
 * Allows: letters, digits, spaces, commas, periods, hyphens.
 * Rejects: pipe '|' and other special characters.
 * Returns: 1 if valid, 0 if contains bad characters.
 */
int validateFieldChars(const char *str) {
    int i;
    for (i = 0; str[i] != '\0'; i++) {
        char ch = str[i];
        if (!((ch >= 'A' && ch <= 'Z') ||
              (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') ||
              ch == ' ' || ch == ',' || ch == '.' || ch == '-')) {
            return 0;  /* Bad character found */
        }
    }
    return 1;  /* All characters are valid */
}

/*
 * validateUrgencyRange — checks if an urgency value is 1-10.
 * Returns: 1 if valid, 0 if out of range.
 */
int validateUrgencyRange(int score) {
    if (score < 1 || score > 10) {
        return 0;  /* Out of range */
    }
    return 1;  /* Valid */
}

/* ================================================================
 *  STDIN PARSING — Read pipe-delimited call data from stdin
 *  Input format: id|location|animalType|description|urgencyScore
 * ================================================================ */

/*
 * parseLine — parses one pipe-delimited line into a CallData struct.
 *
 * Uses strtok() to split the line by '|' delimiter.
 * strtok replaces each '|' with '\0' and returns a pointer to
 * the start of each token.
 *
 * Returns: 1 if successfully parsed, 0 on error.
 */
int parseLine(char *line, CallData *call) {
    char *tok;

    /* Remove trailing newline/carriage return */
    line[strcspn(line, "\n\r")] = '\0';

    /* Skip empty lines */
    if (strlen(line) == 0) {
        return 0;
    }

    /* Token 1: ID (integer) */
    tok = strtok(line, "|");
    if (tok == NULL) { return 0; }
    call->id = atoi(tok);

    /* Token 2: Location (string) */
    tok = strtok(NULL, "|");
    if (tok == NULL) { return 0; }
    strncpy(call->location, tok, MAX_NAME - 1);
    call->location[MAX_NAME - 1] = '\0';

    /* Token 3: Animal Type (string) */
    tok = strtok(NULL, "|");
    if (tok == NULL) { return 0; }
    strncpy(call->animalType, tok, MAX_NAME - 1);
    call->animalType[MAX_NAME - 1] = '\0';

    /* Token 4: Description (string) */
    tok = strtok(NULL, "|");
    if (tok == NULL) { return 0; }
    strncpy(call->description, tok, MAX_DESC - 1);
    call->description[MAX_DESC - 1] = '\0';

    /* Token 5: Urgency Score (integer) */
    tok = strtok(NULL, "|\n\r");
    if (tok == NULL) { return 0; }
    call->urgencyScore = atoi(tok);

    /* Clamp urgency to valid range */
    if (call->urgencyScore < 1)  call->urgencyScore = 1;
    if (call->urgencyScore > 10) call->urgencyScore = 10;

    return 1;  /* Successfully parsed */
}

/*
 * readAllCalls — reads all pipe-delimited lines from stdin.
 * Stores parsed calls into the 'calls[]' array.
 * Returns: the number of calls successfully read.
 */
int readAllCalls(CallData *calls, int max) {
    char line[MAX_LINE];
    int count = 0;

    while (fgets(line, MAX_LINE, stdin) != NULL && count < max) {
        if (parseLine(line, &calls[count])) {
            count++;
        }
    }

    return count;
}

/* ================================================================
 *  SINGLY LINKED LIST — Pending calls queue
 *  Used for dispatch (extract max) and sort operations.
 * ================================================================ */

/*
 * addToList — appends a call to the END of the pending linked list.
 *
 * Steps:
 *   1. Allocate a new node with malloc.
 *   2. Copy the call data into the new node.
 *   3. If list is empty, new node becomes the head.
 *   4. Otherwise, traverse to the last node and link it.
 */
void addToList(CallData call) {
    /* Step 1: Allocate memory */
    PendingNode *newNode = (PendingNode *)malloc(sizeof(PendingNode));
    if (newNode == NULL) {
        fprintf(stderr, "ERROR|malloc failed in addToList\n");
        return;
    }

    /* Step 2: Copy data */
    newNode->data = call;
    newNode->next = NULL;

    /* Step 3 & 4: Insert into list */
    if (pendingHead == NULL) {
        /* List is empty — new node becomes head */
        pendingHead = newNode;
    } else {
        /* Traverse to the last node */
        PendingNode *temp = pendingHead;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        /* Link the new node after the last node */
        temp->next = newNode;
    }
}

/*
 * countList — counts the number of nodes in the pending list.
 * Returns: the count.
 */
int countList(void) {
    int count = 0;
    PendingNode *temp = pendingHead;
    while (temp != NULL) {
        count++;
        temp = temp->next;
    }
    return count;
}

/*
 * buildListFromArray — converts a CallData array into a linked list.
 * Loops through the array and calls addToList for each element.
 */
void buildListFromArray(CallData *calls, int count) {
    int i;
    for (i = 0; i < count; i++) {
        addToList(calls[i]);
    }
}

/*
 * findAndRemoveMax — traverses the list to find and remove the node
 * with the highest urgency score.
 *
 * ALGORITHM (Selection-based extraction):
 *   1. Start with head as the initial maximum.
 *   2. Walk through each node, comparing urgency scores.
 *   3. Track the maximum node AND its predecessor.
 *   4. Remove the max node by re-linking:
 *        HEAD case:   pendingHead = maxNode->next
 *        MIDDLE case: maxPrev->next = maxNode->next
 *   5. Free the removed node's memory.
 *
 * Uses two pointer pairs:
 *   (current, prev)    — the node being inspected + its predecessor
 *   (maxNode, maxPrev) — the best candidate + its predecessor
 *
 * Returns: the CallData of the removed node (id=-1 if list was empty).
 */
CallData findAndRemoveMax(void) {
    CallData result;
    memset(&result, 0, sizeof(CallData));
    result.id = -1;  /* Sentinel value — means "no result" */

    if (pendingHead == NULL) {
        return result;
    }

    /* Initialize: assume head is the maximum */
    PendingNode *maxNode = pendingHead;
    PendingNode *maxPrev = NULL;
    PendingNode *current = pendingHead->next;
    PendingNode *prev    = pendingHead;

    /* Walk the entire list, tracking the node with highest urgency */
    while (current != NULL) {
        if (current->data.urgencyScore > maxNode->data.urgencyScore) {
            maxNode = current;
            maxPrev = prev;
        }
        prev    = current;
        current = current->next;
    }

    /* Copy the data before freeing the node */
    result = maxNode->data;

    /* Remove maxNode by re-linking around it */
    if (maxPrev == NULL) {
        /* maxNode is the HEAD */
        pendingHead = maxNode->next;
    } else {
        /* maxNode is in the MIDDLE or at the TAIL */
        maxPrev->next = maxNode->next;
    }

    /* Free the removed node's memory */
    free(maxNode);
    maxNode = NULL;  /* Avoid dangling pointer */

    return result;
}

/*
 * freeList — frees all nodes in the pending linked list.
 * Uses two pointers: 'cur' and 'nxt' (save next before freeing cur).
 */
void freeList(void) {
    PendingNode *cur = pendingHead;
    PendingNode *nxt;
    while (cur != NULL) {
        nxt = cur->next;   /* Save pointer to next BEFORE freeing */
        free(cur);
        cur = nxt;
    }
    pendingHead = NULL;  /* Reset head to NULL */
}

/* ================================================================
 *  STACK — History (LIFO)
 *  Records dispatched calls so they can be tracked or undone.
 * ================================================================ */

/*
 * pushHistory — pushes a call onto the top of the history stack.
 * O(1) operation — just insert at the head.
 */
void pushHistory(CallData call) {
    HistoryNode *newNode = (HistoryNode *)malloc(sizeof(HistoryNode));
    if (newNode == NULL) {
        fprintf(stderr, "ERROR|malloc failed in pushHistory\n");
        return;
    }

    newNode->data = call;
    newNode->next = historyTop;   /* Link to old top */
    historyTop = newNode;         /* New node is the top */
}

/*
 * countHistoryStack — counts how many entries are on the stack.
 */
int countHistoryStack(void) {
    int count = 0;
    HistoryNode *temp = historyTop;
    while (temp != NULL) {
        count++;
        temp = temp->next;
    }
    return count;
}

/*
 * freeHistory — frees all nodes on the history stack.
 */
void freeHistory(void) {
    HistoryNode *cur = historyTop;
    HistoryNode *nxt;
    while (cur != NULL) {
        nxt = cur->next;
        free(cur);
        cur = nxt;
    }
    historyTop = NULL;
}

/* ================================================================
 *  BINARY SEARCH TREE — Search index keyed on animalType
 *  Enables fast search by animal type.
 * ================================================================ */

/*
 * bstInsert — recursively inserts a call into the BST.
 *
 * Base case: root == NULL → allocate a new node and return it.
 * Recursive case: compare animalType strings:
 *   - If new < root → recurse LEFT
 *   - If new >= root → recurse RIGHT
 * Returns: the (possibly updated) root of this subtree.
 */
BSTNode *bstInsert(BSTNode *root, CallData call) {
    /* Base case: empty spot — create a new node */
    if (root == NULL) {
        BSTNode *newNode = (BSTNode *)malloc(sizeof(BSTNode));
        if (newNode == NULL) {
            fprintf(stderr, "ERROR|malloc failed in bstInsert\n");
            return NULL;
        }
        newNode->data  = call;
        newNode->left  = NULL;
        newNode->right = NULL;
        return newNode;
    }

    /* Compare and recurse */
    int cmp = ciCompare(call.animalType, root->data.animalType);
    if (cmp < 0) {
        root->left = bstInsert(root->left, call);    /* Go left  */
    } else {
        root->right = bstInsert(root->right, call);   /* Go right */
    }

    return root;
}

/*
 * bstCountNodes — counts total nodes in the BST.
 * Post-order: count(left) + count(right) + 1.
 */
int bstCountNodes(BSTNode *root) {
    if (root == NULL) {
        return 0;
    }
    return 1 + bstCountNodes(root->left) + bstCountNodes(root->right);
}

/*
 * bstSearchAnimal — BST key search for a specific animal type.
 *
 * Uses the BST ordering property:
 *   - query < node → go LEFT only
 *   - query > node → go RIGHT only
 *   - query == node → MATCH, then check both subtrees for duplicates
 *
 * Outputs: MATCH_ID|<id> for each matching node.
 */
void bstSearchAnimal(BSTNode *root, const char *type, int *count) {
    if (root == NULL) {
        return;
    }

    int cmp = ciCompare(type, root->data.animalType);

    if (cmp < 0) {
        /* Query is alphabetically smaller — go left */
        bstSearchAnimal(root->left, type, count);
    } else if (cmp > 0) {
        /* Query is alphabetically larger — go right */
        bstSearchAnimal(root->right, type, count);
    } else {
        /* Match! Output this node's ID */
        (*count)++;
        printf("MATCH_ID|%d\n", root->data.id);
        /* Check both subtrees for additional matches */
        bstSearchAnimal(root->left, type, count);
        bstSearchAnimal(root->right, type, count);
    }
}

/*
 * bstSearchLocation — in-order traversal to find calls by location.
 *
 * BST is keyed on animalType, NOT location, so we cannot prune.
 * Must visit EVERY node: LEFT → NODE → RIGHT.
 * Time complexity: O(n).
 *
 * Outputs: MATCH_ID|<id> for each matching node.
 */
void bstSearchLocation(BSTNode *root, const char *keyword, int *count) {
    if (root == NULL) {
        return;
    }

    /* Visit left subtree */
    bstSearchLocation(root->left, keyword, count);

    /* Check current node's location */
    if (ciContains(root->data.location, keyword)) {
        (*count)++;
        printf("MATCH_ID|%d\n", root->data.id);
    }

    /* Visit right subtree */
    bstSearchLocation(root->right, keyword, count);
}

/*
 * bstSearchDescription — in-order traversal to find calls by description.
 * Same approach as location search — full O(n) scan.
 *
 * Outputs: MATCH_ID|<id> for each matching node.
 */
void bstSearchDescription(BSTNode *root, const char *keyword, int *count) {
    if (root == NULL) {
        return;
    }

    bstSearchDescription(root->left, keyword, count);

    if (ciContains(root->data.description, keyword)) {
        (*count)++;
        printf("MATCH_ID|%d\n", root->data.id);
    }

    bstSearchDescription(root->right, keyword, count);
}

/*
 * freeBST — frees all nodes using post-order traversal.
 * Post-order: free LEFT, free RIGHT, free SELF.
 * This ensures we never access a freed node's children.
 */
void freeBST(BSTNode *root) {
    if (root == NULL) {
        return;
    }
    freeBST(root->left);    /* Free left subtree first   */
    freeBST(root->right);   /* Free right subtree next   */
    free(root);              /* Now free this node itself  */
}

/* ================================================================
 *  ENGINE LOGGING — Write operation logs to a file
 *  Uses fprintf to append entries to the engine log file.
 * ================================================================ */

/*
 * logOperation — writes a log entry to the engine log file.
 * Opens the file in "a" (append) mode so previous entries are kept.
 */
void logOperation(const char *operation, int callId, int resultCode) {
    FILE *fp = fopen(ENGINE_LOG, "a");
    if (fp == NULL) {
        /* Silently fail — logging is optional */
        return;
    }

    fprintf(fp, "OP: %-12s | Call ID: %4d | Result: %d\n",
            operation, callId, resultCode);

    fclose(fp);
}

/* ================================================================
 *  COMMAND HANDLERS
 *  Each function handles one command passed via argv.
 * ================================================================ */

/*
 * commandDispatch — processes a 'dispatch' command.
 *
 * Steps:
 *   1. Read all calls from stdin into an array.
 *   2. Build a linked list from the array.
 *   3. Find and remove the node with highest urgency.
 *   4. Output the dispatched call's ID.
 *   5. Push the dispatched call to the history stack.
 *   6. Free all allocated memory.
 *
 * Output: DISPATCH_ID|<id>  (or ERROR|<message>)
 */
void commandDispatch(void) {
    CallData calls[MAX_CALLS];
    int count;
    CallData dispatched;

    /* Step 1: Read all calls from stdin */
    count = readAllCalls(calls, MAX_CALLS);
    if (count == 0) {
        printf("ERROR|No pending calls\n");
        logOperation("dispatch", -1, 0);
        return;
    }

    /* Step 2: Build the linked list */
    buildListFromArray(calls, count);

    /* Step 3: Find and remove the highest priority call */
    dispatched = findAndRemoveMax();

    /* Step 4: Output the result */
    if (dispatched.id != -1) {
        printf("DISPATCH_ID|%d\n", dispatched.id);
        /* Step 5: Push to history stack */
        pushHistory(dispatched);
        /* Log the operation */
        logOperation("dispatch", dispatched.id, 1);
    } else {
        printf("ERROR|Dispatch failed\n");
        logOperation("dispatch", -1, 0);
    }

    /* Step 6: Clean up */
    freeList();
    freeHistory();
}

/*
 * commandSearch — processes a 'search' command.
 *
 * Steps:
 *   1. Read all calls from stdin into an array.
 *   2. Build a BST from the array for searching.
 *   3. Run the appropriate search based on the search type.
 *   4. Output the total number of matches.
 *   5. Free all allocated memory.
 *
 * Search types: "animal", "location", "description"
 * Output: MATCH_ID|<id> (one per match) + TOTAL|<count>
 */
void commandSearch(const char *type, const char *query) {
    CallData calls[MAX_CALLS];
    int count;
    int matches = 0;
    int i;

    /* Step 1: Read all calls */
    count = readAllCalls(calls, MAX_CALLS);
    if (count == 0) {
        printf("TOTAL|0\n");
        logOperation("search", -1, 0);
        return;
    }

    /* Step 2: Build the BST */
    for (i = 0; i < count; i++) {
        searchRoot = bstInsert(searchRoot, calls[i]);
    }

    /* Step 3: Run the search */
    if (strcmp(type, "animal") == 0) {
        bstSearchAnimal(searchRoot, query, &matches);
        logOperation("search_anim", -1, matches);
    } else if (strcmp(type, "location") == 0) {
        bstSearchLocation(searchRoot, query, &matches);
        logOperation("search_loc", -1, matches);
    } else if (strcmp(type, "description") == 0) {
        bstSearchDescription(searchRoot, query, &matches);
        logOperation("search_desc", -1, matches);
    } else {
        fprintf(stderr, "ERROR|Unknown search type: %s\n", type);
        logOperation("search_err", -1, 0);
    }

    /* Step 4: Output total matches */
    printf("TOTAL|%d\n", matches);

    /* Step 5: Free BST memory */
    freeBST(searchRoot);
    searchRoot = NULL;
}

/*
 * commandSort — processes a 'sort' command.
 *
 * ALGORITHM: Selection Sort via linked list.
 *   Repeatedly find and extract the node with the maximum urgency
 *   until the list is empty. This produces calls in descending
 *   urgency order.
 *
 * Steps:
 *   1. Read all calls from stdin.
 *   2. Build a linked list.
 *   3. Loop: extract max, output it, repeat until empty.
 *
 * Output: SORTED|<id>|<urgency> (one per call, descending order)
 */
void commandSort(void) {
    CallData calls[MAX_CALLS];
    int count;
    int sortedCount = 0;

    /* Step 1: Read all calls */
    count = readAllCalls(calls, MAX_CALLS);
    if (count == 0) {
        logOperation("sort", -1, 0);
        return;
    }

    /* Step 2: Build the linked list */
    buildListFromArray(calls, count);

    /* Step 3: Repeatedly extract the maximum */
    while (pendingHead != NULL) {
        CallData maxCall = findAndRemoveMax();
        if (maxCall.id != -1) {
            printf("SORTED|%d|%d\n", maxCall.id, maxCall.urgencyScore);
            sortedCount++;
        }
    }

    logOperation("sort", -1, sortedCount);
}

/*
 * commandStats — processes a 'stats' command.
 *
 * Computes and outputs statistics about the calls:
 *   - Total count
 *   - Highest / lowest / average urgency
 *   - Count per animal type
 *
 * Output format: STAT|<key>|<value>
 */
void commandStats(void) {
    CallData calls[MAX_CALLS];
    int count;
    int i;

    /* Read all calls */
    count = readAllCalls(calls, MAX_CALLS);
    if (count == 0) {
        printf("STAT|total|0\n");
        logOperation("stats", -1, 0);
        return;
    }

    /* Calculate basic statistics */
    int highestUrgency = 0;
    int lowestUrgency  = 11;  /* Higher than max possible */
    int totalUrgency   = 0;

    for (i = 0; i < count; i++) {
        int urg = calls[i].urgencyScore;
        if (urg > highestUrgency) highestUrgency = urg;
        if (urg < lowestUrgency)  lowestUrgency  = urg;
        totalUrgency += urg;
    }

    int averageUrgency = totalUrgency / count;

    /* Output statistics */
    printf("STAT|total|%d\n", count);
    printf("STAT|highest_urgency|%d\n", highestUrgency);
    printf("STAT|lowest_urgency|%d\n", lowestUrgency);
    printf("STAT|average_urgency|%d\n", averageUrgency);
    printf("STAT|total_urgency|%d\n", totalUrgency);

    /*
     * Count per animal type using a simple O(n^2) approach:
     * For each call, check if its type was already counted by
     * looking at all previous calls.
     */
    for (i = 0; i < count; i++) {
        /* Check if this type was already counted */
        int alreadyCounted = 0;
        int j;
        for (j = 0; j < i; j++) {
            if (ciCompare(calls[j].animalType, calls[i].animalType) == 0) {
                alreadyCounted = 1;
                break;
            }
        }

        if (!alreadyCounted) {
            /* Count how many calls have this animal type */
            int typeCount = 0;
            int k;
            for (k = 0; k < count; k++) {
                if (ciCompare(calls[k].animalType, calls[i].animalType) == 0) {
                    typeCount++;
                }
            }
            printf("STAT|type_%s|%d\n", calls[i].animalType, typeCount);
        }
    }

    logOperation("stats", -1, count);
}

/*
 * commandValidate — processes a 'validate' command.
 *
 * Validates a single field value sent from the web UI.
 * The field name and value are passed as arguments.
 *
 * Output: VALID|1  (if valid) or VALID|0|<reason>  (if invalid)
 */
void commandValidate(const char *field, const char *value) {
    /* Validate based on field type */
    if (strcmp(field, "location") == 0) {
        /* Validate location field */
        if (!validateFieldNonEmpty(value)) {
            printf("VALID|0|Location cannot be empty\n");
            return;
        }
        if (!validateFieldLength(value, MAX_NAME)) {
            printf("VALID|0|Location is too long (max %d chars)\n", MAX_NAME - 1);
            return;
        }
        if (!validateFieldChars(value)) {
            printf("VALID|0|Location contains invalid characters\n");
            return;
        }
        printf("VALID|1\n");

    } else if (strcmp(field, "animal") == 0) {
        /* Validate animal type field */
        if (!validateFieldNonEmpty(value)) {
            printf("VALID|0|Animal type cannot be empty\n");
            return;
        }
        if (!validateFieldLength(value, MAX_NAME)) {
            printf("VALID|0|Animal type is too long (max %d chars)\n", MAX_NAME - 1);
            return;
        }
        if (!validateFieldChars(value)) {
            printf("VALID|0|Animal type contains invalid characters\n");
            return;
        }
        printf("VALID|1\n");

    } else if (strcmp(field, "description") == 0) {
        /* Validate description field */
        if (!validateFieldNonEmpty(value)) {
            printf("VALID|0|Description cannot be empty\n");
            return;
        }
        if (!validateFieldLength(value, MAX_DESC)) {
            printf("VALID|0|Description is too long (max %d chars)\n", MAX_DESC - 1);
            return;
        }
        printf("VALID|1\n");

    } else if (strcmp(field, "urgency") == 0) {
        /* Validate urgency field */
        int score = atoi(value);
        if (!validateUrgencyRange(score)) {
            printf("VALID|0|Urgency must be between 1 and 10\n");
            return;
        }
        printf("VALID|1\n");

    } else {
        printf("VALID|0|Unknown field: %s\n", field);
    }

    logOperation("validate", -1, 1);
}

/* ================================================================
 *  MAIN — Entry point for the engine
 *  Parses command-line arguments and routes to the correct handler.
 * ================================================================ */

int main(int argc, char *argv[]) {
    /* Check that at least one command argument was provided */
    if (argc < 2) {
        fprintf(stderr, "=======================================\n");
        fprintf(stderr, "  Stray Animal Rescue - DSA Engine\n");
        fprintf(stderr, "  Author: Sahil Kaushik\n");
        fprintf(stderr, "=======================================\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  engine dispatch\n");
        fprintf(stderr, "  engine sort\n");
        fprintf(stderr, "  engine stats\n");
        fprintf(stderr, "  engine search <animal|location|description> <query>\n");
        fprintf(stderr, "  engine validate <field> <value>\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Input: pipe-delimited data on stdin\n");
        fprintf(stderr, "  Format: id|location|animalType|description|urgency\n");
        fprintf(stderr, "\n");
        return 1;
    }

    /* Route to the correct command handler using if-else */
    if (strcmp(argv[1], "dispatch") == 0) {
        /* DISPATCH: Find and output the highest-priority call */
        commandDispatch();

    } else if (strcmp(argv[1], "search") == 0) {
        /* SEARCH: Requires a search type and query string */
        if (argc < 4) {
            fprintf(stderr, "Usage: engine search <animal|location|description> <query>\n");
            fprintf(stderr, "  Example: engine search animal Dog\n");
            fprintf(stderr, "  Example: engine search location Sector\n");
            fprintf(stderr, "  Example: engine search description injured\n");
            return 1;
        }
        commandSearch(argv[2], argv[3]);

    } else if (strcmp(argv[1], "sort") == 0) {
        /* SORT: Output all calls sorted by urgency (descending) */
        commandSort();

    } else if (strcmp(argv[1], "stats") == 0) {
        /* STATS: Compute and output statistics */
        commandStats();

    } else if (strcmp(argv[1], "validate") == 0) {
        /* VALIDATE: Check a single field value */
        if (argc < 4) {
            fprintf(stderr, "Usage: engine validate <field> <value>\n");
            fprintf(stderr, "  Fields: location, animal, description, urgency\n");
            fprintf(stderr, "  Example: engine validate location \"Sector 15\"\n");
            return 1;
        }
        commandValidate(argv[2], argv[3]);

    } else {
        /* Unknown command */
        fprintf(stderr, "ERROR|Unknown command: %s\n", argv[1]);
        fprintf(stderr, "Valid commands: dispatch, search, sort, stats, validate\n");
        return 1;
    }

    return 0;
}
