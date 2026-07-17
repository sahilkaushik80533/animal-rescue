/*
 *  STRAY ANIMAL RESCUE DISPATCH SYSTEM — C DSA Engine (CLI)
 *  Author: Sahil Kaushik
 *
 *  Called by Python via subprocess. Reads pipe-delimited data from stdin,
 *  runs DSA algorithms, outputs results to stdout.
 *
 *  Usage:
 *    echo "data" | ./engine dispatch
 *    echo "data" | ./engine search animal Dog
 *    echo "data" | ./engine search location Sector
 *    echo "data" | ./engine sort
 *
 *  Input:  id|location|animalType|description|urgencyScore  (one per line)
 *  Output: DISPATCH_ID|<id>  /  MATCH_ID|<id>  /  SORTED|<id>|<urgency>
 *
 *  Compile: gcc engine.c -o engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME  50
#define MAX_DESC  200
#define MAX_LINE  512
#define MAX_CALLS 1000

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

/* BST node — left < parent, right >= parent (by animalType) */
typedef struct BSTNode {
    CallData         data;
    struct BSTNode  *left;
    struct BSTNode  *right;
} BSTNode;

PendingNode *pendingHead = NULL;
BSTNode     *searchRoot  = NULL;

/* ---- Utilities ---- */

void toLowerInPlace(char *s) {
    int i;
    for (i = 0; s[i]; i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] += 32;
}

int ciCompare(const char *a, const char *b) {
    char ca[MAX_NAME], cb[MAX_NAME];
    strncpy(ca, a, MAX_NAME-1); ca[MAX_NAME-1] = '\0';
    strncpy(cb, b, MAX_NAME-1); cb[MAX_NAME-1] = '\0';
    toLowerInPlace(ca); toLowerInPlace(cb);
    return strcmp(ca, cb);
}

int ciContains(const char *haystack, const char *needle) {
    char lh[MAX_DESC], ln[MAX_NAME];
    strncpy(lh, haystack, MAX_DESC-1); lh[MAX_DESC-1] = '\0';
    strncpy(ln, needle, MAX_NAME-1);   ln[MAX_NAME-1] = '\0';
    toLowerInPlace(lh); toLowerInPlace(ln);
    return (strstr(lh, ln) != NULL);
}

/* ---- Stdin Parsing ---- */

int parseLine(char *line, CallData *call) {
    char *tok;
    line[strcspn(line, "\n\r")] = '\0';
    if (strlen(line) == 0) return 0;

    tok = strtok(line, "|"); if (!tok) return 0;
    call->id = atoi(tok);

    tok = strtok(NULL, "|"); if (!tok) return 0;
    strncpy(call->location, tok, MAX_NAME-1); call->location[MAX_NAME-1] = '\0';

    tok = strtok(NULL, "|"); if (!tok) return 0;
    strncpy(call->animalType, tok, MAX_NAME-1); call->animalType[MAX_NAME-1] = '\0';

    tok = strtok(NULL, "|"); if (!tok) return 0;
    strncpy(call->description, tok, MAX_DESC-1); call->description[MAX_DESC-1] = '\0';

    tok = strtok(NULL, "|\n\r"); if (!tok) return 0;
    call->urgencyScore = atoi(tok);

    return 1;
}

int readAllCalls(CallData *calls, int max) {
    char line[MAX_LINE];
    int count = 0;
    while (fgets(line, MAX_LINE, stdin) && count < max) {
        if (parseLine(line, &calls[count])) count++;
    }
    return count;
}

/* ============================================================
 *  SINGLY LINKED LIST — for dispatch and sort
 * ============================================================ */

void addToList(CallData call) {
    PendingNode *n = (PendingNode *)malloc(sizeof(PendingNode));
    if (!n) return;
    n->data = call;
    n->next = NULL;

    if (!pendingHead) {
        pendingHead = n;
    } else {
        PendingNode *t = pendingHead;
        while (t->next) t = t->next;
        t->next = n;
    }
}

void buildListFromArray(CallData *calls, int count) {
    int i;
    for (i = 0; i < count; i++) addToList(calls[i]);
}

/*
 * findAndRemoveMax — traverses the list to find the highest urgency node.
 *
 * Two pointer pairs track the walk:
 *   (current, prev)   — node being inspected + its predecessor
 *   (maxNode, maxPrev) — best candidate + its predecessor
 *
 * Deletion re-links around maxNode:
 *   HEAD case:   pendingHead = maxNode->next
 *   MIDDLE case: maxPrev->next = maxNode->next
 *                [prev] -> [max] -> [after]  becomes  [prev] -> [after]
 * Then free(maxNode).
 */
