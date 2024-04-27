/*
 * MIT License
 * Copyright (c) Çağan Korkmaz <root@hsnovel.net>
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#ifndef FMAP_H
#define FMAP_H

#include <stddef.h>
#include <stdint.h>
#define FMAP_CAP 16
#define FMAP_LOAD_FACTOR 0.75

/*
 * Value and key are not visible on struct but
 * they can be accessed by calculating their
 * position using key1_size and key2_size.
 *
 * The enough space is always allocated for
 * both of them.
 *
 * Key and data pointers are not necesarry
 * but they make it easier to get fmapentry
 */
struct fmapentry {
	struct fmapentry *next;
	uint32_t hash;
};

typedef struct {
	size_t key1_size;
	size_t key2_size;
	size_t mapentry_size;

	size_t bucket_cap;
	size_t num_buckets;

	void *buckets;

	void * (*allocfunc) (size_t size);
	void (*freefunc) (void *ptr);
	uint32_t (*hashfunc) (uint8_t *key, size_t len);
} fmap;

uint32_t fmap_init(fmap *map, uint32_t key1_size, uint32_t key2_size, uint32_t (*hashfunc) (uint8_t *key, size_t len));
void fmap_set_allocation_funcs(fmap *map, void * (*allocfunc) (size_t size), void (*freefunc) (void *ptr));
void fmap_push(fmap *map, void *key, void *val);
void *fmap_get(fmap *map, void *key);
void fmap_free(fmap *map);
void *fmap_pushp(fmap *map, void *key);

#ifdef FMAP_IMPLEMENTATION
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

uint32_t fmap_init_num(fmap *map, uint32_t key1_size, uint32_t key2_size, uint32_t (*hashfunc) (uint8_t *key, size_t len), int num)
{
	/* @Todo: Add default hash function */
	assert(hashfunc != NULL && "fmap_init: hashfunc parameter is null");
	assert(map != NULL && "fmap_init: map parameter is null");

	map->buckets = calloc(num, sizeof(void*));

	map->bucket_cap  = num;
	map->num_buckets = 0;
	map->mapentry_size = sizeof(struct fmapentry) + key1_size + key2_size;

	map->key1_size = key1_size;
	map->key2_size = key2_size;
	map->hashfunc  = hashfunc;
	map->allocfunc = malloc;
	map->freefunc = free;

	return 0;
}

uint32_t fmap_init(fmap *map, uint32_t key1_size, uint32_t key2_size, uint32_t (*hashfunc) (uint8_t *key, size_t len))
{
	return fmap_init_num(map, key1_size, key2_size, hashfunc, FMAP_CAP);
}

void fmap_set_allocation_funcs(fmap *map, void * (*allocfunc) (size_t size), void (*freefunc) (void *ptr)) {
	map->allocfunc = allocfunc;
	map->freefunc = freefunc;
}

static void fmap_initialize_fmapentry(fmap *map, struct fmapentry *entry, uint32_t hash, void *key, void *value) {
	entry->next = NULL;
	entry->hash = hash;
	memcpy((char*)entry + sizeof(struct fmapentry), key, map->key1_size);
	memcpy((char*)entry + sizeof(struct fmapentry) + map->key1_size, value, map->key2_size);
}

static void *fmap_initialize_fmapentryp(fmap *map, struct fmapentry *entry, uint32_t hash, void *key) {
	entry->next = NULL;
	entry->hash = hash;
	memcpy((char*)entry + sizeof(struct fmapentry), key, map->key1_size);
	return (char*)entry + sizeof(struct fmapentry) + map->key1_size;
}

void fmap_new_entry(fmap *map, void *key, uint32_t hash, void *value, uint32_t idx)
{
	assert(map != NULL && "fmap_new_entry: 'fmap *map' parameter is null");
	assert(key != NULL && "fmap_new_entry: 'void *key' parameter is null");
	assert(value != NULL && "fmap_new_entry: 'void *value' parameter is null");

	size_t **bucket_ptr = (map->buckets + sizeof(size_t) *idx);
	if ( *bucket_ptr == 0  ) {
		struct fmapentry *new_bucket;

		new_bucket = map->allocfunc(map->mapentry_size);
		fmap_initialize_fmapentry(map, new_bucket, hash, key, value);

		size_t **base_address = map->buckets + (sizeof(size_t) * idx);
		*base_address = (size_t*)new_bucket;
		map->num_buckets++;
		return;
	}

	struct fmapentry *entry = (struct fmapentry*)(*bucket_ptr);
	while ( entry->next ) {
		entry = entry->next;
	}

	entry->next = map->allocfunc(map->mapentry_size);
	entry = entry->next;
	fmap_initialize_fmapentry(map, entry, hash, key, value);
}

void fmap_push(fmap *map, void *key, void *val);

/* TODO: make a iterator for the fmap */
#define FOR_EACH_FMAP_ENTRY()

static void *fmap_get_value_from_fmapentry(fmap *map, struct fmapentry *entry) {
	return ((unsigned char*)entry) + sizeof(struct fmapentry) + map->key1_size;
}

static void *fmap_get_key_from_fmapentry(fmap *map, struct fmapentry *entry) {
	return ((unsigned char*)entry) + sizeof(struct fmapentry);
}

