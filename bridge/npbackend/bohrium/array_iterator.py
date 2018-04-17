from . import _bh

def array_iterator(a, s, dim_array_tuple):
    b = a[s]

    for (dim, array) in dim_array_tuple:
        _bh.set_start(a, b, array, dim)
    return b

def index():
    return bh.zeros(1, dtype=bh.int64)
