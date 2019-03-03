// Luke Karavolis (ljk55)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mymalloc.h"

// USE THIS GODDAMN MACRO OKAY
#define PTR_ADD_BYTES(ptr, byte_offs) ((void*)(((char*)(ptr)) + (byte_offs)))

// Don't change or remove these constants.
#define MINIMUM_ALLOCATION  16
#define SIZE_MULTIPLE       8

/* Struct: Header
* -------------------
* This is the Header at the beginning of each allocated memory block. Size
* indicates the size of the block. Positive indicates it is free, negative
* indicates it is used. Each Header contains a pointer to the next Header and
* the previous Header in order to construct a doubly linked list
*/
typedef struct Header {
	int size;
	struct Header* next;
	struct Header* prev;
} Header;

// Global variables for the head and tail of the linked list heap
Header* heapHead = NULL;
Header* heapTail = NULL;

/* Function: round_up_size
* ------------------------------
* I don't know. Jarrett put this here
*/
unsigned int round_up_size(unsigned int data_size) {
	if(data_size == 0)
		return 0;
	else if(data_size < MINIMUM_ALLOCATION)
		return MINIMUM_ALLOCATION;
	else
		return (data_size + (SIZE_MULTIPLE - 1)) & ~(SIZE_MULTIPLE - 1);
}

/* Function: listPrint
* -----------------------
* Prints the linked list starting with the parameter
*
* @param head the beginning of the chain
*/
void listPrint() {
      if (heapHead == NULL) return;
      Header* currentHeader = heapHead;
      while (currentHeader->next != NULL) {
            printf("%i(%p) -> ", currentHeader->size, currentHeader);
            currentHeader = currentHeader->next;
      }
	printf("%i(%p)\n", currentHeader->size, currentHeader);
}

/* Function: listAppend
* ------------------------
* creates a new node using createNode(value) and adds it to the tail of the list
*
* @param head first node in the linked list
* @param value value for the new node
* @return returns a pointer to the newest Node
*/
void listAppend(Header* newHeader) {
	Header* currentHeader = heapHead;
	if (currentHeader == NULL) {
		heapHead = newHeader;
		heapHead->prev = NULL;
		heapHead->next = NULL;
		heapTail = heapHead;
		return;
	}
      while (currentHeader->next != NULL) {
            currentHeader = currentHeader->next;
      }
	currentHeader->next = newHeader;
	heapTail = newHeader;
	heapTail->prev = currentHeader;
	heapTail->next = NULL;
	return;
}

/* Function: listRemove
* --------------------------
* This function removes a node from a doubly linked list. It checks corner cases
* then updates next and prev accordingly
*
* @param node node to be removed
*/
void listRemove(Header* node) {
	if (node == NULL) return;
	if (node == heapHead && node == heapTail) {
		heapHead = NULL;
		heapTail = NULL;
	} else if (node == heapHead) {
		heapHead = node->next;
		heapHead->prev = NULL;
	} else if (node == heapTail) {
		heapTail = node->prev;
		heapTail->next = NULL;
	} else {
		Header* prevNode = node->prev;
		Header* nextNode = node->next;
		prevNode->next = nextNode;
		nextNode->prev = prevNode;
	}
	return;
}

/* Function: listAddAfter
* --------------------------
* This function inserts a new node into a doubly linked list after another given
* node. It handles corner cases and updates next and prev accordingly
*
* @param prevNode node after which to insert
* @param newNode node to be inserted
*/
void listAddAfter(Header* prevNode, Header* newNode) {
	if (prevNode == heapTail) {
		listAppend(newNode);
	} else {
		prevNode->next->prev = newNode;
		newNode->next = prevNode->next;
		prevNode->next = newNode;
		newNode->prev = prevNode;
	}
}

