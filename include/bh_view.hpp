/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include <algorithm>
#include <stdbool.h>
#include <iostream>
#include <map>
#include <vector>
#include <cstring>
#include <cassert>
#include <tuple>
#include "bh_type.hpp"
#include "bh_base.hpp"
#include <bh_constant.hpp>
#include <boost/serialization/split_member.hpp>
#include <unordered_map>

// Forward declaration of class boost::serialization::access
namespace boost { namespace serialization { class access; }}

constexpr int64_t BH_MAXDIM = 16;

//Implements pprint of base arrays
std::ostream &operator<<(std::ostream &out, const bh_base &b);

struct bh_view {

    // Start pointer
    bh_base* start_pointer = nullptr;
    // Shape pointer
    bh_base* shape_pointer = nullptr;
    // Stride pointer
    bh_base* stride_pointer = nullptr;

    bh_view() = default;

    /// Copy Constructor
    bh_view(const bh_view &view);

    /// Create a view that represents the whole of `base`
    explicit bh_view(bh_base &base);

    /// Pointer to the base array.
    bh_base *base;

    /// Index of the start element
    int64_t start;

    /// Number of dimensions
    int64_t ndim;

    /// Number of elements in each dimensions
    int64_t shape[BH_MAXDIM];

    /// The stride for each dimensions
    int64_t stride[BH_MAXDIM];

    // Information used for dynamic views within internal loops

    // Dimensions to be slided each loop iterations
    std::vector<int64_t> slide;

    // The relevant dimension
    std::vector<int64_t> slide_dim;


    // The step delay in the dimension
    std::vector<int64_t> slide_dim_step_delay;
    std::vector<int64_t> slide_dim_step_delay_counter;

    int64_t iteration_counter = 0;

    // The change to the shape
    std::vector<int64_t> slide_dim_shape_change;

    // The strides these dimensions is slided each dynamically
    std::vector<int64_t> slide_dim_stride;

    // The shape of the given dimension (used for negative indices)
    std::vector<int64_t> slide_dim_shape;

    // The amount the iterator can reach, before resetting it
    std::unordered_map<int64_t, int64_t> resets;
    std::unordered_map<int64_t, int64_t> changes_since_reset;
    //    std::vector<int64_t> reset_max;

    // The dimension to reset
    std::vector<int64_t> reset_dim;

    // The dimension to reset
    int64_t reset_counter = 0;

    // Returns a vector of tuples that describe the view using (almost)
    // Python Notation.
    // NB: in this notation the stride is always absolute eg. [2:4:3, 0:3:1]
    std::vector<std::tuple<int64_t, int64_t, int64_t> > python_notation() const;

    // Returns a pretty print of this view (as a string)
    std::string pprint(bool python_notation = true) const;

    // Insert a new dimension at 'dim' with the size of 'size' and stride of 'stride'
    // NB: 0 <= 'dim' <= ndim
    void insert_axis(int64_t dim, int64_t size, int64_t stride);

    // Remove the axis 'dim'
    void remove_axis(int64_t dim);

    // Transposes by swapping the two axes 'axis1' and 'axis2'
    void transpose(int64_t axis1, int64_t axis2);

    bool operator<(const bh_view &other) const {
        if (base < other.base) return true;
        if (other.base < base) return false;
        if (start < other.start) return true;
        if (other.start < start) return false;
        if (ndim < other.ndim) return true;
        if (other.ndim < ndim) return false;

        for (int64_t i = 0; i < ndim; ++i) {
            if (shape[i] < other.shape[i]) return true;
            if (other.shape[i] < shape[i]) return false;
        }

        for (int64_t i = 0; i < ndim; ++i) {
            if (stride[i] < other.stride[i]) return true;
            if (other.stride[i] < stride[i]) return false;
        }

        return false;
    }

    bool operator==(const bh_view &other) const {
        if (base == nullptr or this->base == nullptr) return false;
        if (base != other.base) return false;
        if (ndim != other.ndim) return false;
        if (start != other.start) return false;

        for (int64_t i = 0; i < ndim; ++i) {
            if (shape[i] != other.shape[i]) {
                return false;
            }
        }

        for (int64_t i = 0; i < ndim; ++i) {
            if (stride[i] != other.stride[i]) {
                return false;
            }
        }

        return true;
    }

