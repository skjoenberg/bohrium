"""
The Computation Backend

"""
import bhc
import numpy
from _util import dtype_name, dtype_from_bhc

class base(object):
    """base array handle"""
    def __init__(self, size, dtype, bhc_obj):
        self.size = size
        self.dtype = dtype
        self.bhc_obj = bhc_obj

    def __del__(self):
        del_bhc_obj(self.bhc_obj)

class view(object):
    """array view handle"""
    def __init__(self, ndim, start, shape, stride, base, dtype):
        self.ndim = ndim
        self.start = start
        self.shape = shape
        self.stride = stride
        self.base = base
        self.dtype = dtype

def views2bhc(views):
    """convert views to bhc objects but ignores scalars
    #NB: the returned objects should be deleted after use through call to 'del_bhc_obj()'
    """
    singleton = not (hasattr(views, "__iter__") or
                     hasattr(views, "__getitem__"))
    if singleton:
        views = (views,)
    ret = []
    for v in views:
        if not numpy.isscalar(v):
            dtype = dtype_name(v.dtype)
            exec "base = bhc.bh_multi_array_%s_get_base(v.base.bhc_obj)"%dtype
            f = eval("bhc.bh_multi_array_%s_new_from_view"%dtype)
            v = f(base, v.ndim, v.start, v.shape, v.stride)
        ret.append(v)
    if singleton:
        ret = ret[0]
    return ret

def del_bhc_obj(bhc_objects):
    """delete the bhc objectsi but ignores scalars"""
    singleton = not (hasattr(bhc_objects, "__iter__") or
                     hasattr(bhc_objects, "__getitem__"))
    if singleton:
        bhc_objects = (bhc_objects,)
    for o in bhc_objects:
        if not numpy.isscalar(o):
            exec "bhc.bh_multi_array_%s_destroy(o)"%dtype_from_bhc(o)

def bhc_exec(func, *args):
    """convert 'args' to bhc objects, execute the 'func', and cleanup the bhc objects"""
    args = list(args)
    tmp = []
    for i in xrange(len(args)):
        if isinstance(args[i], view):
            args[i] = views2bhc(args[i])
            tmp.append(args[i])
    ret = func(*args)
    for o in tmp:
        del_bhc_obj(o)
    return ret

def new_empty(size, dtype):
    """Return a new empty base array"""
    f = eval("bhc.bh_multi_array_%s_new_empty"%dtype_name(dtype))
    obj = bhc_exec(f, 1, (size,))
    return base(size, dtype, obj)

def new_view(ndim, start, shape, stride, base, dtype):
    """Return a new view that points to 'base'"""
    return view(ndim, start, shape, stride, base, dtype)

def get_data_pointer(ary, allocate=False, nullify=False):
    dtype = dtype_name(ary)
    ary = views2bhc(ary)
    exec "bhc.bh_multi_array_%s_sync(ary)"%dtype
    exec "bhc.bh_multi_array_%s_discard(ary)"%dtype
    exec "bhc.bh_runtime_flush()"
    exec "base = bhc.bh_multi_array_%s_get_base(ary)"%dtype
    exec "data = bhc.bh_multi_array_%s_get_base_data(base)"%dtype
    del_bhc_obj(ary)
    if data is None:
        if not allocate:
            return 0
        exec "data = bhc.bh_multi_array_%s_get_base_data_and_force_alloc(base)"%dtype
        if data is None:
            raise MemoryError()
    if nullify:
        exec "bhc.bh_multi_array_%s_nullify_base_data(base)"%dtype
    return int(data)

def ufunc(op, *args):
    """Apply the 'op' on args, which is the output followed by one or two inputs"""
    scalar_str = ""
    in_dtype = dtype_name(args[1])
    for i,a in enumerate(args):
        if numpy.isscalar(a):
            if i == 1:
                scalar_str = "_scalar" + ("_lhs" if len(args) > 2 else "")
            if i == 2:
                scalar_str = "_scalar" + ("_rhs" if len(args) > 2 else "")
        elif i > 0:
            in_dtype = a.dtype#overwrite with a non-scalar input

    if op.info['name'] == "identity":#Identity is a special case
        cmd = "bhc.bh_multi_array_%s_identity_%s"%(dtype_name(args[0].dtype), dtype_name(in_dtype))
    else:
        cmd = "bhc.bh_multi_array_%s_%s"%(dtype_name(in_dtype), op.info['name'])
    cmd += scalar_str
    bhc_exec(eval(cmd), *args)

def reduce(op, out, a, axis):
    """reduce 'axis' dimension of 'a' and write the result to out"""

    f = eval("bhc.bh_multi_array_%s_%s_reduce"%(dtype_name(a), op.info['name']))
    bhc_exec(f, out, a, axis)

def accumulate(op, out, a, axis):
    """accumulate 'axis' dimension of 'a' and write the result to out"""

    f = eval("bhc.bh_multi_array_%s_%s_accumulate"%(dtype_name(a), op.info['name']))
    bhc_exec(f, out, a, axis)

def extmethod(name, out, in1, in2):
    """Apply the extended method 'name' """

    f = eval("bhc.bh_multi_array_extmethod_%s_%s_%s"%(dtype_name(out),\
              dtype_name(in1), dtype_name(in2)))
    ret = bhc_exec(f, name, out, in1, in2)
    if ret != 0:
        raise RuntimeError("The current runtime system does not support "
                           "the extension method '%s'"%name)

def range(size, dtype):
    """create a new array containing the values [0:size["""

    f = eval("bhc.bh_multi_array_%s_new_range"%dtype_name(dtype))
    return bhc_exec(f, size)

def random123(size, start_index, key):
    """Create a new random array using the random123 algorithm.
    The dtype is uint64 always."""

    f = eval("bhc.bh_multi_array_uint64_new_random123")
    return bhc_exec(f, totalsize, start_index, key)

