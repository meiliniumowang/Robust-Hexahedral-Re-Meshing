// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "bvh.h"

namespace embree
{
  namespace isa
  {
    template<int N, int Nx>
      struct TravRay 
    {
      __forceinline TravRay () {}

      __forceinline TravRay(const Vec3fa& ray_org, const Vec3fa& ray_dir) 
        : org_xyz(ray_org), dir_xyz(ray_dir) 
      {
        const Vec3fa ray_rdir = rcp_safe(ray_dir);
        const Vec3fa ray_org_rdir = ray_org*ray_rdir;
        org = Vec3<vfloat<N>>(ray_org.x,ray_org.y,ray_org.z);
        dir = Vec3<vfloat<N>>(ray_dir.x,ray_dir.y,ray_dir.z);
        rdir = Vec3<vfloat<N>>(ray_rdir.x,ray_rdir.y,ray_rdir.z);
#if defined(__AVX2__)
        org_rdir = Vec3<vfloat<N>>(ray_org_rdir.x,ray_org_rdir.y,ray_org_rdir.z);
#endif
        nearX = ray_rdir.x >= 0.0f ? 0*sizeof(vfloat<N>) : 1*sizeof(vfloat<N>);
        nearY = ray_rdir.y >= 0.0f ? 2*sizeof(vfloat<N>) : 3*sizeof(vfloat<N>);
        nearZ = ray_rdir.z >= 0.0f ? 4*sizeof(vfloat<N>) : 5*sizeof(vfloat<N>);
        farX  = nearX ^ sizeof(vfloat<N>);
        farY  = nearY ^ sizeof(vfloat<N>);
        farZ  = nearZ ^ sizeof(vfloat<N>);
      }

      template<int K>
      __forceinline TravRay (size_t k, const Vec3<vfloat<K>> &ray_org, const Vec3<vfloat<K>> &ray_dir, const Vec3<vfloat<K>> &ray_rdir, const Vec3<vint<K>>& nearXYZ, const size_t flip = sizeof(vfloat<N>))
      {
        org = Vec3<vfloat<N>>(ray_org.x[k], ray_org.y[k], ray_org.z[k]);
	dir = Vec3<vfloat<N>>(ray_dir.x[k], ray_dir.y[k], ray_dir.z[k]);
	rdir = Vec3<vfloat<N>>(ray_rdir.x[k], ray_rdir.y[k], ray_rdir.z[k]);
#if defined(__AVX2__)
	org_rdir = org*rdir;
#endif
	nearX = nearXYZ.x[k];
	nearY = nearXYZ.y[k];
	nearZ = nearXYZ.z[k];
        farX  = nearX ^ flip;
        farY  = nearY ^ flip;
        farZ  = nearZ ^ flip;
      }

      __forceinline TravRay (const TravRay& ray)
      {
        org_xyz = ray.org_xyz;
        dir_xyz = ray.dir_xyz;
        org = ray.org;
        dir = ray.dir;
        rdir = ray.rdir;
#if defined(__AVX2__)
        org_rdir = ray.org_rdir;
#endif
        nearX = ray.nearX;
        nearY = ray.nearY;
        nearZ = ray.nearZ;
        farX = ray.farX;
        farY = ray.farY;
        farZ = ray.farZ;
      }

      Vec3fa org_xyz, dir_xyz;
      Vec3<vfloat<Nx>> org, dir, rdir;
#if defined(__AVX2__)
      Vec3<vfloat<Nx>> org_rdir;
#endif
      size_t nearX, nearY, nearZ;
      size_t farX, farY, farZ;
    };

    /*! intersection with single rays */
    template<int N,int Nx>
      __forceinline size_t intersectNodeRobust(const typename BVHN<N>::Node* node, const TravRay<N,Nx>& ray, const vfloat<Nx>& tnear, const vfloat<Nx>& tfar, vfloat<Nx>& dist) 
    {      
      const vfloat<N> tNearX = (vfloat<N>::load((float*)((const char*)&node->lower_x+ray.nearX)) - ray.org.x) * ray.rdir.x;
      const vfloat<N> tNearY = (vfloat<N>::load((float*)((const char*)&node->lower_x+ray.nearY)) - ray.org.y) * ray.rdir.y;
      const vfloat<N> tNearZ = (vfloat<N>::load((float*)((const char*)&node->lower_x+ray.nearZ)) - ray.org.z) * ray.rdir.z;
      const vfloat<N> tFarX  = (vfloat<N>::load((float*)((const char*)&node->lower_x+ray.farX )) - ray.org.x) * ray.rdir.x;
      const vfloat<N> tFarY  = (vfloat<N>::load((float*)((const char*)&node->lower_x+ray.farY )) - ray.org.y) * ray.rdir.y;
      const vfloat<N> tFarZ  = (vfloat<N>::load((float*)((const char*)&node->lower_x+ray.farZ )) - ray.org.z) * ray.rdir.z;
      const float round_down = 1.0f-2.0f*float(ulp); // FIXME: use per instruction rounding for AVX512
      const float round_up   = 1.0f+2.0f*float(ulp);
      const vfloat<N> tNear = max(tNearX,tNearY,tNearZ,tnear);
      const vfloat<N> tFar  = min(tFarX ,tFarY ,tFarZ ,tfar);
      const vbool<N> vmask = (round_down*tNear <= round_up*tFar);
      const size_t mask = movemask(vmask);
      dist = tNear;
      return mask;
    }

#if defined(__AVX512F__)