CallData findAndRemoveMax(void) {
    CallData result;
    memset(&result, 0, sizeof(CallData));
    result.id = -1;
    if (!pendingHead) return result;

    PendingNode *maxNode = pendingHead, *maxPrev = NULL;
    PendingNode *current = pendingHead->next, *prev = pendingHead;

    while (current) {
        if (current->data.urgencyScore > maxNode->data.urgencyScore) {
            maxNode = current;
            maxPrev = prev;
        }
        prev    = current;
        current = current->next;
    }

    result = maxNode->data;

    if (!maxPrev)
        pendingHead = maxNode->next;
    else
        maxPrev->next = maxNode->next;

    free(maxNode);
    return result;
}

void freeList(void) {
    PendingNode *cur = pendingHead, *nxt;
    while (cur) { nxt = cur->next; free(cur); cur = nxt; }
    pendingHead = NULL;
}

/* ============================================================
 *  BST — Search index (keyed on animalType)
 * ============================================================ */

/*
 * bstInsert — recursive: NULL → create node; otherwise compare
 * animalType and recurse left (smaller) or right (>=).
 */
BSTNode *bstInsert(BSTNode *root, CallData call) {
    if (!root) {
        BSTNode *n = (BSTNode *)malloc(sizeof(BSTNode));
        if (!n) return NULL;
        n->data = call;
        n->left = n->right = NULL;
        return n;
    }
    if (ciCompare(call.animalType, root->data.animalType) < 0)
        root->left  = bstInsert(root->left, call);
    else
        root->right = bstInsert(root->right, call);
    return root;
}

/* BST key search — prunes branches using BST ordering property */
void bstSearchAnimal(BSTNode *root, const char *type, int *count) {
    if (!root) return;
    int cmp = ciCompare(type, root->data.animalType);
    if (cmp < 0) {
        bstSearchAnimal(root->left, type, count);
    } else if (cmp > 0) {
        bstSearchAnimal(root->right, type, count);
    } else {
        (*count)++;
        printf("MATCH_ID|%d\n", root->data.id);
        bstSearchAnimal(root->left, type, count);
        bstSearchAnimal(root->right, type, count);
    }
}

/* In-order traversal (LEFT→NODE→RIGHT) — full O(n) scan for location */
void bstSearchLocation(BSTNode *root, const char *kw, int *count) {
    if (!root) return;
    bstSearchLocation(root->left, kw, count);
    if (ciContains(root->data.location, kw)) {
        (*count)++;
        printf("MATCH_ID|%d\n", root->data.id);
    }
    bstSearchLocation(root->right, kw, count);
}

/* Post-order free: children before parent to avoid dangling pointers */
void freeBST(BSTNode *root) {
    if (!root) return;
    freeBST(root->left);
    freeBST(root->right);
    free(root);
}

/* ============================================================
 *  Command Handlers (called from main based on argv)
 * ============================================================ */

void commandDispatch(void) {
    CallData calls[MAX_CALLS];
    int count = readAllCalls(calls, MAX_CALLS);
    if (count == 0) { printf("ERROR|No pending calls\n"); return; }

    buildListFromArray(calls, count);
    CallData d = findAndRemoveMax();

    if (d.id != -1)
        printf("DISPATCH_ID|%d\n", d.id);
    else
        printf("ERROR|Dispatch failed\n");

    freeList();
}

void commandSearch(const char *type, const char *query) {
    CallData calls[MAX_CALLS];
    int count = readAllCalls(calls, MAX_CALLS);
    int matches = 0;
    int i;

    if (count == 0) { printf("TOTAL|0\n"); return; }

    for (i = 0; i < count; i++)
        searchRoot = bstInsert(searchRoot, calls[i]);

    if (strcmp(type, "animal") == 0)
        bstSearchAnimal(searchRoot, query, &matches);
    else if (strcmp(type, "location") == 0)
        bstSearchLocation(searchRoot, query, &matches);

    printf("TOTAL|%d\n", matches);
    freeBST(searchRoot);
    searchRoot = NULL;
}

/*
 * commandSort — selection sort via linked list.
 * Repeatedly extracts the max-urgency node (pointer re-link + free)
 * until the list is empty. Outputs IDs in descending urgency order.
 */
void commandSort(void) {
    CallData calls[MAX_CALLS];
    int count = readAllCalls(calls, MAX_CALLS);
    if (count == 0) return;

    buildListFromArray(calls, count);

    while (pendingHead) {
        CallData m = findAndRemoveMax();
        if (m.id != -1)
            printf("SORTED|%d|%d\n", m.id, m.urgencyScore);
    }
}

/* ---- Main ---- */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: engine <dispatch|sort|search animal|location query>\n");
        return 1;
    }

    if (strcmp(argv[1], "dispatch") == 0) {
        commandDispatch();
    } else if (strcmp(argv[1], "search") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: engine search <animal|location> <query>\n");
            return 1;
        }
        commandSearch(argv[2], argv[3]);
    } else if (strcmp(argv[1], "sort") == 0) {
        commandSort();
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