/* Function: findBlock
* -------------------------
* Starts at the beginning the the heap linked list and searches for a block
* large enough for the new data. Returns a pointer to where the new Header will
* start
*
* @param size number of bytes the new data is
* @return a pointer to where the new Header will start
*/
Header* findBlock(unsigned int size) {
	Header* currentHeader;
	for (currentHeader = heapHead; currentHeader != NULL; currentHeader = currentHeader->next) {
		if (currentHeader->size > 0 && currentHeader->size >= size) {
			return currentHeader;
		}
	}
	return NULL;
}

/* Function: moveBreak
* ------------------------
* This occurs when the break must be moved up for a new block. The break
* is moved up the size of the data plus enough for a new Header. The size
* is placed in the Header. The list is then updated
*
* @param size the size of the new data
*/ // TODO: abstract updated the list
Header* moveBreak(unsigned int size) {
	Header* newHeader = sbrk(size + sizeof(Header));
	newHeader->size = size;
	return newHeader;
}

/* Function: splitBlock
* -------------------------
* This function takes a large block and fills the first part with a header and
* space for the allocation. The second part will be another header indicating
* the extra free space
*
* @param size size of the allocated block
* @param blockToSplit pointer to the current large block
* @return a pointer to the new header indicating the free space
*/
Header* splitBlock(unsigned int size, Header* blockToSplit) {
	blockToSplit->size = -1*size;
	Header* newHeader = PTR_ADD_BYTES(blockToSplit, (sizeof(Header)+size));
	return newHeader;
}

/* Function: canCoalesce
* ------------------------
* This function checks if a node can be coalesced with its neighboring nodes.
* It will first check to its left and then its right.
*
* @param node node that is being checked
* @return -1 if it can be coalesced to the left, 1 if it can be coalesced to
* the right, 0 if it can't be coalesced
*/
int canCoalesce(Header* node) {
	if (node->prev != NULL) {
		if (node->prev->size > 0) {
			return -1;
		}
	}
	if (node->next != NULL) {
		if (node->next->size > 0) {
			return 1;
		}
	}
	return 0;
}

/* Function: coalesce
* -----------------------
* This function will take two free blocks and coalesce them into one. It will
* continue this process until all neighboring free blocks are joined.
*
* @param node node to be coalesced
* @return a pointer to the new larger coalesced free block
*/
Header* coalesce(Header* node) {
	Header* newNode = node;
	for (int coalesceDirection = canCoalesce(node); coalesceDirection != 0; coalesceDirection = canCoalesce(newNode)) {
		if (coalesceDirection == -1) {
			newNode = node->prev;
			newNode->size += sizeof(Header) + newNode->next->size;
			listRemove(newNode->next);
		} else if (coalesceDirection == 1) {
			newNode = node->next;
			newNode->size += sizeof(Header) + newNode->prev->size;
			listRemove(newNode->prev);
		}
	}
	return newNode;
}

void* my_malloc(unsigned int size) {
	if(size == 0) {
		return NULL;
	}
	size = round_up_size(size);
	// ------- Don't remove anything above this line. -------
	Header* newSpot = findBlock(size);
	Header* newHeader = newSpot;
	if (newSpot == NULL) {
		newHeader = moveBreak(size);
		listAppend(newHeader);
	} else {
		if (newSpot->size >= (size + sizeof(Header) + sizeof(Header) + MINIMUM_ALLOCATION)) {
			Header* newEmptyHeader = PTR_ADD_BYTES(newHeader, (sizeof(Header)+size));
			newEmptyHeader->size = newHeader->size - size - sizeof(Header);
			newHeader->size = size;
			listAddAfter(newHeader, newEmptyHeader);
		}
	}
	newHeader->size *= -1;
	return PTR_ADD_BYTES(newHeader, sizeof(Header));
}

void my_free(void* ptr) {
	if (ptr == NULL) {
		return;
	}
	Header* headerPtr = (Header*)PTR_ADD_BYTES(ptr, -1*sizeof(Header));
	headerPtr->size *= -1;
	headerPtr = coalesce(headerPtr);
	if (heapTail->size > 0) {
		sbrk(-1*(heapTail->size + sizeof(Header)));
		listRemove(heapTail);
	}
	return;
}
