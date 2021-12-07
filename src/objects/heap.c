#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "objects.h"

#if USING_VALGRIND
#include <valgrind/memcheck.h>
#endif

typedef struct OflowAlloc OflowAlloc;
struct OflowAlloc {
	OflowAlloc *prev;
	char body[];
};

typedef struct {
	Object *object;
	_Bool (*destructor)(Object*, Error*);
} PendingDestruct;

struct xHeap {
	int objcount;
	int   size;
	int   used;
	int   total;
	void *body;
	OflowAlloc *oflow;
	PendingDestruct *pend;
	int pend_size, pend_used;

	_Bool collecting;
	_Bool collection_failed;
	int movedcount;
	void *old_body;
	int   old_used;
	int   old_total;
	OflowAlloc *old_oflow;
	Error *error;
};

Heap *Heap_New(int size)
{
	if(size < 0)
		size = 65536;

	Heap *heap = malloc(sizeof(Heap));

	if(heap == NULL)
		return NULL;

	heap->objcount = 0;
	heap->total = 0;
	heap->size = size;
	heap->used = 0;
	heap->body = malloc(size);
	heap->pend = NULL;
	heap->pend_size = 0;
	heap->pend_used = 0;
	heap->oflow = 0;
	heap->collecting = 0;

	if(heap->body == NULL)
		{
			free(heap);
			return NULL;
		}

#if USING_VALGRIND
	VALGRIND_CREATE_MEMPOOL(heap, 0, 0);
#endif

	return heap;
}

void Heap_Free(Heap *heap)
{

#if USING_VALGRIND
	VALGRIND_DESTROY_MEMPOOL(heap);
#endif

	Error error;
	Error_Init(&error);

	for(int i = 0; i < heap->pend_used; i += 1)
		{
			heap->pend[i].destructor(heap->pend[i].object, &error);
			if(error.occurred)
				{
					// Errors occurred! We can't do anything about
					// it now though.
					Error_Free(&error);
					Error_Init(&error);
				}
		}

	while(heap->oflow)
		{
			OflowAlloc *prev = heap->oflow->prev;
			free(heap->oflow);
			heap->oflow = prev;
		}

	free(heap->pend);
	free(heap->body);
	free(heap);
}

void *Heap_GetPointer(Heap *heap)
{
	return heap->body;
}

unsigned int Heap_GetSize(Heap *heap)
{
	return heap->size;
}

unsigned int Heap_GetObjectCount(Heap *heap)
{
	return heap->objcount;
}

float Heap_GetUsagePercentage(Heap *heap)
{
	return 100.0 * heap->total / heap->size;
}

void *Heap_Malloc(Heap *heap, TypeObject *type, Error *err)
{
	_Bool requires_destruct = type->free != NULL;

	if(requires_destruct)
		{
			// This type of object requires
			// a destructor to be called.
			if(heap->pend == NULL)
				{
					int n = 8;

					heap->pend = malloc(n * sizeof(PendingDestruct));
				
					if(heap->pend == NULL)
						{
							Error_Report(err, 1, "No memory");
							return NULL;
						}

					heap->pend_used = 0;
					heap->pend_size = n;
				}
			else if(heap->pend_size == heap->pend_used)
				{
					int factor = 2;

					void *new_pend = realloc(heap->pend, factor * heap->pend_size * sizeof(PendingDestruct));

					if(new_pend == NULL)
						{
							Error_Report(err, 1, "No memory");
							return NULL;
						}

					heap->pend = new_pend;
					heap->pend_size *= factor;
				}

			assert(heap->pend_size > heap->pend_used);
		}

	int size = type->size;

	if(size < (int) sizeof(MovedObject))
		size = sizeof(MovedObject);

	void *addr = Heap_RawMalloc(heap, type->size, err);

	if(addr == NULL)
		return NULL;

	Object *obj = addr;

	obj->type = type;
	obj->flags = 0;

	if(type->init && !type->init(obj, err))
		return NULL;

	obj->type = type;
	obj->flags = 0;

	if(requires_destruct)
		heap->pend[heap->pend_used++] = (PendingDestruct) { .object = obj, .destructor = obj->type->free };

	heap->objcount += 1;

	return (Object*) addr;
}

void *Heap_RawMalloc(Heap *heap, int size, Error *err)
{
	assert(err);
	assert(heap);
	assert(size > -1);

	void *addr;

	int padding = heap->used;

	if(heap->used & 7)
		heap->used = (heap->used & ~7) + 8;

	padding = heap->used - padding;

	if(heap->used + size > heap->size)
		{
			OflowAlloc *oflow = malloc(sizeof(OflowAlloc) + size);

			if(oflow == 0)
				return 0;

			oflow->prev = heap->oflow;
			heap->oflow = oflow;

			addr = oflow->body;
		}
	else
		{
			assert(heap->used + size <= heap->size);
			addr = heap->body + heap->used;
			heap->used += size;
		}

	heap->total += size + padding;

	assert(((intptr_t) addr) % 8 == 0);

#if USING_VALGRIND
	VALGRIND_MEMPOOL_ALLOC(heap, addr, size);
#endif

	return addr;
}

