# fmap

fmap is single header generic hashmap implementation without any macros, allows custom allocators and free functions.
Note that fmap is not thread safe, you have to wrap it around your own multithreading api.

Basic usage:
```c
/* Initialize fmap with the following function: */
fmap_init(fmap *map, uint32_t key1_size, uint32_t key2_size, uint32_t (*hashfunc) (uint8_t *key, size_t len)

/* If you want to use a custom allocator you can use the following function to change the allocator */
fmap_set_allocation_funcs(fmap *map, void * (*allocfunc) (size_t size), void (*freefunc) (void *ptr))

/* To push a key-value pair simple use: */
/* And give the pointer address of the variables to be used */
fmap_push(fmap *map, void *key, void *val)

/* The above method is great but is limited. What if you have a struct
 * that you do not want to stack initialize and copy it to the fmap?
 * A good way to fix this is to return a pointer to the value, so whenever
 * someone uses a function like InitializeStruct(struct example_struct *a)
 * they can simply pass the pointer returned from the function fmap_pushp
 * to InitializeStruct. This is what fmap_pushp does.

 * The function is decleared as following: */

void *fmap_pushp(fmap *map, void *key);
/* Returns a pointer that can be written to. Be carefull
 * to not overwrite bytes to the pointer! */

/* To free the fmap and all of it's allocations use: */
void fmap_free(fmap *map)
```