    template<>
      __forceinline size_t intersectNodeRobust<4,16>(const typename BVHN<4>::Node* node, const TravRay<4,16>& ray, const vfloat<16>& tnear, const vfloat<16>& tfar, vfloat<16>& dist) 
    {      
      const vfloat16 tNearX = (vfloat16(*(vfloat<4>*)((const char*)&node->lower_x+ray.nearX)) - ray.org.x) * ray.rdir.x;
      const vfloat16 tNearY = (vfloat16(*(vfloat<4>*)((const char*)&node->lower_x+ray.nearY)) - ray.org.y) * ray.rdir.y;
      const vfloat16 tNearZ = (vfloat16(*(vfloat<4>*)((const char*)&node->lower_x+ray.nearZ)) - ray.org.z) * ray.rdir.z;
      const vfloat16 tFarX  = (vfloat16(*(vfloat<4>*)((const char*)&node->lower_x+ray.farX )) - ray.org.x) * ray.rdir.x;
      const vfloat16 tFarY  = (vfloat16(*(vfloat<4>*)((const char*)&node->lower_x+ray.farY )) - ray.org.y) * ray.rdir.y;
      const vfloat16 tFarZ  = (vfloat16(*(vfloat<4>*)((const char*)&node->lower_x+ray.farZ )) - ray.org.z) * ray.rdir.z;
      const float round_down = 1.0f-2.0f*float(ulp); // FIXME: use per instruction rounding for AVX512
      const float round_up   = 1.0f+2.0f*float(ulp);
      const vfloat16 tNear = max(tNearX,tNearY,tNearZ,tnear);
      const vfloat16 tFar  = min(tFarX ,tFarY ,tFarZ ,tfar);
      const vbool16 vmask = le((1 << 4)-1,round_down*tNear,round_up*tFar);
      const size_t mask = movemask(vmask);
      dist = tNear;
      return mask;
    }

    template<>
      __forceinline size_t intersectNodeRobust<8,16>(const typename BVHN<8>::Node* node, const TravRay<8,16>& ray, const vfloat<16>& tnear, const vfloat<16>& tfar, vfloat<16>& dist) 
    {      
      const vfloat16 tNearX = (vfloat16(*(vfloat<8>*)((const char*)&node->lower_x+ray.nearX)) - ray.org.x) * ray.rdir.x;
      const vfloat16 tNearY = (vfloat16(*(vfloat<8>*)((const char*)&node->lower_x+ray.nearY)) - ray.org.y) * ray.rdir.y;
      const vfloat16 tNearZ = (vfloat16(*(vfloat<8>*)((const char*)&node->lower_x+ray.nearZ)) - ray.org.z) * ray.rdir.z;
      const vfloat16 tFarX  = (vfloat16(*(vfloat<8>*)((const char*)&node->lower_x+ray.farX )) - ray.org.x) * ray.rdir.x;
      const vfloat16 tFarY  = (vfloat16(*(vfloat<8>*)((const char*)&node->lower_x+ray.farY )) - ray.org.y) * ray.rdir.y;
      const vfloat16 tFarZ  = (vfloat16(*(vfloat<8>*)((const char*)&node->lower_x+ray.farZ )) - ray.org.z) * ray.rdir.z;
      const float round_down = 1.0f-2.0f*float(ulp); // FIXME: use per instruction rounding for AVX512
      const float round_up   = 1.0f+2.0f*float(ulp);
      const vfloat16 tNear = max(tNearX,tNearY,tNearZ,tnear);
      const vfloat16 tFar  = min(tFarX ,tFarY ,tFarZ ,tfar);
      const vbool16 vmask = le((1 << 8)-1,round_down*tNear,round_up*tFar);
      const size_t mask = movemask(vmask);
      dist = tNear;
      return mask;
    }


#endif


