#include <assert.h>
#include "../utils/defs.h"
#include "objects.h"

const Type t_type = {
	.base = (Object) { .type = &t_type, .flags = Object_STATIC },
	.name = "type",
	.size = sizeof (Type),
};

const char *Object_GetName(const Object *obj)
{
	assert(obj);

	const Type *type = Object_GetType(obj);
	assert(type);

	const char *name = type->name;
	assert(name);

	return name;
}

const Type *Object_GetType(const Object *obj)
{
	assert(obj);
	assert(obj->type);
	return obj->type;
}

unsigned int Object_GetSize(const Object *obj, Error *err)
{
	assert(err);
	assert(obj);

	const Type *type = Object_GetType(obj);
	assert(type);

	return type->size;
}

unsigned int Object_GetDeepSize(const Object *obj, Error *err)
{
	assert(err);
	assert(obj);

	const Type *type = Object_GetType(obj);
	assert(type);

	if(type->deepsize == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(obj), __func__);
			return 0;
		}

	return type->deepsize(obj);
}

int Object_Hash(Object *obj, Error *err)
{
	assert(obj);

	const Type *type = Object_GetType(obj);
	assert(type);

	if(type->hash == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(obj), __func__);
			return -1;
		}

	return type->hash(obj);
}

Object *Object_Copy(Object *obj, Heap *heap, Error *err)
{
	assert(err);
	assert(obj);

	const Type *type = Object_GetType(obj);
	assert(type);

	if(type->copy == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(obj), __func__);
			return NULL;
		}

	return type->copy(obj, heap, err);
}

Object *Object_Call(Object *obj, Object **argv, unsigned int argc, Heap *heap, Error *err)
{
	assert(err);
	assert(obj);

	const Type *type = Object_GetType(obj);
	assert(type);

	if(type->call == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(obj), __func__);
			return NULL;
		}

	return type->call(obj, argv, argc, heap, err);
}

Object *Object_Select(Object *coll, Object *key, Heap *heap, Error *err)
{
	assert(err);
	assert(key);
	assert(coll);

	const Type *type = Object_GetType(coll);
	assert(type);

	if(type->select == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(coll), __func__);
			return NULL;
		}

	return type->select(coll, key, heap, err);	
}

Object *Object_Delete(Object *coll, Object *key, Heap *heap, Error *err)
{
	assert(err);
	assert(key);
	assert(coll);

	const Type *type = Object_GetType(coll);
	assert(type);

	if(type->delete == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(coll), __func__);
			return NULL;
		}

	return type->delete(coll, key, heap, err);	
}

_Bool Object_Insert(Object *coll, Object *key, Object *val, Heap *heap, Error *err)
{
	assert(err);
	assert(key);
	assert(coll);

	const Type *type = Object_GetType(coll);
	assert(type);

	if(type->insert == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(coll), __func__);
			return 0;
		}

	return type->insert(coll, key, val, heap, err);
}

int	Object_Count(Object *coll, Error *err)
{
	assert(err);
	assert(coll);

	const Type *type = Object_GetType(coll);
	assert(type);

	if(type->count == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(coll), __func__);
			return -1;
		}

	return type->count(coll);
}

Object *Object_Next(Object *iter, Heap *heap, Error *err)
{
	assert(err);
	assert(iter);

	const Type *type = Object_GetType(iter);
	assert(type);

	if(type->next == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(iter), __func__);
			return NULL;
		}

	return type->next(iter, heap, err);
}

Object *Object_Prev(Object *iter, Heap *heap, Error *err)
{
	assert(err);
	assert(iter);

	const Type *type = Object_GetType(iter);
	assert(type);

	if(type->prev == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(iter), __func__);
			return NULL;
		}

	return type->prev(iter, heap, err);
}

_Bool Object_IsInt(Object *obj)
{
	assert(obj != NULL);
	assert(obj->type != NULL);
	return obj->type->atomic == ATMTP_INT;
}

_Bool Object_IsFloat(Object *obj)
{
	assert(obj != NULL);
	assert(obj->type != NULL);
	return obj->type->atomic == ATMTP_FLOAT;
}

long long int Object_ToInt(Object *obj, Error *err)
{
	assert(err);
	assert(obj);

	if(!Object_IsInt(obj))
		{
			Error_Report(err, 0, "Object is not an integer");
			return 0;
		}

	const Type *type = Object_GetType(obj);
	assert(type);

	if(type->to_int == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(obj), __func__);
			return 0;
		}

	return type->to_int(obj, err);
}

double Object_ToFloat(Object *obj, Error *err)
{
	assert(err);
	assert(obj);

	if(!Object_IsFloat(obj))
		{
			Error_Report(err, 0, "Object is not a floating");
			return 0;
		}

	const Type *type = Object_GetType(obj);
	assert(type);

	if(type->to_float == NULL)
		{
			Error_Report(err, "Object %s doesn't implement %s", Object_GetName(obj), __func__);
			return 0;
		}

	return type->to_float(obj, err);
}

_Bool Object_Compare(Object *obj1, Object *obj2, Error *error)
{
	if(obj1->type != obj2->type)
		return 0;

	if(obj1->type->op_eql == NULL)
		{
			Error_Report(error, "Object %s doesn't implement %s", Object_GetName(obj1), __func__);
			return 0;
		}

	return obj1->type->op_eql(obj1, obj2);
}