void fmap_grow(fmap *map)
{
	fmap tmp;
	size_t newcap;

	newcap = map->bucket_cap * 2;

	fmap_init_num(&tmp, map->key1_size, map->key2_size, map->hashfunc, newcap);

	for ( size_t i = 0; i < map->bucket_cap; i++ ) {
		struct fmapentry *entry;
		size_t **bucket_ptr = map->buckets + (sizeof(size_t) * i);

		entry = (struct fmapentry*)(*bucket_ptr);

		if ( *bucket_ptr == 0 )
			continue;

		do {
			void *value = fmap_get_value_from_fmapentry(map, entry);
			void *key   = fmap_get_key_from_fmapentry(map, entry);
			fmap_push(&tmp, key, value);
			struct fmapentry *tmp = entry;
			entry = entry->next;
			map->freefunc(tmp);
		} while ( entry != NULL  );
	}

	map->freefunc(map->buckets);
	map->buckets = tmp.buckets;
	map->bucket_cap = newcap;
}

void fmap_push(fmap *map, void *key, void *val)
{
	assert(map != NULL && "fmap_push: fmap *map parameter is null");
	assert(key != NULL && "fmap_push: void *key parameter is null");
	assert(val != NULL && "fmap_push: void *value parameter is null");

	float load_factor;

	if ( map->bucket_cap != 0 && map->num_buckets != 0 ) {
		load_factor = (float)map->num_buckets / (float)map->bucket_cap;

		if ( load_factor > FMAP_LOAD_FACTOR )
			fmap_grow(map);
	}

	unsigned int hash = abs(map->hashfunc((uint8_t*)key, map->key1_size));
	uint32_t idx = abs(hash) % map->bucket_cap;

	fmap_new_entry(map, key, hash, val, idx);
}

void *fmap_new_entryp(fmap *map, void *key, uint32_t hash, size_t idx) {
	assert(map != NULL && "fmap_new_entry: 'fmap *map' parameter is null");
	assert(key != NULL && "fmap_new_entry: 'void *key' parameter is null");

	size_t **bucket_ptr = (map->buckets + sizeof(size_t) *idx);
	if ( *bucket_ptr == 0  ) {
		struct fmapentry *new_bucket;

		new_bucket = map->allocfunc(map->mapentry_size);
		void *ret = fmap_initialize_fmapentryp(map, new_bucket, hash, key);

		size_t **base_address = map->buckets + (sizeof(size_t) * idx);
		*base_address = (size_t*)new_bucket;
		map->num_buckets++;
		return ret;
	}

	struct fmapentry *entry = (struct fmapentry*)(*bucket_ptr);
	while ( entry->next ) {
		entry = entry->next;
	}

	entry->next = map->allocfunc(map->mapentry_size);
	entry = entry->next;
	return fmap_initialize_fmapentryp(map, entry, hash, key);
}

void *fmap_pushp(fmap *map, void *key) {
	assert(map != NULL && "fmap_push: fmap *map parameter is null");
	assert(key != NULL && "fmap_push: void *key parameter is null");

	float load_factor;

	if ( map->bucket_cap != 0 && map->num_buckets != 0 ) {
		load_factor = (float)map->num_buckets / (float)map->bucket_cap;

		if ( load_factor > FMAP_LOAD_FACTOR )
			fmap_grow(map);
	}

	unsigned int hash = abs(map->hashfunc((uint8_t*)key, map->key1_size));
	uint32_t idx = abs(hash) % map->bucket_cap;

	return fmap_new_entryp(map, key, hash, idx);
}

void *fmap_get(fmap *map, void *key)
{
	assert(map != NULL && "fmap_get: fmap *map parameter is null");
	assert(key != NULL && "fmap_get: void *key parameter is null");

	/* TODO: Check is requested key if out bounds */

	unsigned int hash = abs(map->hashfunc((uint8_t*)key, map->key1_size));
	uint32_t idx = abs(hash) % map->bucket_cap;

	size_t **bucket_ptr = map->buckets + (sizeof(size_t) * idx);
	if (*bucket_ptr == NULL) {
		return NULL;
	}

	struct fmapentry *entry = (struct fmapentry*)(*bucket_ptr);

	do {
		if ( entry->next == NULL ) {
			return (((unsigned char*)(*bucket_ptr)) + sizeof(struct fmapentry) + map->key1_size);
		}

		if ( memcmp(key, fmap_get_key_from_fmapentry(map, entry), map->key1_size) == 0 ) {
			return entry;
		}
		entry = entry->next;
	} while ( entry->next != NULL );

	return NULL;
}

void fmap_free(fmap *map) {
	for ( size_t i = 0; i < map->bucket_cap; i++ ) {
		struct fmapentry *entry;
		size_t **bucket_ptr = map->buckets + (sizeof(size_t) * i);

		entry = (struct fmapentry*)(*bucket_ptr);

		if ( *bucket_ptr == 0 )
			continue;

		do {
			struct fmapentry *tmp = entry;
			entry = entry->next;
			map->freefunc(tmp);
		} while ( entry != NULL  );
	}

	map->freefunc(map->buckets);
}
#endif

#endif /* FMAP_H */