    /*! intersection with single rays */
    template<int N, int Nx>
      __forceinline size_t intersectNode(const typename BVHN<N>::Node* node, const TravRay<N,Nx>& ray, const vfloat<Nx>& tnear, const vfloat<Nx>& tfar, vfloat<Nx>& dist);

    template<>
      __forceinline size_t intersectNode<4,4>(const typename BVH4::Node* node, const TravRay<4,4>& ray, const vfloat4& tnear, const vfloat4& tfar, vfloat4& dist)
    {
#if defined (__AVX2__)
      const vfloat4 tNearX = msub(vfloat4::load((float*)((const char*)&node->lower_x+ray.nearX)), ray.rdir.x, ray.org_rdir.x);
      const vfloat4 tNearY = msub(vfloat4::load((float*)((const char*)&node->lower_x+ray.nearY)), ray.rdir.y, ray.org_rdir.y);
      const vfloat4 tNearZ = msub(vfloat4::load((float*)((const char*)&node->lower_x+ray.nearZ)), ray.rdir.z, ray.org_rdir.z);
      const vfloat4 tFarX  = msub(vfloat4::load((float*)((const char*)&node->lower_x+ray.farX )), ray.rdir.x, ray.org_rdir.x);
      const vfloat4 tFarY  = msub(vfloat4::load((float*)((const char*)&node->lower_x+ray.farY )), ray.rdir.y, ray.org_rdir.y);
      const vfloat4 tFarZ  = msub(vfloat4::load((float*)((const char*)&node->lower_x+ray.farZ )), ray.rdir.z, ray.org_rdir.z);
#else
      const vfloat4 tNearX = (vfloat4::load((float*)((const char*)&node->lower_x+ray.nearX)) - ray.org.x) * ray.rdir.x;
      const vfloat4 tNearY = (vfloat4::load((float*)((const char*)&node->lower_x+ray.nearY)) - ray.org.y) * ray.rdir.y;
      const vfloat4 tNearZ = (vfloat4::load((float*)((const char*)&node->lower_x+ray.nearZ)) - ray.org.z) * ray.rdir.z;
      const vfloat4 tFarX  = (vfloat4::load((float*)((const char*)&node->lower_x+ray.farX )) - ray.org.x) * ray.rdir.x;
      const vfloat4 tFarY  = (vfloat4::load((float*)((const char*)&node->lower_x+ray.farY )) - ray.org.y) * ray.rdir.y;
      const vfloat4 tFarZ  = (vfloat4::load((float*)((const char*)&node->lower_x+ray.farZ )) - ray.org.z) * ray.rdir.z;
#endif
      
#if defined(__SSE4_1__)
      const vfloat4 tNear = maxi(maxi(tNearX,tNearY),maxi(tNearZ,tnear));
      const vfloat4 tFar  = mini(mini(tFarX ,tFarY ),mini(tFarZ ,tfar ));
      const vbool4 vmask = asInt(tNear) > asInt(tFar);
      const size_t mask = movemask(vmask) ^ ((1<<4)-1);
#else
      const vfloat4 tNear = max(tNearX,tNearY,tNearZ,tnear);
      const vfloat4 tFar  = min(tFarX ,tFarY ,tFarZ ,tfar);
      const vbool4 vmask = tNear <= tFar;
      const size_t mask = movemask(vmask);
#endif
      dist = tNear;
      return mask;
    }

#if defined(__AVX__)

