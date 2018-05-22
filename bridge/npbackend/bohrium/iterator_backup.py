from . import _bh
import copy





class iterator(int):
    '''Iterator integer used for sliding views within loops.'''

    def __new__(cls, value, *args, **kwargs):
        it = super(iterator, cls).__new__(cls, 0)
        setattr(it, "step", 1)
        setattr(it, "offset", value)
        setattr(it, "max_iter", 0)
        return it

    def __add__(self, other):
        new_it = copy.copy(self)
        setattr(new_it, "offset", self.offset+other)
        return new_it

    def __radd__(self, other):
        new_it = copy.copy(self)
        setattr(new_it, "offset", self.offset+other)
        return new_it

    def __sub__(self, other):
        new_it = copy.copy(self)
        setattr(new_it, "offset", self.offset-other)
        return new_it

    def __rsub__(self, other):
        new_it = copy.copy(self)
        setattr(new_it, "offset", self.offset-other)
        return new_it

    def __mul__(self, other):
        new_it = copy.copy(self)
        setattr(new_it, 'step', self.step * other)
        return new_it

    def __rmul__(self, other):
        new_it = copy.copy(self)
        setattr(new_it, 'step', self.step * other)
        return new_it


class IteratorOutOfBounds(Exception):
    pass


class ViewShape(Exception):
    pass


def get_iterator(max_iter, val):
    '''Returns an iterator with a given starting value and maxium iteration'''
    it = iterator(val)
    setattr(it, 'max_iter', max_iter)
    return it


def slide_view(a, dim_stride_tuples):
    """Creates a dynamic view within a loop, that updates the given dimensions by the given strides at the end of each iteration.

    Parameters
    ----------
    a : array view
        A view into an array
    dim_stride_tuples: (int, int)[]
        A list of (dimension, stride) pairs. For each of these pairs, the dimension is updated by the stride in each iteration of a loop.
    """

    # Allocate a new view
    b = a

    # Set the relevant update conditions for the new view
    for (dim, stride) in dim_stride_tuples:
        _bh.slide_view(b, dim, stride)
    return itarray(b)


def hihi(*args):
    if isinstance(args, _bh.ndarray):
        return itarray(args)
    else:
        return args


class Wrapper(object):
    '''A wrapper class that proxies another instance. Used for itarray having a proxy to ndarray.'''

    __wraps__  = None
    __ignore__ = "class mro new init setattr getattr getattribute getslice"

    def __init__(self, ndarray):
        self._ndarray = ndarray

    # proxy for attributes
    def __getattr__(self, name):
        return getattr(self._ndarray, name)

    # proxy for double-underscore attributes
    class __metaclass__(type):
        def __init__(cls, name, bases, dct):
            def make_proxy(name):
                def proxy(self, *args):
                    # A very hacky way of converting itarrays to bohrium.ndarrays
                    # Potentially breaks stuff
                    func = getattr(self._ndarray, name)
                    # def wrapz(*argz):
                    #     new_argz = ()
                    #     for i in argz:
                    #         if isinstance(i, itarray):
                    #         return func
                        # if not argz:
                        #     return func()
                        # if isinstance(argz[0], itarray):
                        #     lol = func(*argz._ndarray)
                        #     if isinstance(lol, _bh.ndarray):
                        #         return itarray(lol)
                        #     else:
                        #         return lol
                        # else:
                        #     print("HAAHAHAH: " + str(argz))

                        #     lol = func(*argz)
                        #     print("ehm....")
                        #     if isinstance(lol, _bh.ndarray):
                        #         return itarray(lol)
                        #     else:
                        #         return lol
#                    return wrapz
                    return func
                return proxy

            type.__init__(cls, name, bases, dct)
            if cls.__wraps__:
                ignore = set("__%s__" % n for n in cls.__ignore__.split())
                for name in dir(cls.__wraps__):
                    if name.startswith("__"):
                        if name not in ignore and name not in dct:
                            setattr(cls, name, property(make_proxy(name)))


