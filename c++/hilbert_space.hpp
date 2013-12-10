/*******************************************************************************
 *
 * TRIQS: a Toolbox for Research in Interacting Quantum Systems
 *
 * Copyright (C) 2013 by I. Krivenko, M. Ferrero, O. Parcollet
 *
 * TRIQS is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * TRIQS is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * TRIQS. If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/
#pragma once

#include <triqs/utility/exceptions.hpp>
#include "fundamental_operator_set.hpp"
#include <boost/container/flat_map.hpp>

namespace cthyb_krylov {

/// The coding of the Fock state
using fock_state_t = uint64_t;

/* ---------------------------------------------------------------------------------------------------
 * A *full* Hilbert space spanned from all Fock states generated by a given set of fundamental operators
 * --------------------------------------------------------------------------------------------------  */
class hilbert_space {
 int dim; // the dimension

 public:
 hilbert_space() : dim(0) {}

 // construct for a given basis
 hilbert_space(fundamental_operator_set const &fops) : dim(fops.dimension()) {}

 // size of the hilbert space
 int dimension() const { return dim; }

 // find the index of a given fock state
 int get_state_index(fock_state_t f) const {
  if (f >= dim) TRIQS_RUNTIME_ERROR << "this index is too big";
  return f;
 }

 // return the i^th basis element as a fock state
 fock_state_t get_fock_state(int i) const {
  if (i >= dim) TRIQS_RUNTIME_ERROR << "this fock state doesn't exist (index too big)";
  return i; 
 }
};


/* ---------------------------------------------------------------------------------------------------
 * a subhilbert space, as a set of basis Fock states.
 * --------------------------------------------------------------------------------------------------  */
// // contains 2 functions to switch from the Fock state to its number in this set
class sub_hilbert_space {

 public:
 sub_hilbert_space(int index) : index(index) {}

 // add a fock state to the hilbert space basis
 void add_fock_state(fock_state_t f) {
  int ind = fock_states.size();
  fock_states.push_back(f);
  fock_to_index.insert(std::make_pair(f, ind));
 }

 // dimension
 int dimension() const { return fock_states.size(); }

 // find the index of a given state
 int get_state_index(fock_state_t f) const { return fock_to_index.find(f)->second; }

 // the state for a given index
 fock_state_t get_fock_state(int i) const { return fock_states[i]; }

 // The subhilbert has a (block) index
 int get_index() const {
  return index;
 };

 private:
 int index;

 // the list of all fock states
 std::vector<fock_state_t> fock_states;

 // reverse map to quickly find the index of a state
 // the boost flat_map is implemented as an ordered vector,
 // hence is it slow to insert (we don't care) but fast to look up (we do it a lot)
 // std::map<fock_state_t, int> fock_to_index;
 boost::container::flat_map<fock_state_t, int> fock_to_index;
};
}
