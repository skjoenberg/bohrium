#include <cinttypes>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <omp.h>
#include "accelerator.hpp"

using namespace std;

namespace bohrium{
namespace engine{
namespace cpu{

const char Accelerator::TAG[] = "Accelerator";

#if defined(VE_CPU_WITH_INTEL_LEO)
Accelerator::Accelerator(int id) : id_(id), bytes_allocated_(0) {};

template <typename T>
void Accelerator::_alloc(operand_t& operand)
{
    DEBUG(TAG, "_alloc bh_base("<< (void*)(operand.base) << ") bh_base.data(" << operand.base->data << ") operand.data(" << operand.data << ")");
    if (allocated(operand)) {   //  Operand is already allocated
        DEBUG(TAG, "_alloc skipping");
        return;
    }

    T* data = (T*)(*operand.data);
    const int nelem = operand.nelem;
    bytes_allocated_ += nelem*sizeof(T);
    bases_.insert(operand.base);

    #pragma offload_transfer                                    \
            target(mic:id_)                                     \
            in(data:length(nelem) alloc_if(1) free_if(0))
}

void Accelerator::alloc(operand_t& operand)
{
    switch(operand.etype) {
        case BOOL:      _alloc<unsigned char>(operand); break;
        case INT8:      _alloc<int8_t>(operand); break;
        case INT16:     _alloc<int16_t>(operand); break;
        case INT32:     _alloc<int32_t>(operand); break;
        case INT64:     _alloc<int64_t>(operand); break;
        case UINT8:     _alloc<uint8_t>(operand); break;
        case UINT16:    _alloc<uint16_t>(operand); break;
        case UINT32:    _alloc<uint32_t>(operand); break;
        case UINT64:    _alloc<uint64_t>(operand); break;
        case FLOAT32:   _alloc<float>(operand); break;
        case FLOAT64:   _alloc<double>(operand); break;

        case COMPLEX64:
        case COMPLEX128:
        case PAIRLL:
        default:
            throw runtime_error("Accelerator does not support this etype, yet...");
            break;
    }
}

template <typename T>
void Accelerator::_free(operand_t& operand)
{
    DEBUG(TAG, "_free bh_base("<< (void*)(operand.base) << ") bh_base.data(" << operand.base->data << ") operand.data(" << operand.data << ")");
    if (!allocated(operand)) {  // Not allocated on device
        DEBUG(TAG, "_free skipping...");
        return;
    }

    T* data = (T*)(*operand.data);
    const int nelem = operand.nelem;

    bytes_allocated_ -= nelem*sizeof(T);
    bases_.erase(operand.base);

    #pragma offload_transfer                                    \
            target(mic:id_)                                     \
            nocopy(data:length(nelem) alloc_if(0) free_if(1))
}

void Accelerator::free(operand_t& operand)
{
    switch(operand.etype) {
        case BOOL:      _free<unsigned char>(operand); break;
        case INT8:      _free<int8_t>(operand); break;
        case INT16:     _free<int16_t>(operand); break;
        case INT32:     _free<int32_t>(operand); break;
        case INT64:     _free<int64_t>(operand); break;
        case UINT8:     _free<uint8_t>(operand); break;
        case UINT16:    _free<uint16_t>(operand); break;
        case UINT32:    _free<uint32_t>(operand); break;
        case UINT64:    _free<uint64_t>(operand); break;
        case FLOAT32:   _free<float>(operand); break;
        case FLOAT64:   _free<double>(operand); break;

        case COMPLEX64:
        case COMPLEX128:
        case PAIRLL:
        default:
            throw runtime_error("Accelerator does not support this etype, yet...");
            break;
    }
}

template <typename T>
void Accelerator::_push(operand_t& operand)
{
    DEBUG(TAG, "_push bh_base("<< (void*)(operand.base) << ") bh_base.data(" << operand.base->data << ") operand.data(" << operand.data << ")");

    T* data = (T*)(*operand.data);
    const int nelem = operand.nelem;
    bases_.insert(operand.base);

    #pragma offload_transfer                                    \
            target(mic:id_)                                     \
            in(data:length(nelem) alloc_if(0) free_if(0))
}

void Accelerator::push(operand_t& operand)
{
    switch(operand.etype) {
        case BOOL:      _push<unsigned char>(operand); break;
        case INT8:      _push<int8_t>(operand); break;
        case INT16:     _push<int16_t>(operand); break;
        case INT32:     _push<int32_t>(operand); break;
        case INT64:     _push<int64_t>(operand); break;
        case UINT8:     _push<uint8_t>(operand); break;
        case UINT16:    _push<uint16_t>(operand); break;
        case UINT32:    _push<uint32_t>(operand); break;
        case UINT64:    _push<uint64_t>(operand); break;
        case FLOAT32:   _push<float>(operand); break;
        case FLOAT64:   _push<double>(operand); break;

        case COMPLEX64:
        case COMPLEX128:
        case PAIRLL:
        default:
            throw runtime_error("Accelerator does not support this etype, yet...");
            break;
    }
}

template <typename T>
void Accelerator::_push_alloc(operand_t& operand)
{
    DEBUG(TAG, "_push_alloc bh_base("<< (void*)(operand.base) << ") bh_base.data(" << operand.base->data << ") operand.data(" << operand.data << ")");

    T* data = (T*)(*operand.data);
    const int nelem = operand.nelem;

    bytes_allocated_ += nelem*sizeof(T);

    #pragma offload_transfer                                    \
            target(mic:id_)                                     \
            in(data:length(nelem) alloc_if(1) free_if(0))
}

void Accelerator::push_alloc(operand_t& operand)
{
    switch(operand.etype) {
        case BOOL:      _push_alloc<unsigned char>(operand); break;
        case INT8:      _push_alloc<int8_t>(operand); break;
        case INT16:     _push_alloc<int16_t>(operand); break;
        case INT32:     _push_alloc<int32_t>(operand); break;
        case INT64:     _push_alloc<int64_t>(operand); break;
        case UINT8:     _push_alloc<uint8_t>(operand); break;
        case UINT16:    _push_alloc<uint16_t>(operand); break;
        case UINT32:    _push_alloc<uint32_t>(operand); break;
        case UINT64:    _push_alloc<uint64_t>(operand); break;
        case FLOAT32:   _push_alloc<float>(operand); break;
        case FLOAT64:   _push_alloc<double>(operand); break;

        case COMPLEX64:
        case COMPLEX128:
        case PAIRLL:
        default:
            throw runtime_error("Accelerator does not support this etype, yet...");
            break;
    }
}

template <typename T>
void Accelerator::_pull(operand_t& operand)
{
    DEBUG(TAG, "_pull bh_base("<< (void*)(operand.base) << ") bh_base.data(" << operand.base->data << ") operand.data(" << operand.data << ")");

    if (bases_.count(operand.base)==0) {    // Not allocated on device
        DEBUG(TAG, "_pull skipping");
        return;
    }

    T* data = (T*)(*operand.data);
    const int nelem = operand.nelem;

    #pragma offload_transfer                                    \
            target(mic:id_)                                     \
            out(data:length(nelem) alloc_if(0) free_if(0))
}

void Accelerator::pull(operand_t& operand)
{
    switch(operand.etype) {
        case BOOL:      _pull<unsigned char>(operand); break;
        case INT8:      _pull<int8_t>(operand); break;
        case INT16:     _pull<int16_t>(operand); break;
        case INT32:     _pull<int32_t>(operand); break;
        case INT64:     _pull<int64_t>(operand); break;
        case UINT8:     _pull<uint8_t>(operand); break;
        case UINT16:    _pull<uint16_t>(operand); break;
        case UINT32:    _pull<uint32_t>(operand); break;
        case UINT64:    _pull<uint64_t>(operand); break;
        case FLOAT32:   _pull<float>(operand); break;
        case FLOAT64:   _pull<double>(operand); break;

        case COMPLEX64:
        case COMPLEX128:
        case PAIRLL:
        default:
            throw runtime_error("Accelerator does not support this etype, yet...");
            break;
    }
}

template <typename T>
void Accelerator::_pull_free(operand_t& operand)
{
    DEBUG(TAG, "_pull_free bh_base("<< (void*)(operand.base) << ") bh_base.data(" << operand.base->data << ") operand.data(" << operand.data << ")");
    if (bases_.count(operand.base)==0) {    // Not allocated on device
        DEBUG(TAG, "_pull_free skipping...");
        return;
    }

    T* data = (T*)(*operand.data);
    const int nelem = operand.nelem;

    bytes_allocated_ -= nelem*sizeof(T);
    bases_.erase(operand.base);

    #pragma offload_transfer                                    \
            target(mic:id_)                                     \
            out(data:length(nelem) alloc_if(0) free_if(1))
}

void Accelerator::pull_free(operand_t& operand)
{
    switch(operand.etype) {
        case BOOL:      _pull_free<unsigned char>(operand); break;
        case INT8:      _pull_free<int8_t>(operand); break;
        case INT16:     _pull_free<int16_t>(operand); break;
        case INT32:     _pull_free<int32_t>(operand); break;
        case INT64:     _pull_free<int64_t>(operand); break;
        case UINT8:     _pull_free<uint8_t>(operand); break;
        case UINT16:    _pull_free<uint16_t>(operand); break;
        case UINT32:    _pull_free<uint32_t>(operand); break;
        case UINT64:    _pull_free<uint64_t>(operand); break;
        case FLOAT32:   _pull_free<float>(operand); break;
        case FLOAT64:   _pull_free<double>(operand); break;

        case COMPLEX64:
        case COMPLEX128:
        case PAIRLL:
        default:
            throw runtime_error("Accelerator does not support this etype, yet...");
            break;
    }
}

int Accelerator::get_max_threads(void)
{
    int mthreads;                        
    #pragma offload target(mic:id_) out(mthreads)
    {                                    
        mthreads = omp_get_max_threads();
    }                                    
    return mthreads;                     
}

#else
//
// Fallback when compiled without Language Extensions for Offload (Intel LEO).
//
Accelerator::Accelerator(int id) : id_(id), bytes_allocated_(0) {
    throw runtime_error(
        "Bohrium was compiled without LEO, offload is not possible.\n"
        "Disable offloading by setting jit_offload=0 in config."
    );
};
void Accelerator::alloc(operand_t& operand) {}
void Accelerator::free(operand_t& operand) {}
void Accelerator::push(operand_t& operand) {}
void Accelerator::push_alloc(operand_t& operand) {}
void Accelerator::pull(operand_t& operand) {}
void Accelerator::pull_free(operand_t& operand) {}
int Accelerator::get_max_threads(void) { return 1; }
#endif

int Accelerator::id(void)
{
    return id_;
}

string Accelerator::text(void)
{
    stringstream ss;
    ss << "Accelerator id(" << id_ << ") {}";
    return ss.str();
}

size_t Accelerator::bytes_allocated(void)
{
    return bytes_allocated_;
}

bool Accelerator::allocated(operand_t& operand)
{
    return bases_.count(operand.base)!=0;
}

}}}
