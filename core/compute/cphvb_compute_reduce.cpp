/*
This file is part of cphVB and copyright (c) 2012 the cphVB team:
http://cphvb.bitbucket.org

cphVB is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as 
published by the Free Software Foundation, either version 3 
of the License, or (at your option) any later version.

cphVB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the 
GNU Lesser General Public License along with cphVB. 

If not, see <http://www.gnu.org/licenses/>.
*/
#include <cphvb.h>
#include <cphvb_compute.h>

/**
 * cphvb_compute_reduce
 *
 * Implementation of the user-defined funtion "reduce".
 * Note that we follow the function signature defined by cphvb_userfunc_impl.
 *
 */
cphvb_error cphvb_compute_reduce(cphvb_userfunc *arg, void* ve_arg)
{
    cphvb_reduce_type *a = (cphvb_reduce_type *) arg;
    cphvb_instruction inst;
    cphvb_error err;
    cphvb_intp i, j, step, axis_size;
    cphvb_array *out, *in, tmp;

    if (cphvb_operands(a->opcode) != 3) {
        fprintf(stderr, "Reduce only support binary operations.\n");
        return CPHVB_ERROR;
    }

	if (cphvb_base_array(a->operand[1])->data == NULL) {
        fprintf(stderr, "Reduce called with input set to null.\n");
        return CPHVB_ERROR;
	}
    
    // Make sure that the array memory is allocated.
    if (cphvb_data_malloc(a->operand[0]) != CPHVB_SUCCESS) {
        return CPHVB_OUT_OF_MEMORY;
    }
    
    out = a->operand[0];
    in  = a->operand[1];
    
	if (in->ndim == 1)
    {
	    cphvb_index shape[CPHVB_MAXDIM];
	    cphvb_index stride[CPHVB_MAXDIM];

		// Create a new view with a single element, mapped to the output
	    shape[0] = in->shape[0]-1;
	    stride[0] = 0;
	    cphvb_array *tt;
    	cphvb_create_array(cphvb_base_array(out), out->type, 1, out->start, shape, stride, &tt);

		// Create a new view hides the first element
	    cphvb_array *tt2;
		shape[0] = in->shape[0]-1;
		stride[0] = in->stride[0];
    	cphvb_create_array(cphvb_base_array(in), in->type, 1, in->start+in->stride[0], shape, stride, &tt2);
    	
    	// Copy the first element to the output
    	memcpy((char*)cphvb_base_array(out)->data + out->start, (char*)cphvb_base_array(in)->data + in->start * cphvb_type_size(in->type), cphvb_type_size(in->type));

		// Prepare a instruction for execution
		inst.status = CPHVB_INST_PENDING;
		inst.opcode = a->opcode;
		inst.operand[0] = tt;
		inst.operand[1] = tt2;
		inst.operand[2] = tt;
    	
    	// Apply the operation
		err = cphvb_compute_apply( &inst );

		// Cleanup
    	cphvb_destroy_array(tt);
    	cphvb_destroy_array(tt2);

        if (err != CPHVB_SUCCESS)
            return err;    	
    	
    	return CPHVB_SUCCESS;
    }
    
    // WARN: This can create a ndim = 0 it seems to work though...
    tmp         = *in;                          // Copy the input-array meta-data
    tmp.base    = cphvb_base_array(in);
    tmp.start = in->start;

    step = in->stride[a->axis];
    j=0;
    for(i=0; i<in->ndim; ++i) {                 // Remove the 'axis' dimension from in
        if(i != a->axis) {
            tmp.shape[j]    = in->shape[i];
            tmp.stride[j]   = in->stride[i];
            ++j;
        }
    }
    --tmp.ndim;
    
    inst.status = CPHVB_INST_PENDING;           // We copy the first element to the output.
    inst.opcode = CPHVB_IDENTITY;
    inst.operand[0] = out;
    inst.operand[1] = &tmp;
    inst.operand[2] = NULL;

    err = cphvb_compute_apply( &inst );         // execute the pseudo-instruction
    if (err != CPHVB_SUCCESS) {
        return err;
    }
    tmp.start += step;

    inst.status = CPHVB_INST_PENDING;           // Reduce over the 'axis' dimension.
    inst.opcode = a->opcode;                    // NB: the first element is already handled.
    inst.operand[0] = out;
    inst.operand[1] = out;
    inst.operand[2] = &tmp;
    
    axis_size = in->shape[a->axis];

    for(i=1; i<axis_size; ++i) {
        err = cphvb_compute_apply( &inst );
        if (err != CPHVB_SUCCESS) {
            return err;
        }
        tmp.start += step;
    }

    return CPHVB_SUCCESS;
}