    template<>
      __forceinline size_t intersectNode<8,8>(const typename BVH8::Node* node, const TravRay<8,8>& ray, const vfloat8& tnear, const vfloat8& tfar, vfloat8& dist)
    {
#if defined (__AVX2__)
      const vfloat8 tNearX = msub(vfloat8::load((float*)((const char*)&node->lower_x+ray.nearX)), ray.rdir.x, ray.org_rdir.x);
      const vfloat8 tNearY = msub(vfloat8::load((float*)((const char*)&node->lower_x+ray.nearY)), ray.rdir.y, ray.org_rdir.y);
      const vfloat8 tNearZ = msub(vfloat8::load((float*)((const char*)&node->lower_x+ray.nearZ)), ray.rdir.z, ray.org_rdir.z);
      const vfloat8 tFarX  = msub(vfloat8::load((float*)((const char*)&node->lower_x+ray.farX )), ray.rdir.x, ray.org_rdir.x);
      const vfloat8 tFarY  = msub(vfloat8::load((float*)((const char*)&node->lower_x+ray.farY )), ray.rdir.y, ray.org_rdir.y);
      const vfloat8 tFarZ  = msub(vfloat8::load((float*)((const char*)&node->lower_x+ray.farZ )), ray.rdir.z, ray.org_rdir.z);
#else
      const vfloat8 tNearX = (vfloat8::load((float*)((const char*)&node->lower_x+ray.nearX)) - ray.org.x) * ray.rdir.x;
      const vfloat8 tNearY = (vfloat8::load((float*)((const char*)&node->lower_x+ray.nearY)) - ray.org.y) * ray.rdir.y;
      const vfloat8 tNearZ = (vfloat8::load((float*)((const char*)&node->lower_x+ray.nearZ)) - ray.org.z) * ray.rdir.z;
      const vfloat8 tFarX  = (vfloat8::load((float*)((const char*)&node->lower_x+ray.farX )) - ray.org.x) * ray.rdir.x;
      const vfloat8 tFarY  = (vfloat8::load((float*)((const char*)&node->lower_x+ray.farY )) - ray.org.y) * ray.rdir.y;
      const vfloat8 tFarZ  = (vfloat8::load((float*)((const char*)&node->lower_x+ray.farZ )) - ray.org.z) * ray.rdir.z;
#endif
      
#if defined(__AVX2__) && !defined(__AVX512F__)
      const vfloat8 tNear = maxi(maxi(tNearX,tNearY),maxi(tNearZ,tnear));
      const vfloat8 tFar  = mini(mini(tFarX ,tFarY ),mini(tFarZ ,tfar ));
      const vbool8 vmask = asInt(tNear) > asInt(tFar);
      const size_t mask = movemask(vmask) ^ ((1<<8)-1);
#else
      const vfloat8 tNear = max(tNearX,tNearY,tNearZ,tnear);
      const vfloat8 tFar  = min(tFarX ,tFarY ,tFarZ ,tfar);
      const vbool8 vmask = tNear <= tFar;
      const size_t mask = movemask(vmask);
#endif
      dist = tNear;
      return mask;
    }

#endif

#if defined(__AVX512F__)

    template<>
      __forceinline size_t intersectNode<4,16>(const typename BVH4::Node* node, const TravRay<4,16>& ray, const vfloat16& tnear, const vfloat16& tfar, vfloat16& dist)
    {
      const vfloat16 tNearX = msub(vfloat16(*(vfloat4*)((const char*)&node->lower_x+ray.nearX)), ray.rdir.x, ray.org_rdir.x);
      const vfloat16 tNearY = msub(vfloat16(*(vfloat4*)((const char*)&node->lower_x+ray.nearY)), ray.rdir.y, ray.org_rdir.y);
      const vfloat16 tNearZ = msub(vfloat16(*(vfloat4*)((const char*)&node->lower_x+ray.nearZ)), ray.rdir.z, ray.org_rdir.z);
      const vfloat16 tFarX  = msub(vfloat16(*(vfloat4*)((const char*)&node->lower_x+ray.farX )), ray.rdir.x, ray.org_rdir.x);
      const vfloat16 tFarY  = msub(vfloat16(*(vfloat4*)((const char*)&node->lower_x+ray.farY )), ray.rdir.y, ray.org_rdir.y);
      const vfloat16 tFarZ  = msub(vfloat16(*(vfloat4*)((const char*)&node->lower_x+ray.farZ )), ray.rdir.z, ray.org_rdir.z);      
      const vfloat16 tNear  = max(tNearX,tNearY,tNearZ,tnear);
      const vfloat16 tFar   = min(tFarX ,tFarY ,tFarZ ,tfar);
      const vbool16 vmask   = le(vbool16(0xf),tNear,tFar);
      const size_t mask     = movemask(vmask);
      dist = tNear;
      return mask;
    }

    template<>
      __forceinline size_t intersectNode<8,16>(const typename BVH8::Node* node, const TravRay<8,16>& ray, const vfloat16& tnear, const vfloat16& tfar, vfloat16& dist)
    {
      const vfloat16 tNearX = msub(vfloat16(*(vfloat8*)((const char*)&node->lower_x+ray.nearX)), ray.rdir.x, ray.org_rdir.x);
      const vfloat16 tNearY = msub(vfloat16(*(vfloat8*)((const char*)&node->lower_x+ray.nearY)), ray.rdir.y, ray.org_rdir.y);
      const vfloat16 tNearZ = msub(vfloat16(*(vfloat8*)((const char*)&node->lower_x+ray.nearZ)), ray.rdir.z, ray.org_rdir.z);
      const vfloat16 tFarX  = msub(vfloat16(*(vfloat8*)((const char*)&node->lower_x+ray.farX )), ray.rdir.x, ray.org_rdir.x);
      const vfloat16 tFarY  = msub(vfloat16(*(vfloat8*)((const char*)&node->lower_x+ray.farY )), ray.rdir.y, ray.org_rdir.y);
      const vfloat16 tFarZ  = msub(vfloat16(*(vfloat8*)((const char*)&node->lower_x+ray.farZ )), ray.rdir.z, ray.org_rdir.z);      
      
      const vfloat16 tNear  = max(tNearX,tNearY,tNearZ,tnear);
      const vfloat16 tFar   = min(tFarX ,tFarY ,tFarZ ,tfar);
      const vbool16 vmask   = le(vbool16(0xff),tNear,tFar);
      const size_t mask     = movemask(vmask);
      dist = tNear;
      return mask;
    }
    
#endif

