#pragma once
/******************************************************************************
 *
 * mfmm
 * A high-performance fast multipole method library using C++.
 *
 * A fork of ExaFMM (BSD-3-Clause lisence).
 * Originally copyright Wang, Yokota and Barba.
 *
 * Modifications copyright HJA Bird.
 *
 ******************************************************************************/
#ifndef INCLUDE_MFMM_MORTON_H_
#define INCLUDE_MFMM_MORTON_H_
#include <stdint.h>

#include "potential_traits.h"
#include "relative_coords.h"

namespace mfmm {

/** Representation of a location in an octree including octree level and Morton
 * key. This maps a 3D integer index to linear index using z-ordering.
 **/
class octree_location {
 public:
  octree_location() = default;

  octree_location(uint64_t x, int level) : m_key{x}, m_level{level} {};

  /** Get Morton key from 3D index of a node.
   * @param iX 3D index of a node, an integer triplet.
   * @param level Level of the node.
   * @param offset Whether to add level offset to the key, default to true.
   */
  octree_location(ivec3 iX, int level) : m_key{0}, m_level{level} {
    for (int l = 0; l < level; l++) {
      m_key |= (iX[2] & (uint64_t)1 << l) << 2 * l;
      m_key |= (iX[1] & (uint64_t)1 << l) << (2 * l + 1);
      m_key |= (iX[0] & (uint64_t)1 << l) << (2 * l + 2);
    }
  }

  /// Get a uint64_t representation of this key.
  uint64_t& key() { return m_key; }
  const uint64_t& key() const { return m_key; }

  bool operator==(const octree_location& other) const noexcept {
    return (m_key == other.m_key) && (m_level == other.m_level);
  }
  bool operator!=(const octree_location& other) const noexcept {
    return (m_key != other.m_key) || (m_level != other.m_level);
  }

  /** Get level of this key.
   * @return int Level.
   */
  inline int level() const { return m_level; }

  /** Get parent's Morton key (level - 1).
   * @return octree_location Parent's Morton key.
   */
  inline octree_location parent() const {
    return octree_location(m_key / 8, m_level - 1);
  }

  /** Get first child's Morton key (level + 1).
   * @param octant The octant of the child. Default 0.
   * @return octree_location First child's Morton key.
   */
  inline octree_location child(size_t octant = 0) const {
    return octree_location(m_key * 8 + octant, m_level + 1);
  }

  /** Determine which octant the key belongs to.
   * @return int Octant.
   */
  inline int octant() const { return m_key & 0x7; }

  /** Get 3D index from a Morton key.
   * @return ivec3 3D index, an integer triplet.
   */
  ivec3 get_3D_index() const {
    ivec3 iX = {0, 0, 0};
    for (int i = 0; i < m_level; i++) {
      // And the relevant bit from m_key, then shift to location in
      // iX: >> 3*i << i == >> 2*i.
      iX[2] |= (m_key & (uint64_t)1 << 3 * i) >> 2 * i;
      iX[1] |= (m_key & (uint64_t)1 << (3 * i + 1)) >> (2 * i + 1);
      iX[0] |= (m_key & (uint64_t)1 << (3 * i + 2)) >> (2 * i + 2);
    }
    return iX;
  }

  /** Check the adjacency of another key to this one.
   * This and the other key can be of different levels. Adjacentcy is determined
   * by looking at the radii of the cube in an octree according to the key's
   * level. If the sum of the key's radii is less than the distance apart, the
   * keys are adjacent.
   * @param other Morton keys with level offset.
   * @return True if adjacent.
   */
  inline bool is_adjacent(const octree_location other) const noexcept {
    int maxLevel = std::max(m_level, other.m_level);
    ivec3 iX_a = get_3D_index();
    ivec3 iX_b = other.get_3D_index();
    ivec3 iX_ac = (iX_a * 2 + ivec3{1, 1, 1}) *
                  (1 << (maxLevel - m_level));  // center coordinates
    ivec3 iX_bc = (iX_b * 2 + ivec3{1, 1, 1}) *
                  (1 << (maxLevel - other.m_level));  // center coordinates
    ivec3 diff = iX_ac - iX_bc;
    int maxDiff = -1;  // L-infinity norm of diff
    diff = diff.array().abs();
    maxDiff = diff.maxCoeff();
    int sum_radius =
        (1 << (maxLevel - m_level)) + (1 << (maxLevel - other.m_level));
    return (diff[0] <= sum_radius) && (diff[1] <= sum_radius) &&
           (diff[2] <= sum_radius) && (maxDiff == sum_radius);
  }

 protected:
  // The Morton key itself.
  uint64_t m_key;
  // The level in the octree the key represents
  int m_level;
};

/** Given bounding box and level, get 3D index from 3D coordinates.
 * @tparam T A potential type.
 * @param X 3D coordinates.
 * @param level Level.
 * @param x0 Coordinates of the center of the bounding box.
 * @param r0 Half of the side length of the bounding box.
 * @return ivec3 3D index, an integer triplet.
 */
template <typename T>
ivec3 get3DIndex(typename potential_traits<T>::coord_t X, int level,
                 typename potential_traits<T>::coord_t x0,
                 typename potential_traits<T>::real_t r0) {
  using coord_t = typename potential_traits<T>::coord_t;
  coord_t Xmin = x0 - r0 * coord_t::Ones();
  typename potential_traits<T>::real_t dx = 2 * r0 / (1 << level);
  ivec3 iX = ((X - Xmin) / dx).array().floor().cast<int>();
  return iX;
}

/** Given bounding box and level, get 3D coordinates from 3D index.
 * @tparam T A potential type.
 * @param iX 3D index, an integer triplet.
 * @param level Level.
 * @param x0 Coordinates of the center of the bounding box.
 * @param r0 Half of the side length of the bounding box.
 * @return vec3 3D coordinates.
 */
template <typename PotentialT>
typename potential_traits<PotentialT>::coord_t getCoordinates(
    ivec3 iX, int level, typename potential_traits<PotentialT>::coord_t x0,
    typename potential_traits<PotentialT>::real_t r0) {
  using pt = potential_traits<PotentialT>;
  typename pt::coord_t Xmin = x0 - r0;
  typename pt::real_t dx = 2 * r0 / (1 << level);
  typename pt::coord_t X = (iX + coord_t::Ones() * 0.5) * dx + Xmin;
  return X;
}
}  // namespace mfmm

namespace std {
template <>
struct hash<mfmm::octree_location> {
  std::size_t operator()(const mfmm::octree_location& k) const {
    return std::hash<uint64_t>()(k.key()) ^ std::hash<int>()(k.level());
  }
};
}  // namespace std

#endif  // INCLUDE_MFMM_MORTON_H_
