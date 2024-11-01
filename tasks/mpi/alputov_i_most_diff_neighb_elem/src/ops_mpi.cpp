// Copyright 2024 Alputov Ivan
#include "mpi/alputov_i_most_diff_neighb_elem/include/ops_mpi.hpp"

#include <algorithm>
#include <iostream>

int alputov_i_most_diff_neighb_elem_mpi::Max_Neighbour_Seq_Pos(const std::vector<int>& data) {
  if (data.size() < 2) {
    return -1;
  }
  int maxDiff = std::abs(data[0] - data[1]);
  int maxIndex = 0;
  for (size_t i = 1; i < data.size() - 1; ++i) {
    int diff = std::abs(data[i] - data[i + 1]);
    if (diff > maxDiff) {
      maxDiff = diff;
      maxIndex = i;
    }
  }
  return maxIndex;
}

int alputov_i_most_diff_neighb_elem_mpi::MPIParallelTask::getElementsPerProcess() const {
  return taskData->inputs_count[0] / world.size();
}

bool alputov_i_most_diff_neighb_elem_mpi::MPISequentialTask::validation() {
  internal_order_test();
  return taskData->inputs_count[0] >= 2 && taskData->outputs_count[0] == 2;
}

bool alputov_i_most_diff_neighb_elem_mpi::MPISequentialTask::pre_processing() {
  internal_order_test();
  if (taskData->inputs.empty() || taskData->inputs[0] == nullptr || taskData->inputs_count.empty() ||
      taskData->inputs_count[0] == 0) {
    throw std::runtime_error("Input data is invalid.");
  }

  int size = taskData->inputs_count[0];
  if (size < 2) return false;
  inputData = std::vector<int>(size);
  memcpy(inputData.data(), taskData->inputs[0], sizeof(int) * size);
  return true;
}

bool alputov_i_most_diff_neighb_elem_mpi::MPISequentialTask::run() {
  internal_order_test();
  int index = Max_Neighbour_Seq_Pos(inputData);
  if (index == -1) {
    result[0] = 0;
    result[1] = 0;
    return false;
  } else {
    result[0] = inputData[index];
    result[1] = inputData[index + 1];
    return true;
  }
}
bool alputov_i_most_diff_neighb_elem_mpi::MPISequentialTask::post_processing() {
  internal_order_test();
  if (taskData->outputs.empty() || taskData->outputs[0] == nullptr || taskData->outputs_count.empty() ||
      taskData->outputs_count[0] == 0) {
    throw std::runtime_error("Output data is invalid.");
  }
  reinterpret_cast<int*>(taskData->outputs[0])[0] = result[0];
  reinterpret_cast<int*>(taskData->outputs[0])[1] = result[1];
  return true;
}

void updateMaxDifferencePair(const int* currentPair, int* maxDiffPair, int* arrayLength, MPI_Datatype* dataType) {
  if (currentPair[2] > maxDiffPair[2]) {
    maxDiffPair[0] = currentPair[0];
    maxDiffPair[1] = currentPair[1];
    maxDiffPair[2] = currentPair[2];
  }
}

int* findMaxDifference(const std::vector<int>& vec) {
  if (vec.size() < 2) {
    return NULL;
  }
  int* max_dif = new int[3];
  max_dif[0] = vec[0];
  max_dif[1] = vec[1];
  max_dif[2] = std::abs(vec[1] - vec[0]);
  for (size_t i = 1; i < vec.size() - 1; ++i) {
    int dif = std::abs(vec[i + 1] - vec[i]);
    if (dif > max_dif[2]) {
      max_dif[0] = vec[i];
      max_dif[1] = vec[i + 1];
      max_dif[2] = dif;
    }
  }
  return max_dif;
}

bool alputov_i_most_diff_neighb_elem_mpi::MPIParallelTask::validation() {
  internal_order_test();
  if (world.rank() == 0) {
    return taskData->inputs_count[0] >= 2 && taskData->outputs_count[0] == 2 && getElementsPerProcess() >= 2;
  }
  return true;
}