    /*! intersection with ray packet of size K */
    template<int N, int K>
      __forceinline vbool<K> intersectNodeRobust(const typename BVHN<N>::Node* node, size_t i, const Vec3<vfloat<K>>& org, const Vec3<vfloat<K>>& rdir, const Vec3<vfloat<K>>& org_rdir,
                                                 const vfloat<K>& tnear, const vfloat<K>& tfar, vfloat<K>& dist)
    {
      // FIXME: use per instruction rounding for AVX512
      const vfloat<K> lclipMinX = (node->lower_x[i] - org.x) * rdir.x;
      const vfloat<K> lclipMinY = (node->lower_y[i] - org.y) * rdir.y;
      const vfloat<K> lclipMinZ = (node->lower_z[i] - org.z) * rdir.z;
      const vfloat<K> lclipMaxX = (node->upper_x[i] - org.x) * rdir.x;
      const vfloat<K> lclipMaxY = (node->upper_y[i] - org.y) * rdir.y;
      const vfloat<K> lclipMaxZ = (node->upper_z[i] - org.z) * rdir.z;
      const float round_down = 1.0f-2.0f*float(ulp);
      const float round_up   = 1.0f+2.0f*float(ulp);
      const vfloat<K> lnearP = max(max(min(lclipMinX, lclipMaxX), min(lclipMinY, lclipMaxY)), min(lclipMinZ, lclipMaxZ));
      const vfloat<K> lfarP  = min(min(max(lclipMinX, lclipMaxX), max(lclipMinY, lclipMaxY)), max(lclipMinZ, lclipMaxZ));
      const vbool<K> lhit   = round_down*max(lnearP,tnear) <= round_up*min(lfarP,tfar);
      dist = lnearP;
      return lhit;
    }
    
    /*! intersection with ray packet of size K */
    template<int N, int K>
      __forceinline vbool<K> intersectNode(const typename BVHN<N>::Node* node, size_t i, const Vec3<vfloat<K>>& org, const Vec3<vfloat<K>>& rdir, const Vec3<vfloat<K>>& org_rdir,
                                           const vfloat<K>& tnear, const vfloat<K>& tfar, vfloat<K>& dist)
 
    {
#if defined(__AVX2__)
      const vfloat<K> lclipMinX = msub(node->lower_x[i],rdir.x,org_rdir.x);
      const vfloat<K> lclipMinY = msub(node->lower_y[i],rdir.y,org_rdir.y);
      const vfloat<K> lclipMinZ = msub(node->lower_z[i],rdir.z,org_rdir.z);
      const vfloat<K> lclipMaxX = msub(node->upper_x[i],rdir.x,org_rdir.x);
      const vfloat<K> lclipMaxY = msub(node->upper_y[i],rdir.y,org_rdir.y);
      const vfloat<K> lclipMaxZ = msub(node->upper_z[i],rdir.z,org_rdir.z);
#else
      const vfloat<K> lclipMinX = (node->lower_x[i] - org.x) * rdir.x;
      const vfloat<K> lclipMinY = (node->lower_y[i] - org.y) * rdir.y;
      const vfloat<K> lclipMinZ = (node->lower_z[i] - org.z) * rdir.z;
      const vfloat<K> lclipMaxX = (node->upper_x[i] - org.x) * rdir.x;
      const vfloat<K> lclipMaxY = (node->upper_y[i] - org.y) * rdir.y;
      const vfloat<K> lclipMaxZ = (node->upper_z[i] - org.z) * rdir.z;
#endif  
      const vfloat<K> lnearP = maxi(maxi(mini(lclipMinX, lclipMaxX), mini(lclipMinY, lclipMaxY)), mini(lclipMinZ, lclipMaxZ));
      const vfloat<K> lfarP  = mini(mini(maxi(lclipMinX, lclipMaxX), maxi(lclipMinY, lclipMaxY)), maxi(lclipMinZ, lclipMaxZ));
      const vbool<K> lhit    = maxi(lnearP,tnear) <= mini(lfarP,tfar);
      dist = lnearP;
      return lhit;
    }