    bool operator!=(const bh_view &other) const {
        return !(*this == other);
    }

    template<class Archive>
    void save(Archive &ar, const unsigned int version) const {
        size_t tmp = (size_t) base;
        ar << tmp;
        if (base != nullptr) { // This view is NOT a constant
            ar << start;
            ar << ndim;
            for (int64_t i = 0; i < ndim; ++i) {
                ar << shape[i];
                ar << stride[i];
            }
            ar << slide;
            ar << slide_dim;
            ar << slide_dim_stride;
            ar << slide_dim_shape;
            ar << start_pointer;
            ar << shape_pointer;
            ar << stride_pointer;
        }
    }

    template<class Archive>
    void load(Archive &ar, const unsigned int version) {
        size_t tmp;
        ar >> tmp;
        base = (bh_base *) tmp;
        if (base != nullptr) { // This view is NOT a constant
            ar >> start;
            ar >> ndim;
            for (int64_t i = 0; i < ndim; ++i) {
                ar >> shape[i];
                ar >> stride[i];
            }
            ar >> slide;
            ar >> slide_dim;
            ar >> slide_dim_stride;
            ar >> slide_dim_shape;
            ar >> start_pointer;
            ar >> shape_pointer;
            ar >> stride_pointer;
        }
    }

    bool uses_pointer() {
        return (this->start_pointer != nullptr)
            || (this->shape_pointer != nullptr)
            || (this->stride_pointer != nullptr);
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

//Implements pprint of views
std::ostream &operator<<(std::ostream &out, const bh_view &v);

/* Returns the simplest view (fewest dimensions) that access
 * the same elements in the same pattern
 *
 * @view The view
 * @return The simplified view
 */
bh_view bh_view_simplify(const bh_view &view);

/* Simplifies the given view down to the given shape.
 * If that is not possible an std::invalid_argument exception is thrown
 *
 * @view The view
 * @return The simplified view
 */
bh_view bh_view_simplify(const bh_view &view, const std::vector<int64_t> &shape);

/* Find the base array for a given view
 *
 * @view   The view in question
 * @return The Base array
 */
#define bh_base_array(view) ((view)->base)

/* Number of element in a given shape
 *
 * @ndim     Number of dimentions
 * @shape[]  Number of elements in each dimention.
 * @return   Number of element operations
 */
int64_t bh_nelements(int64_t ndim, const int64_t shape[]);

int64_t bh_nelements(const bh_view &view);

/* Set the view stride to contiguous row-major
 *
 * @view    The view in question
 * @return  The total number of elements in view
 */
int64_t bh_set_contiguous_stride(bh_view *view);

/* Updates the view with the complete base
 *
 * @view    The view to update (in-/out-put)
 * @base    The base assign to the view
 * @return  The total number of elements in view
 */
void bh_assign_complete_base(bh_view *view, bh_base *base);

/* Determines whether the view is a scalar or a broadcasted scalar.
 *
 * @view The view
 * @return The boolean answer
 */
bool bh_is_scalar(const bh_view *view);

/* Determines whether the operand is a constant
 *
 * @o The operand
 * @return The boolean answer
 */
bool bh_is_constant(const bh_view *o);

/* Flag operand as a constant
 *
 * @o The operand
 */
void bh_flag_constant(bh_view *o);

/* Determines whether two views have same shape.
 *
 * @a The first view
 * @b The second view
 * @return The boolean answer
 */
bool bh_view_same_shape(const bh_view *a, const bh_view *b);

/* Determines whether a view is contiguous
 *
 * @a The view
 * @return The boolean answer
 */
bool bh_is_contiguous(const bh_view *a);


/* Determines whether two views access some of the same data points
 * NB: This functions may return True on non-overlapping views.
 *     But will always return False on overlapping views.
 *
 * @a The first view
 * @b The second view
 * @return The boolean answer
 */
bool bh_view_disjoint(const bh_view *a, const bh_view *b);
