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
#include "sorted_spaces.hpp"

using namespace triqs::arrays;
using std::string;

namespace cthyb_krylov {

// define a more tolerant comparison between vectors for the quantum numbers
struct lt_dbl {
 bool operator()(std::vector<double> const& v1, std::vector<double> const& v2) const {
  for (int i = 0; i < v1.size(); ++i) {
   if (v1[i] < (v2[i] - 1e-8))
    return true;
   else if (v2[i] < (v1[i] - 1e-8))
    return false;
  }
  return false;
 }
};

//-----------------------------

sorted_spaces::sorted_spaces(triqs::utility::many_body_operator<double> const& h_,
                             std::vector<triqs::utility::many_body_operator<double>> const& qn_vector,
                             fundamental_operator_set const& fops, std::vector<block_desc_t> const& block_structure)
   : n_blocks(0),
     hamilt(h_, fops),
     creation_operators(fops.n_operators()),
     destruction_operators(fops.n_operators()),
     creation_connection(fops.n_operators()),
     destruction_connection(fops.n_operators()) {

 std::map<indices_t, std::pair<int, int>> indices_to_ints;
 for (int bl = 0; bl < block_structure.size(); ++bl) {
  auto const& indices = block_structure[bl].indices;
  for (int i = 0; i < indices.size(); ++i) {
   indices_to_ints[indices[i]] = std::make_pair(bl, i);
  }
 }

 // hilbert spaces and quantum numbers
 std::map<std::vector<double>, int, lt_dbl> map_qn_n;

 //
 using hilbert_map_t = std::unordered_map<const sub_hilbert_space*, const sub_hilbert_space*>;
 std::vector<hilbert_map_t> creation_map(fops.n_operators()), destruction_map(fops.n_operators());

 // create the map int_pair_to_n : (int,int) --> int identifying operators
 for (auto x : fops) int_pair_to_n[indices_to_ints.at(x.index)] = x.linear_index;

 // the full Hilbert space
 hilbert_space full_hs(fops);

 // The QN as operators : a vector of imperative operators for the quantum numbers
 std::vector<imperative_operator<hilbert_space>> qn_operators;
 for (auto& qn : qn_vector) qn_operators.emplace_back(qn, fops);

 // Helper function to get quantum numbers
 auto get_quantum_numbers = [&qn_operators](state<hilbert_space, double, true> const& s) {
  std::vector<quantum_number_t> qn;
  for (auto const& op : qn_operators) qn.push_back(dot_product(s, op(s)));
  return qn;
 };

 /*
   The first part consists in dividing the full Hilbert space
   into smaller subspaces using the quantum numbers
 */
 for (size_t r = 0; r < full_hs.dimension(); ++r) {

  // fock_state corresponding to r
  fock_state_t fs = full_hs.get_fock_state(r);

  // the state we'll act on
  state<hilbert_space, double, true> s(full_hs);
  s(r) = 1.0;

  // create the vector with the quantum numbers
  std::vector<quantum_number_t> qn = get_quantum_numbers(s);

  // if first time we meet these quantum numbers create partial Hilbert space
  if (map_qn_n.count(qn) == 0) {
   sub_hilbert_spaces.push_back(std::make_shared<sub_hilbert_space>(sub_hilbert_spaces.size()));
   //   sub_hilbert_spaces.emplace_back(sub_hilbert_spaces.size()); // a new sub_hilbert_space
   quantum_numbers.push_back(qn);
   map_qn_n[qn] = n_blocks;
   n_blocks++;
  }

  // add fock state to partial Hilbert space
  sub_hilbert_spaces[map_qn_n[qn]]->add_fock_state(fs);
 }

 /*
   In this second part we want to derive the partial Hilbert space
   mapping. Basically we want to know if we act on a partial Hilbert
   space with a creation (destruction) operator, in which other
   partial Hilbert space we end up.
 */
 for (auto const& x : fops) {

  // get the operators and their index
  int n = x.linear_index;
  auto create = triqs::utility::many_body_operator<double>::make_canonical(true, x.index);
  auto destroy = triqs::utility::many_body_operator<double>::make_canonical(false, x.index);

  // construct their imperative counterpart
  imperative_operator<hilbert_space> op_c_dag(create, fops), op_c(destroy, fops);

  // to avoid declaring every time in the loop below
  std::vector<quantum_number_t> qn_before, qn_after;
  sub_hilbert_space* origin, *target;

  // these will be mapping tables
  creation_connection[n].resize(n_subspaces(), -1);
  destruction_connection[n].resize(n_subspaces(), -1);

  // now act on the state with the c, c_dag to see how quantum numbers change
  for (size_t r = 0; r < full_hs.dimension(); ++r) {

   // the state we'll act on and its quantum numbers
   state<hilbert_space, double, true> s(full_hs);
   s(r) = 1.0;
   qn_before = get_quantum_numbers(s);

   // apply creation on state to figure quantum numbers
   qn_after = get_quantum_numbers(op_c_dag(s));

   // insert in creation map checking whether it was already there
   if (dot_product(op_c_dag(s), op_c_dag(s)) > 1.e-10) {
    origin = sub_hilbert_spaces[map_qn_n[qn_before]].get();
    target = sub_hilbert_spaces[map_qn_n[qn_after]].get();
    if (creation_map[n].count(origin) == 0)
     creation_map[n][origin] = target;
    else if (creation_map[n][origin] != target)
     std::cout << "error creation";
    creation_connection[n][map_qn_n[qn_before]] = map_qn_n[qn_after];
   }

   // apply destruction on state to figure quantum numbers
   qn_after = get_quantum_numbers(op_c(s));

   // insert in destruction map checking whether it was already there
   if (dot_product(op_c(s), op_c(s)) > 1.e-10) {
    origin = sub_hilbert_spaces[map_qn_n[qn_before]].get();
    target = sub_hilbert_spaces[map_qn_n[qn_after]].get();
    if (destruction_map[n].count(origin) == 0)
     destruction_map[n][origin] = target;
    else if (destruction_map[n][origin] != target)
     std::cout << "error destruction";
    destruction_connection[n][map_qn_n[qn_before]] = map_qn_n[qn_after];
   }
  }

  // insert the creation and destruction operators in vectors. this is the fast version
  // of the operators because we explicitly use the map
  creation_operators[n] = imperative_operator<sub_hilbert_space, true>(create, fops, creation_map[n]);
  destruction_operators[n] = imperative_operator<sub_hilbert_space, true>(destroy, fops, destruction_map[n]);
 }

 // Compute energy levels and eigenvectors of the local Hamiltonian
 compute_eigensystems();

 // Shift the ground state energy of the local Hamiltonian to zero.
 for (auto& eigensystem : eigensystems) eigensystem.eigenvalues() -= get_gs_energy();
 hamilt = imperative_operator<sub_hilbert_space, false>(h_ - get_gs_energy(), fops);
}

// -----------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, sorted_spaces const& ss) {