    /*! intersection with single rays */
    template<int N>
      __forceinline size_t intersectNode(const typename BVHN<N>::NodeMB* node, const TravRay<N,N>& ray, const vfloat<N>& tnear, const vfloat<N>& tfar, const float time, vfloat<N>& dist)
    {
      const vfloat<N>* pNearX = (const vfloat<N>*)((const char*)&node->lower_x+ray.nearX);
      const vfloat<N>* pNearY = (const vfloat<N>*)((const char*)&node->lower_x+ray.nearY);
      const vfloat<N>* pNearZ = (const vfloat<N>*)((const char*)&node->lower_x+ray.nearZ);
      const vfloat<N> tNearX = (vfloat<N>(pNearX[0]) + time*pNearX[6] - ray.org.x) * ray.rdir.x;
      const vfloat<N> tNearY = (vfloat<N>(pNearY[0]) + time*pNearY[6] - ray.org.y) * ray.rdir.y;
      const vfloat<N> tNearZ = (vfloat<N>(pNearZ[0]) + time*pNearZ[6] - ray.org.z) * ray.rdir.z;
      const vfloat<N> tNear = max(tnear,tNearX,tNearY,tNearZ);
      const vfloat<N>* pFarX = (const vfloat<N>*)((const char*)&node->lower_x+ray.farX);
      const vfloat<N>* pFarY = (const vfloat<N>*)((const char*)&node->lower_x+ray.farY);
      const vfloat<N>* pFarZ = (const vfloat<N>*)((const char*)&node->lower_x+ray.farZ);
      const vfloat<N> tFarX = (vfloat<N>(pFarX[0]) + time*pFarX[6] - ray.org.x) * ray.rdir.x;
      const vfloat<N> tFarY = (vfloat<N>(pFarY[0]) + time*pFarY[6] - ray.org.y) * ray.rdir.y;
      const vfloat<N> tFarZ = (vfloat<N>(pFarZ[0]) + time*pFarZ[6] - ray.org.z) * ray.rdir.z;
      const vfloat<N> tFar = min(tfar,tFarX,tFarY,tFarZ);
      const size_t mask = movemask(tNear <= tFar);
      dist = tNear;
      return mask;
    }

    /*! intersection with ray packet of size K */
    template<int N, int K>
    __forceinline vbool<K> intersectNode(const typename BVHN<N>::NodeMB* node, const size_t i, const Vec3<vfloat<K>>& org, const Vec3<vfloat<K>>& rdir, const Vec3<vfloat<K>>& org_rdir,
                                         const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist)
    {
      const vfloat<K> vlower_x = vfloat<K>(node->lower_x[i]) + time * vfloat<K>(node->lower_dx[i]);
      const vfloat<K> vlower_y = vfloat<K>(node->lower_y[i]) + time * vfloat<K>(node->lower_dy[i]);
      const vfloat<K> vlower_z = vfloat<K>(node->lower_z[i]) + time * vfloat<K>(node->lower_dz[i]);
      const vfloat<K> vupper_x = vfloat<K>(node->upper_x[i]) + time * vfloat<K>(node->upper_dx[i]);
      const vfloat<K> vupper_y = vfloat<K>(node->upper_y[i]) + time * vfloat<K>(node->upper_dy[i]);
      const vfloat<K> vupper_z = vfloat<K>(node->upper_z[i]) + time * vfloat<K>(node->upper_dz[i]);

#if defined(__AVX2__)
      const vfloat<K> lclipMinX = msub(vlower_x,rdir.x,org_rdir.x);
      const vfloat<K> lclipMinY = msub(vlower_y,rdir.y,org_rdir.y);
      const vfloat<K> lclipMinZ = msub(vlower_z,rdir.z,org_rdir.z);
      const vfloat<K> lclipMaxX = msub(vupper_x,rdir.x,org_rdir.x);
      const vfloat<K> lclipMaxY = msub(vupper_y,rdir.y,org_rdir.y);
      const vfloat<K> lclipMaxZ = msub(vupper_z,rdir.z,org_rdir.z);
#else
      const vfloat<K> lclipMinX = (vlower_x - org.x) * rdir.x;
      const vfloat<K> lclipMinY = (vlower_y - org.y) * rdir.y;
      const vfloat<K> lclipMinZ = (vlower_z - org.z) * rdir.z;
      const vfloat<K> lclipMaxX = (vupper_x - org.x) * rdir.x;
      const vfloat<K> lclipMaxY = (vupper_y - org.y) * rdir.y;
      const vfloat<K> lclipMaxZ = (vupper_z - org.z) * rdir.z;
#endif

      const vfloat<K> lnearP = maxi(maxi(mini(lclipMinX, lclipMaxX), mini(lclipMinY, lclipMaxY)), mini(lclipMinZ, lclipMaxZ));
      const vfloat<K> lfarP  = mini(mini(maxi(lclipMinX, lclipMaxX), maxi(lclipMinY, lclipMaxY)), maxi(lclipMinZ, lclipMaxZ));
      const vbool<K> lhit   = maxi(lnearP,tnear) <= mini(lfarP,tfar);
      dist = lnearP;
      return lhit;
    }

