#include "xs.h"

static void xs_allocate_data(xs *x, size_t len, bool reallocate)
{
    size_t n = 1 << x->capacity;

    /* Medium string */
    if (len < LARGE_STRING_LEN) {
        x->ptr = reallocate ? krealloc(x->ptr, n, GFP_KERNEL)
                            : kmalloc(n, GFP_KERNEL);
        return;
    }

    /*
     * Large string
     */
    x->is_large_string = 1;

    /* The extra 4 bytes are used to store the reference count */
    x->ptr = reallocate ? krealloc(x->ptr, n + 4, GFP_KERNEL)
                        : kmalloc(n + 4, GFP_KERNEL);

    xs_set_ref_count(x, 1);
}

xs *xs_new(xs *x, const void *p)
{
    *x = xs_literal_empty();
    size_t len = strlen(p) + 1;
    if (len > 16) {
        x->capacity = ilog2(len) + 1;
        x->size = len - 1;
        x->is_ptr = true;
        xs_allocate_data(x, x->size, 0);
        memcpy(xs_data(x), p, len);
    } else {
        memcpy(x->data, p, len);
        x->space_left = 15 - (len - 1);
    }
    return x;
}
