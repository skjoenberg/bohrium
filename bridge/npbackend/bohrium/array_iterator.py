from . import _bh

def array_iterator(a, s, dim_array_tuple):
    b = a[s]
#    _bh.sync(b)

    for (dim, array) in dim_array_tuple:
#        _bh.sync(array)
        _bh.set_start(a, b, array, dim)
    return b

def index(value=0):
    return bh.array(value, dtype=bh.int64)