    /*! intersect N OBBs with single ray */
    template<int N>
      __forceinline size_t intersectNode(const typename BVHN<N>::UnalignedNode* node, const TravRay<N,N>& ray, const vfloat<N>& tnear, const vfloat<N>& tfar, vfloat<N>& dist)
    {
      const Vec3<vfloat<N>> dir = xfmVector(node->naabb,ray.dir);
      //const Vec3<vfloat<N>> nrdir = Vec3<vfloat<N>>(vfloat<N>(-1.0f))/dir;
      const Vec3<vfloat<N>> nrdir = Vec3<vfloat<N>>(vfloat<N>(-1.0f))*rcp_safe(dir);
      const Vec3<vfloat<N>> org = xfmPoint(node->naabb,ray.org);
      const Vec3<vfloat<N>> tLowerXYZ = org * nrdir;     // (Vec3fa(zero) - org) * rdir;
      const Vec3<vfloat<N>> tUpperXYZ = tLowerXYZ - nrdir; // (Vec3fa(one ) - org) * rdir;

      const vfloat<N> tNearX = mini(tLowerXYZ.x,tUpperXYZ.x);
      const vfloat<N> tNearY = mini(tLowerXYZ.y,tUpperXYZ.y);
      const vfloat<N> tNearZ = mini(tLowerXYZ.z,tUpperXYZ.z);
      const vfloat<N> tFarX  = maxi(tLowerXYZ.x,tUpperXYZ.x);
      const vfloat<N> tFarY  = maxi(tLowerXYZ.y,tUpperXYZ.y);
      const vfloat<N> tFarZ  = maxi(tLowerXYZ.z,tUpperXYZ.z);
      const vfloat<N> tNear  = max(tnear, tNearX,tNearY,tNearZ);
      const vfloat<N> tFar   = min(tfar,  tFarX ,tFarY ,tFarZ );
      const vbool<N> vmask = tNear <= tFar;
      dist = tNear;
      return movemask(vmask);
    }

     /*! intersect N OBBs with single ray */
    template<int N>
      __forceinline size_t intersectNode(const typename BVHN<N>::UnalignedNodeMB* node, const TravRay<N,N>& ray, const vfloat<N>& tnear, const vfloat<N>& tfar, const float time, vfloat<N>& dist)
    {
      const vfloat<N> t0 = vfloat<N>(1.0f)-time, t1 = time;

      const AffineSpaceT<LinearSpace3<Vec3<vfloat<N>>>> xfm = node->space0;
      const Vec3<vfloat<N>> b0_lower = zero;
      const Vec3<vfloat<N>> b0_upper = one;
      const Vec3<vfloat<N>> lower = t0*b0_lower + t1*node->b1.lower;
      const Vec3<vfloat<N>> upper = t0*b0_upper + t1*node->b1.upper;

      const BBox<Vec3<vfloat<N>>> bounds(lower,upper);
      const Vec3<vfloat<N>> dir = xfmVector(xfm,ray.dir);
      const Vec3<vfloat<N>> rdir = rcp_safe(dir);
      const Vec3<vfloat<N>> org = xfmPoint(xfm,ray.org);

      const Vec3<vfloat<N>> tLowerXYZ = (bounds.lower - org) * rdir;
      const Vec3<vfloat<N>> tUpperXYZ = (bounds.upper - org) * rdir;

      const vfloat<N> tNearX = mini(tLowerXYZ.x,tUpperXYZ.x);
      const vfloat<N> tNearY = mini(tLowerXYZ.y,tUpperXYZ.y);
      const vfloat<N> tNearZ = mini(tLowerXYZ.z,tUpperXYZ.z);
      const vfloat<N> tFarX  = maxi(tLowerXYZ.x,tUpperXYZ.x);
      const vfloat<N> tFarY  = maxi(tLowerXYZ.y,tUpperXYZ.y);
      const vfloat<N> tFarZ  = maxi(tLowerXYZ.z,tUpperXYZ.z);
      const vfloat<N> tNear  = max(tnear, tNearX,tNearY,tNearZ);
      const vfloat<N> tFar   = min(tfar,  tFarX ,tFarY ,tFarZ );
      const vbool<N> vmask = tNear <= tFar;
      dist = tNear;
      return movemask(vmask);
    }