_Bool Heap_StartCollection(Heap *heap, Error *error)
{
	assert(heap->collecting == 0);

	void *new_body = malloc(heap->size);

	if(new_body == NULL)
		{
			Error_Report(error, 1, "No memory");
			return 0;
		}

	heap->old_body = heap->body;
	heap->old_used = heap->used;
	heap->old_total = heap->total;
	heap->old_oflow = heap->oflow;
	heap->total = 0;
	heap->body = new_body;
	heap->used = 0;
	heap->oflow = NULL;
	heap->collecting = 1;
	heap->collection_failed = 0;
	heap->movedcount = 0;
	heap->error = error;
	return 1;
}

_Bool Heap_StopCollection(Heap *heap)
{
	assert(heap->collecting == 1);

	if(heap->collection_failed)
		{
			free(heap->old_body);
			return 0;
		}

	/* Call destructors here */
	{
		int i = 0;
	
		while(i < heap->pend_used)
			{
				Object *obj = heap->pend[i].object;

				if(obj->flags & Object_MOVED)
					{
						heap->pend[i].object = ((MovedObject*) heap->pend[i].object)->new_location;
						i += 1;
					}
				else
					{
						// We need to call the destructor.

						heap->pend[i].destructor(obj, heap->error);
						
						if(heap->error->occurred) 
							return 0; // There will be leaks.
					
						heap->pend[i] = heap->pend[heap->pend_used-1];
						heap->pend_used -= 1;
					}
			}

		if(heap->pend_size / 2 > heap->pend_used)
			{
				// Downsize
				void *temp = realloc(heap->pend, heap->pend_size / 2 * sizeof(PendingDestruct));

				if(temp != NULL)
					{
						heap->pend = temp;
						heap->pend_size /= 2;
					}
			}
	}

	while(heap->old_oflow)
		{
			OflowAlloc *prev = heap->old_oflow->prev;
			free(heap->old_oflow);
			heap->old_oflow = prev;
		}

	free(heap->old_body);

	heap->collecting = 0;
	heap->objcount = heap->movedcount;
	return 1;
}

void Heap_CollectExtension(void **referer, unsigned int size, void *userp)
{
	Heap *heap = userp;

	assert(referer != NULL);
	assert(heap->collecting);

	void *old_location = *referer;

	if(heap->collection_failed || old_location == NULL)
		return;

	void *new_location = Heap_RawMalloc(heap, size, heap->error);

	if(new_location == NULL)
		{
			heap->collection_failed = 1;
			return;
		}

	memcpy(new_location, old_location, size);

	*referer = new_location;
}

void Heap_CollectReference(Object **referer, void *userp)
{
	Heap *heap = userp;

	assert(referer != NULL);
	assert(heap->collecting);

	Object *old_location = *referer;

	if(heap->collection_failed || old_location == NULL)
		return;

	if(old_location->flags & Object_MOVED)
	
		// The object was already moved.
		*referer = ((MovedObject*) old_location)->new_location;

	else
		{
			Object *new_location;

			// This object wasn't moved to
			// the new heap yet.

			if(old_location->flags & Object_STATIC)
	
				// The object doesn't need to be moved
				// since it was statically allocated.
				new_location = old_location;
			else
				{
					// Get some information.
					TypeObject *type = old_location->type;
					int size         = type->size;

					// Copy the object to a new location.
					{
						new_location = Heap_RawMalloc(heap, size, heap->error);

						if(new_location == NULL)
							{
								heap->collection_failed = 1;
								return;
							}

						memcpy(new_location, old_location, size);
					}

					// Set the old location as moved and
					// leave the reference to the new
					// location.
					{
						old_location->flags |= Object_MOVED;

						assert((int) sizeof(MovedObject) <= size);
						((MovedObject*) old_location)->new_location = new_location;
					}

					heap->movedcount += 1;
				}

			// Collect the reference to the type.
			if((Object*) new_location->type != new_location)
				Heap_CollectReference((Object**) &new_location->type, heap);

			// Collect all of the references to
			// extensions allocate using the GC'd
			// heap.
			Object_WalkExtensions(new_location, Heap_CollectExtension, heap);

			// Now collect all of the children.
			Object_WalkReferences(new_location, Heap_CollectReference, heap);
		
			// Update the referer
			*referer = new_location;
		}
}