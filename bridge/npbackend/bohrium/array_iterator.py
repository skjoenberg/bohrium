import bohrium as bh
import exceptions
from .bhary import fix_biclass_wrapper
from .iterator import dynamic_view_info
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
    for s in idx:
        if isinstance(s, slice):
            if (is_index_array(s.start) or
                is_index_array(s.stop)):
                return True
        else:
            if is_index_array(s):
                return True
    return False


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

    offset_array = index()
    for dim, idx in enumerate(indices):
        try:
            if isinstance(idx, slice):
                offset = idx.start[0:1]
            else:
                offset = idx[0:1]
            offset_array += (a.strides[dim]/8) * offset
        except exceptions.TypeError as e:
            pass

    has_slice = reduce(lambda x,y: x or y, [isinstance(idx, slice) for idx in indices])
    has_slice = has_slice or len(indices) < a.ndim
    new_indices = ()
    shape_array = index(len(indices))
    contains_index_arrays = False
    for dim, idx in enumerate(indices):
        if isinstance(idx, slice):
            print(type(idx.start),type(idx.stop))
            try:
                start = idx.start[0]
            except:
                start = 0

            try:
                stop = idx.stop[0]
            except:
                stop = a.shape[dim]-1

            shape_array[dim:dim+1] += stop - start
#            new_indices = new_indices + (slice(int(start),int(stop)),)
            new_indices = new_indices + (slice(0,1),)
        else:
            shape_array[dim:dim+1] = 1
            if has_slice:
                new_indices = new_indices + (idx,)
            else:
#                new_indices = new_indices + (slice(int(idx),int(idx)+1),)
                new_indices = new_indices + (slice(0,1),)

    if len(new_indices) < a.ndim:
        for _ in range(a.ndim - len(new_indices)):
            new_indices = new_indices + (slice(None, None, None),)

#    b = a.copy()
#    print(shape_array)
    b = a[new_indices]
    dvi = dynamic_view_info([(0,0,0)],a.shape,a.strides)
    dvi.array = True
    b.bhc_dynamic_view_info = dvi
    print("walla")
#    b = a[new_indices]
#    _bh.set_start(b, offset_array)
#    _bh.set_shape(b, shape_array)
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