    /*! Intersects N nodes with 1 ray */
    template<int N, int Nx, int types, bool robust>
    struct BVHNNodeIntersector1;

    template<int N, int Nx>
      struct BVHNNodeIntersector1<N,Nx,BVH_AN1,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const TravRay<N,Nx>& ray, const vfloat<Nx>& tnear, const vfloat<Nx>& tfar, const float time, vfloat<Nx>& dist, size_t& mask)
      {
        mask = intersectNode<N,Nx>(node.node(),ray,tnear,tfar,dist);
        return true;
      }
    };

    template<int N, int Nx>
      struct BVHNNodeIntersector1<N,Nx,BVH_AN1,true>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const TravRay<N,Nx>& ray, const vfloat<Nx>& tnear, const vfloat<Nx>& tfar, const float time, vfloat<Nx>& dist, size_t& mask)
      {
        mask = intersectNodeRobust<N,Nx>(node.node(),ray,tnear,tfar,dist);
        return true;
      }
    };

    template<int N, int Nx>
      struct BVHNNodeIntersector1<N,Nx,BVH_AN2,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const TravRay<N,Nx>& ray, const vfloat<N>& tnear, const vfloat<N>& tfar, const float time, vfloat<N>& dist, size_t& mask)
      {
        mask = intersectNode<N>(node.nodeMB(),ray,tnear,tfar,time,dist);
        return true;
      }
    };

    template<int N, int Nx>
      struct BVHNNodeIntersector1<N,Nx,BVH_AN1_UN1,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const TravRay<N,Nx>& ray, const vfloat<N>& tnear, const vfloat<N>& tfar, const float time, vfloat<N>& dist, size_t& mask)
      {
        if (likely(node.isNode()))                 mask = intersectNode<N,N>(node.node(),ray,tnear,tfar,dist);
        else if (unlikely(node.isUnalignedNode())) mask = intersectNode<N>(node.unalignedNode(),ray,tnear,tfar,dist);
        return true;
      }
    };

    template<int N, int Nx>
      struct BVHNNodeIntersector1<N,Nx,BVH_AN2_UN2,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const TravRay<N,Nx>& ray, const vfloat<N>& tnear, const vfloat<N>& tfar, const float time, vfloat<N>& dist, size_t& mask)
      {
        if (likely(node.isNodeMB()))            mask = intersectNode<N>(node.nodeMB(),ray,tnear,tfar,time,dist);
        if (unlikely(node.isUnalignedNodeMB())) mask = intersectNode<N>(node.unalignedNodeMB(),ray,tnear,tfar,time,dist);
        return true;
      }
    };

    template<int N, int Nx>
      struct BVHNNodeIntersector1<N,Nx,BVH_TN_AN1_AN2,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const TravRay<N,Nx>& ray, const vfloat<N>& tnear, const vfloat<N>& tfar, const float time, vfloat<N>& dist, size_t& mask)
      {
        if (likely(node.isNode()))        mask = intersectNode<N,N>(node.node(),ray,tnear,tfar,dist);
        else if (likely(node.isNodeMB())) mask = intersectNode<N>(node.nodeMB(),ray,tnear,tfar,time,dist);
        else                              return false;
        return true;
      }
    };


    /*! Intersects N nodes with K rays */
    template<int N, int K, int types, bool robust>
    struct BVHNNodeIntersectorK;

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN1,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i, const Vec3<vfloat<K>>& org, const Vec3<vfloat<K>>& rdir, const Vec3<vfloat<K>>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        vmask = intersectNode<N,K>(node.node(),i,org,rdir,org_rdir,tnear,tfar,dist);
        return true;
      }
    };

    template<int N, int K>
      struct BVHNNodeIntersectorK<N,K,BVH_AN1,true>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i, const Vec3<vfloat<K>>& org, const Vec3<vfloat<K>>& rdir, const Vec3<vfloat<K>>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        vmask = intersectNodeRobust<N,K>(node.node(),i,org,rdir,org_rdir,tnear,tfar,dist);
        return true;
      }
    };

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN2,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i, const Vec3<vfloat<K>>& org, const Vec3<vfloat<K>>& rdir, const Vec3<vfloat<K>>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        vmask = intersectNode<N,K>(node.nodeMB(),i,org,rdir,org_rdir,tnear,tfar,time,dist);
        return true;
      }
    };
  }
}

