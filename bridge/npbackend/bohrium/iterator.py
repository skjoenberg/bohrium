import copy

from .bhary import fix_biclass_wrapper
from . import array_create

class iterator(object):
    '''Iterator used for sliding views within loops.

    Notes
    -----
    Supports addition, subtraction and multiplication.
    '''

    def __init__(self, value):
        self.step = 1
        self.offset = value
        self.max_iter = 0

    def __add__(self, other):
        new_it = copy.copy(self)
        new_it.offset += other
        return new_it

    def __radd__(self, other):
        new_it = copy.copy(self)
        new_it.offset += other
        return new_it

    def __sub__(self, other):
        new_it = copy.copy(self)
        new_it.offset -= other
        return new_it

    def __rsub__(self, other):
        new_it = copy.copy(self)
        new_it.step *= -1
        new_it.offset = other - new_it.offset
        return new_it

    def __mul__(self, other):
        new_it = copy.copy(self)
        new_it.offset *= other
        new_it.step *= other
        return new_it

    def __rmul__(self, other):
        new_it = copy.copy(self)
        new_it.offset *= other
        new_it.step *= other
        return new_it

    def __neg__(self):
        new_it = copy.copy(self)
        new_it.step *= -1
        new_it.offset *= -1
        return new_it

class IteratorOutOfBounds(Exception):
    '''Exception thrown when a view goes out of bounds after the maximum
       iterations.'''
    def __init__(self, dim, shape, first_index, last_index):
        error_msg = "\n    Iterator out of bounds:\n" \
                    "     Dimension %d has length %d, iterator starts from %d and goes to %d." \
                    % (dim, shape, first_index, last_index)
        super(IteratorOutOfBounds, self).__init__(error_msg)


class ViewShape(Exception):
    '''Exception thrown when a view changes shape between iterations in a
       loop.'''
    def __init__(self, start,stop):
        error_msg = "\n    View must not change shape between iterations:\n" \
                    "    Stride of view start is %d, stride of view end is %d." \
                    % (start, stop)
        super(ViewShape, self).__init__(error_msg)


class dynamic_view_info(object):
    '''Object for storing information about dynamic changes to the view'''
    def __init__(self, dc, sh, st):
        self.dynamic_changes = dc
        self.shape = sh
        self.stride = st


def inherit_dynamic_changes(a, sliced):
    dvi = a.bhc_dynamic_view_info
    a.bhc_dynamic_view_info = None
    b = a[sliced]
    b.bhc_dynamic_view_info = dvi
    a.bhc_dynamic_view_info = dvi
    return b


def get_iterator(max_iter, val):
    '''Returns an iterator with a given starting value. An iterator behaves like
       an integer, but is used to slide view between loop iterations.

    Parameters
    ----------
    max_iter : int
        The maximum amount of iterations of the loop. Used for checking
        boundaries.
    val : int
        The initial value of the iterator.

    Notes
    -----
    `get_iterator` can only be used within a bohrium loop function. Within the
    loop `max_iter` is set by a lambda function. This is also the case in the
    example.

    Examples
    --------
    >>> def kernel(a):
    ...     i = get_iterator(1)
    ...     a[i] *= a[i-1]
    >>> a = bh.arange(1,6)
    >>> bh.do_while(kernel, 4, a)
    array([1, 2, 6, 24, 120])'''

    it = iterator(val)
    it.max_iter = max_iter
    return it


@fix_biclass_wrapper
def has_iterator(*s):
    '''Checks whether a (multidimensional) slice contains an iterator

    Parameters
    ----------
    s : pointer to an integer, iterator or a tuple of integers/iterators

    Notes
    -----
    Only called from __getitem__ in bohrium arrays (see _bh.c) and .'''

    # Helper function for one-dimensional slices
    def check_simple_type(ss):
        if isinstance(ss, slice):
            # Checks whether the view changes shape during iterations
            return isinstance(ss.start, iterator) or \
                   isinstance(ss.stop, iterator)
        else:
            return isinstance(ss, iterator)

    # Checking single or multidimensional slices for iterators
    if isinstance(s, tuple):
        for t in s:
            it = check_simple_type(t)
            if it:
                return it
        return False
    else:
        return check_simple_type(s)


