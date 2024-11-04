#include "mpi/alputov_i_most_different_neighbor_elements/include/ops_mpi.hpp"

#include <algorithm>
#include <functional>
#include <random>
#include <thread>
#include <vector>

#include "seq/alputov_i_most_different_neighbor_elements/include/ops_seq.hpp"

using namespace std::chrono_literals;

bool alputov_i_most_different_neighbor_elements_mpi::most_different_neighbor_elements_seq::pre_processing() {
  internal_order_test();

  auto input = std::vector<int>(taskData->inputs_count[0]);
  auto* tmp = reinterpret_cast<int*>(taskData->inputs[0]);
  std::copy(tmp, tmp + taskData->inputs_count[0], input.begin());

  input_ = std::vector<std::pair<int, int>>(input.size() - 1);

  for (size_t i = 1; i < input.size(); ++i) {
    input_[i - 1] = {std::abs(input[i] - input[i - 1]), std::min(input[i], input[i - 1])};
  }

  res = input_[0];

  return true;
}

bool alputov_i_most_different_neighbor_elements_mpi::most_different_neighbor_elements_seq::validation() {
  internal_order_test();
  return taskData->inputs_count[0] > 1 && taskData->outputs_count[0] == 1;
}

bool alputov_i_most_different_neighbor_elements_mpi::most_different_neighbor_elements_seq::run() {
  internal_order_test();

  for (size_t i = 1; i < input_.size(); ++i) {
    if (res.first < input_[i].first) res = input_[i];
  }
  return true;
}

bool alputov_i_most_different_neighbor_elements_mpi::most_different_neighbor_elements_seq::post_processing() {
  internal_order_test();

  reinterpret_cast<int*>(taskData->outputs[0])[0] = res.first;
  return true;
}

bool alputov_i_most_different_neighbor_elements_mpi::most_different_neighbor_elements_mpi::pre_processing() {
  internal_order_test();

  // Init value for output
  res = {INT_MIN, -1};
  return true;
}

bool alputov_i_most_different_neighbor_elements_mpi::most_different_neighbor_elements_mpi::validation() {
  internal_order_test();
  if (world.rank() == 0) {
    // Check count elements of output
    return taskData->inputs_count[0] > 1 && taskData->outputs_count[0] == 1;
  }
  return true;
}

bool alputov_i_most_different_neighbor_elements_mpi::most_different_neighbor_elements_mpi::run() {
  internal_order_test();

  unsigned int delta = 0;
  if (world.rank() == 0) {
    delta = (taskData->inputs_count[0]) / world.size();
    size = taskData->inputs_count[0];
    if (taskData->inputs_count[0] % world.size() > 0u) delta++;
  }
  broadcast(world, delta, 0);

  if (world.rank() == 0) {
    // Init vectors
    input_ = std::vector<int>(world.size() * delta + 2, 0);
    auto* tmp = reinterpret_cast<int*>(taskData->inputs[0]);
    std::copy(tmp, tmp + taskData->inputs_count[0], input_.begin());
    for (int proc = 1; proc < world.size(); proc++) {
      world.send(proc, 0, input_.data() + proc * delta, delta + 1);
    }
  }

  local_input_ = std::vector<int>(delta + 1);
  st = world.rank() * delta;
  if (world.rank() == 0) {
    local_input_ = std::vector<int>(input_.begin(), input_.begin() + delta + 1);
  } else {
    world.recv(0, 0, local_input_.data(), delta + 1);
  }

  std::pair<int, int> local_ans_ = {INT_MIN, -1};
  for (size_t i = 0; i < local_input_.size() - 1 && (i + st) < size - 1; ++i) {
    std::pair<int, int> tmp = {abs(local_input_[i + 1] - local_input_[i]), i + st};
    local_ans_ = std::max(local_ans_, tmp);
  }
  reduce(world, local_ans_, res, boost::mpi::maximum<std::pair<int, int>>(), 0);
  return true;
}

bool alputov_i_most_different_neighbor_elements_mpi::most_different_neighbor_elements_mpi::post_processing() {
  internal_order_test();

  if (world.rank() == 0) {
    reinterpret_cast<int*>(taskData->outputs[0])[0] = res.first;
  }
  return true;
}