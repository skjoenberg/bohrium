import bohrium as bh
import exceptions
from .bhary import fix_biclass_wrapper
from .iterator import dynamic_view_info

def index(size=1):
    return bh.zeros(size, dtype=bh.int64)


@fix_biclass_wrapper
def has_index_array(*idx):
    def is_index_array(idx):
        if isinstance(idx, bh.ndarray):
            return idx.shape == (1,)
        else:
            return False

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
        if isinstance(idx, slice):
            offset = idx.start
        else:
            offset = idx
        offset_array += (a.strides[dim]/8) * offset

    has_slice = reduce(lambda x,y: x or y, [isinstance(idx, slice) for idx in indices])
    has_slice = has_slice or len(indices) < a.ndim
    new_indices = ()
    shape_array = index(len(indices))
    contains_index_arrays = False
    for dim, idx in enumerate(indices):
        if isinstance(idx, slice):
            if idx.start:
                start = idx.start
            else:
                start = 0

            if idx.stop:
                stop = idx.stop[0:1]
            else:
                stop = a.shape[dim:dim+1]-1

            shape_array[dim:dim+1] += stop - start
        else:
            shape_array[dim:dim+1] += 1

    slices = ()
    for _ in range(len(indices)):
        slices = slices + (slice(0,1),)

    b = a[slices]
    _bh.set_start(b, offset_array)
    _bh.set_shape(b, shape_array)
    return b

def set_start(a, offset_array):
    _bh.set_start(a, offset_array)

def set_shape(a, shape_array):
    _bh.set_shape(a, shape_array)

def set_stride(a, stride_array):
    _bh.set_stride(a, stride_array)