class itarray(Wrapper):
    '''An array wrapper that allows using iterators in views during loops iterations'''

    # Wraps ndarray from bohrium
    __wraps__ = _bh.ndarray

    # def __iadd__(self, a):
    #     if isinstance(a, itarray):
    #         a = a._ndarray
    #     return self._ndarray.__iadd__(a)


    # def __imul__(self, a):
    #     if isinstance(a, itarray):
    #         a = a._ndarray
    #     return self._ndarray.__iadd__(a)


    # def __mul__(self, a):
    #     if isinstance(a, itarray):
    #         a = a._ndarray
    #     return itarray(self._ndarray.__mul__(a))


    def __setitem__(self, sliced, b):
        '''Method for setting values. Handles the special case
           with assigning to an array with a single index.'''
        if isinstance(sliced, slice):
            self._ndarray.__setitem__(sliced, b)
        else:
            self._ndarray.__setitem__(slice(sliced, sliced+1), b)


    def __getitem__(self, sliced):
        '''Method for handling getting a single item or a single
           with multiple dimesions and/or step size.'''

        def has_iterator(s):
            '''Checks whether a (multidimensional) slice contains an iterator'''

            # Helper function for one-dimensional slices
            def simple_it(ss):
                if isinstance(ss, slice):
                    # Checks whether the view changes shape during iterations
                    if isinstance(ss.start, iterator) and \
                       isinstance(ss.stop, iterator):
                        if sliced.start.step != sliced.stop.step:
                            raise ViewShape(\
                                "\n    View must not change shape between iterations:\n" \
                                "    Stride of view start is %d, stride of view end is %d." \
                                            % (sliced.start.step, sliced.stop.step))
                        else: return True
                    else: return False
                else:
                    return isinstance(ss, iterator)
            # Checking single or multidimensional slices for iterators
            if isinstance(s, tuple):
                for t in s:
                    it = simple_it(t)
                    if it: return it
                return False
            else:
                return simple_it(s)


        def check_bounds(shape, dim, s):
            '''Checks whether the view is within the bounds of the array,
               given the maximum number of iterations'''
            last_index = s.offset + s.step * s.max_iter
            if 0 <= last_index <= shape[dim]:
                return True
            else:
                raise IteratorOutOfBounds( \
                        "\n    Iterator out of bounds:\n" \
                        "    Dimension %d has length %d, iterator goes to %d." \
                        % (dim, shape[dim], last_index))

        # Check whether the view contains an iterator
        if has_iterator(sliced):
            # A single iterator (eg. a[i])
            if isinstance(sliced, iterator):
                # Check whether the iterator stays within the array
                check_bounds(self._ndarray.shape, 0, sliced)
                return slide_view(self._ndarray[slice(sliced.offset,sliced.offset+1)], [(0, sliced.step)])
            # A slice with optional step size (eg. a[i:i+2] or a[i:i+2:2])
            if isinstance(sliced, slice):
                if sliced.step:
                    # Set the correct step size
                    setattr(sliced.start.step, "step", sliced.start.step*sliced.step)
                    setattr(sliced.stop.step, "step", sliced.stop.step*sliced.step)
                # Check whether the iterator stays within the array
                check_bounds(self._ndarray.shape, 0, sliced.start)
                check_bounds(self._ndarray.shape, 0, sliced.stop)
                return slide_view(
                    self._ndarray[sliced.start.offset:sliced.stop.offset],
                    [(0, sliced.start.step)])
            # A multidimension slice with optional step size
            # (eg. a[i:i+1, i+2:i+5] or a[i:i+1:2, i+2:i+5:4])
            elif isinstance(sliced, tuple):
                new_slices = ()
                slides = []
                for i, s in enumerate(sliced):
                    if has_iterator(s):
                        if isinstance(s, slice):
                            if s.step:
                                # Set the correct step size
                                setattr(s.start.step, "step", s.start.step*s.step)
                                setattr(s.stop.step, "step", s.stop.step*s.step)
                                # Check whether the iterator stays within the array
                                check_bounds(self._ndarray.shape, i, s.start)
                            check_bounds(self._ndarray.shape, i, s.stop)
                            new_slices += (slice(s.start.offset, s.stop.offset),)
                            slides.append((i, s.start.step))
                        else:
                            # Check whether the iterator stays within the array
                            check_bounds(self._ndarray.shape, 0, s)

                            new_slices += (slice(s.offset, s.offset+1),)
                            slides.append((i, s.step))
                    else:
                        new_slices += (s,)
                return slide_view(self._ndarray[new_slices], slides)
            else:
                raise Exception("Malformed view")
        # The view does not contain an iterator
        else:
            if isinstance(sliced, int):
#                return self._ndarray[sliced:sliced+1]
                return itarray(self._ndarray[sliced:sliced+1])
            else:
                return itarray(self._ndarray[sliced])