@fix_biclass_wrapper
def slide_from_view(a, sliced):
    def check_bounds(shape, dim, s):
        '''Checks whether the view is within the bounds of the array,
        given the maximum number of iterations'''
        last_index = s.offset + s.step * (s.max_iter-1)
        if -shape[dim] <= s.offset   < shape[dim] and \
           -shape[dim] <= last_index < shape[dim]:
            return True
        else:
            raise IteratorOutOfBounds(dim, shape[dim], s.offset, last_index)


    def check_shape(s):
        '''Checks whether the view changes shape between iterations.'''
        if isinstance(s.start, iterator) and \
           isinstance(s.stop, iterator):
            if s.start.step != s.stop.step:
                raise ViewShape(s.start.step, s.stop.step)
        elif isinstance(s.start, iterator):
            raise ViewShape(s.start.step, 0)
        elif isinstance(s.stop, iterator):
            raise ViewShape(0, s.stop.step)
        return True


    def get_shape(s):
        '''Checks whether the view changes shape between iterations.'''
        if isinstance(s.start, iterator):
            start_step = s.start.step
        else:
            start_step = 0

        if isinstance(s.stop, iterator):
            stop_step = s.stop.step
        else:
            stop_step = 0
        return stop_step - start_step


    if not isinstance(sliced, tuple):
        sliced = (sliced,)

    new_slices = ()
    slides = []
    has_slices = reduce((lambda x, y: x or y), [isinstance(s, slice) for s in sliced])

    for i, s in enumerate(sliced):
        if len(sliced) == 1 or has_iterator(s):
            # A slice with optional step size (eg. a[i:i+2] or a[i:i+2:2])
            if isinstance(s, slice):
                #check_shape(s)

                # Check whether the start/end iterator stays within the array
                if isinstance(s.start, iterator):
                    check_bounds(a.shape, i, s.start)
                    start = s.start.offset
                    step = s.start.step
                else:
                    start = s.start
                    step = 0
                if isinstance(s.stop, iterator):
                    check_bounds(a.shape, i, s.stop-1)
                    stop = s.stop.offset
                else:
                    stop = s.stop

                # Store the new slice
                new_slices += (slice(start, stop, s.step),)
                slides.append((i, step, get_shape(s)))

            # A single iterator (eg. a[i])
            else:
                # Check whether the iterator stays within the array
                check_bounds(a.shape, i, s)
                if not has_slices:
                    new_slices += (slice(s.offset, s.offset+1),)
                else:
                    new_slices += (s.offset,)
#                if s.offset == -1:
#                    new_slices += (slice(s.offset, None),)
                slides.append((i, s.step, 0))
        else:
            # Does not contain an iterator, just pass it through
            new_slices += (s,)


    b = a[new_slices]

    a_dvi = a.bhc_dynamic_view_info
    if a_dvi:
        o_shape = a_dvi.shape
        o_stride = a_dvi.stride
        o_dynamic_changes = a_dvi.dynamic_changes
        new_stride = [(b.strides[i] / a.strides[i]) for i in range(b.ndim)]
        new_slides = []
        for (b_dim, b_slide, b_shape) in slides:
            for (a_dim, a_slide, a_shape) in o_dynamic_changes:
                if a_dim == b_dim:
                    parent_stride = a.strides[a_dim] / o_stride[a_dim]
                    b_slide = a_slide + b_slide * parent_stride

            new_slides.append((b_dim, b_slide, b_shape))

        dvi = dynamic_view_info(new_slides, o_shape, o_stride)
    else:
        dvi = dynamic_view_info(slides, a.shape, a.strides)

    b.bhc_dynamic_view_info = dvi
    return b

def add_slide_info(a):
    """Checks whether a view is dynamic and adds the relevant
       information to the view structure within BXX if it is.

    Parameters
    ----------
    a : array view
        A view into an array
    """
    from . import _bh

    # Check whether the view is a dynamic view
    dvi = a.bhc_dynamic_view_info
    if dvi:
        # Set the relevant update conditions for the new view
        for (dim, slide, shape_change) in dvi.dynamic_changes:
            # Stride is bytes, so it has to be diveded by 8
            stride = dvi.stride[dim]/8
            shape = dvi.shape[dim]

            # Add information
            _bh.slide_view(a, dim, slide, shape_change, shape, stride)