 os << "Number of blocks: " << ss.n_subspaces() << std::endl;
 for (int n = 0; n < ss.sub_hilbert_spaces.size(); ++n) {
  os << "Block " << n << ", ";
  os << "qn = ";
  for (auto const& x : ss.quantum_numbers[n]) os << x << " ";
  os << ", ";
  os << "size = " << ss.sub_hilbert_spaces[n]->dimension();
  os << " Relative gs energy : " << ss.get_eigensystems()[n].eigenvalues[0] << std::endl;
 }
 return os;
}

//----------------------------------------------------------------------

void sorted_spaces::compute_eigensystems() {
 eigensystems.resize(n_subspaces());
 gs_energy = std::numeric_limits<double>::infinity();

 for (std::size_t spn = 0; spn < n_subspaces(); ++spn) {
  auto const& sp = subspace(spn);
  auto& eigensystem = eigensystems[spn];

  state<sub_hilbert_space, double, false> i_state(sp);
  matrix<double> h_matrix(sp.dimension(), sp.dimension());

  for (std::size_t i = 0; i < sp.dimension(); ++i) {
   i_state.amplitudes()() = 0;
   i_state(i) = 1;
   auto f_state = hamilt(i_state);
   h_matrix(range(), i) = f_state.amplitudes();
  }
  linalg::eigenelements_worker<matrix_view<double>, true> ew(h_matrix);

  ew.invoke();
  eigensystem.eigenvalues = ew.values();
  eigensystem.unitary_matrix = h_matrix.transpose();
  gs_energy = std::min(gs_energy, eigensystem.eigenvalues[0]);

  eigensystem.eigenstates.reserve(sp.dimension());
  for (std::size_t e = 0; e < sp.dimension(); ++e) {
   eigensystem.eigenstates.emplace_back(sp);
   eigensystem.eigenstates.back().amplitudes() = h_matrix(e, range());
  }
 }
}
}
