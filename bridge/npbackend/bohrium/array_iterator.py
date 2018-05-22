import bohrium as bh
from .bhary import fix_biclass_wrapper
#from . import _bh

def index(size=1):
    return bh.zeros(size, dtype=bh.int64)

def is_index_array(idx):
    if isinstance(idx, bh.ndarray):
        return idx.shape == (1,)
    else:
        return False

@fix_biclass_wrapper
def has_index_array(*idx):
    if len(idx) == 1 and isinstance(idx[0], slice):
        return (is_index_array(idx[0].start) or
                is_index_array(idx[0].stop))
    else:
        return is_index_array(idx)


@fix_biclass_wrapper
def dynamic_set_item(a, indices, b):
    c = dynamic_get_item(a, indices)
    c = b
    return c

@fix_biclass_wrapper
def dynamic_get_item(a, indices):
    from . import _bh
    if not isinstance(indices, tuple):
        indices = (indices,)
    b = a

    offset_array = index()
#    offset_array += a.offset
    for dim, idx in enumerate(indices):
        if isinstance(idx, slice):
            offset = idx.start
        else:
            offset = idx
        offset_array += (a.strides[dim]/8) * offset

    shape_array = index(len(indices))
    contains_index_arrays = False
    for dim, idx in enumerate(indices):
        if isinstance(idx, slice):
# #            shape_array[dim] += idx.stop[0] - idx.start[0]
            shape_array[dim:dim+1] += idx.stop - idx.start
        else:
            shape_array[dim] += 1
    _bh.set_start(b, offset_array)
    _bh.set_shape(b, shape_array)

    return b


def dynamic_indices(a, s, dim_array_tuple):
    if not isinstance(s, tuple):
        s = (s,)

    index_arr = index()
    shape_arr = index(len(s))

    b = a

    index_arr += a.offset
    for (dim, array) in dim_array_tuple:
        index_arr += a.strides[dim] * array[0:1]

    for i, ss in enumerate(s):
        if isinstance(ss, slice):
            sl = ss.stop-ss.start
        else:
            sl = 1
        shape_arr[i] = sl

    _bh.set_start(b, index_arr)
    _bh.set_shape(b, shape_arr)
    return b, index_arr


def set_start(a, tp):
    from . import _bh
    lol = index()
    for (d, s) in tp:
#        print((a.strides[d] / 8) * s)
        lol += (a.strides[d] / 8) * s
    _bh.set_start(a, lol)

def set_shape(a, tp):
    from . import _bh
    lol = index(len(tp))
    for i, ss in enumerate(tp):
        lol[i:i+1] += ss

    _bh.set_shape(a, lol)