bool alputov_i_most_diff_neighb_elem_mpi::MPIParallelTask::pre_processing() {
  internal_order_test();
  int data_chunk = 0;
  if (world.rank() == 0) {
    data_chunk = taskData->inputs_count[0] / world.size();
  }
  boost::mpi::broadcast(world, data_chunk, 0);

  if (world.rank() == 0) {
    inputData = std::vector<int>(taskData->inputs_count[0]);
    memcpy(inputData.data(), taskData->inputs[0], sizeof(int) * taskData->inputs_count[0]);
    for (int proc = 1; proc < world.size(); ++proc) {
      world.send(proc, 0, inputData.data() + proc * data_chunk, data_chunk);
    }
  }
  localData = std::vector<int>(data_chunk);
  if (world.rank() == 0) {
    localData = std::vector<int>(inputData.begin(), inputData.begin() + data_chunk);
  } else {
    world.recv(0, 0, localData.data(), data_chunk);
  }
  result[0] = 0;
  result[1] = 1;
  localMaxDiff[0] = localMaxDiff[1] = localMaxDiff[2] = 0;
  return true;
}

bool alputov_i_most_diff_neighb_elem_mpi::MPIParallelTask::run() {
  internal_order_test();

  int* localResult = findMaxDifference(localData);
  if (localResult == nullptr) {
    return false;
  }
  localMaxDiff[0] = localResult[0];
  localMaxDiff[1] = localResult[1];
  localMaxDiff[2] = localResult[2];
  delete[] localResult;

  int lastElement = localData.back();
  int firstElement = localData.front();
  int nextFirstElement = 0;
  int prevLastElement = 0;

  if (world.rank() == 0) {
    if (world.size() > 1) {
      world.send(world.rank() + 1, 0, lastElement);
      world.recv(world.rank() + 1, 0, nextFirstElement);
    }
  } else if (world.rank() == world.size() - 1) {
    world.send(world.rank() - 1, 0, firstElement);
    world.recv(world.rank() - 1, 0, prevLastElement);
  } else {
    world.send(world.rank() + 1, 0, lastElement);
    world.recv(world.rank() + 1, 0, nextFirstElement);
    world.send(world.rank() - 1, 0, firstElement);
    world.recv(world.rank() - 1, 0, prevLastElement);
  }

  if (world.rank() > 0) {
    int diff = std::abs(firstElement - prevLastElement);
    if (diff > localMaxDiff[2]) {
      localMaxDiff[0] = prevLastElement;
      localMaxDiff[1] = firstElement;
      localMaxDiff[2] = diff;
    }
  }
  if (world.rank() < world.size() - 1) {
    int diff = std::abs(nextFirstElement - lastElement);
    if (diff > localMaxDiff[2]) {
      localMaxDiff[0] = lastElement;
      localMaxDiff[1] = nextFirstElement;
      localMaxDiff[2] = diff;
    }
  }
  int globalDataArr[3] = {0, 0, 0};
  MPI_Op customOperation;
  MPI_Op_create(reinterpret_cast<MPI_User_function*>(&updateMaxDifferencePair), 1, &customOperation);
  MPI_Reduce(localMaxDiff, globalDataArr, 3, MPI_INT, customOperation, 0, MPI_COMM_WORLD);
  if (world.rank() == 0) {
    result[0] = globalDataArr[0];
    result[1] = globalDataArr[1];
  }
  MPI_Op_free(&customOperation);
  return true;
}

bool alputov_i_most_diff_neighb_elem_mpi::MPIParallelTask::post_processing() {
  internal_order_test();
  if (world.rank() == 0) {
    reinterpret_cast<int*>(taskData->outputs[0])[0] = result[0];
    reinterpret_cast<int*>(taskData->outputs[0])[1] = result[1];
  }
  return true;
}