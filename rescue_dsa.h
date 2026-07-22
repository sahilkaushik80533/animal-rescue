/*
 *  rescue_dsa.h — User Defined Header File
 *  Stray Animal Rescue Dispatch System
 *  Author: Sahil Kaushik
 *
 *  Contains all struct definitions and constants used by main.c.
 *  This is a "user-defined header file" — we put shared typedefs
 *  and #defines here so both main.c and engine.c stay in sync.
 */

#ifndef RESCUE_DSA_H
#define RESCUE_DSA_H

/* ---- Constants ---- */

#define MAX_NAME  50        /* Max characters for location / animal type   */
#define MAX_DESC  150       /* Max characters for the description field    */
#define MAX_LINE  512       /* Max characters for a single line of input   */
#define MAX_CALLS 1000      /* Upper limit on calls stored in an array     */
#define LOG_FILE  "rescue_log.txt"   /* Default save/load file name       */

/* ---- Data Structures ---- */

/* CallData: pure data container for one rescue call.
 * This is a simple struct — no pointers inside, just fixed arrays. */
typedef struct {
    int  id;                     /* Unique numeric ID for each call       */
    char location[MAX_NAME];     /* Where the animal was spotted          */
    char animalType[MAX_NAME];   /* e.g. "Dog", "Cat", "Bird"            */
    char description[MAX_DESC];  /* Brief description of the situation    */
    int  urgencyScore;           /* 1 (low) to 10 (critical)             */
} CallData;

/* Singly Linked List node — 'next' chains nodes sequentially.
 * Used for the pending-calls queue. */
typedef struct PendingNode {
    CallData            data;    /* The call data stored in this node     */
    struct PendingNode *next;    /* Pointer to the next node in the list  */
} PendingNode;

/* Stack node (LIFO) — 'next' points to the node below.
 * Used for dispatch history. */
typedef struct HistoryNode {
    CallData             data;   /* The call data stored in this node     */
    struct HistoryNode  *next;   /* Pointer to the node below on stack    */
} HistoryNode;

/* BST node — left < parent, right >= parent (by animalType).
 * Used for fast search by animal type. */
typedef struct BSTNode {
    CallData         data;       /* The call data stored in this node     */
    struct BSTNode  *left;       /* Left child (alphabetically smaller)   */
    struct BSTNode  *right;      /* Right child (alphabetically >= )      */
} BSTNode;

#endif /* RESCUE_DSA_H */
