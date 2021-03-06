/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_COMPILER_XLA_PYTHON_LOCAL_CLIENT_H_
#define TENSORFLOW_COMPILER_XLA_PYTHON_LOCAL_CLIENT_H_

#include <string>
#include <vector>

#include "absl/types/span.h"
#include "include/pybind11/pybind11.h"
#include "tensorflow/compiler/xla/client/executable_build_options.h"
#include "tensorflow/compiler/xla/client/local_client.h"
#include "tensorflow/compiler/xla/client/xla_computation.h"
#include "tensorflow/compiler/xla/service/computation_placer.h"
#include "tensorflow/compiler/xla/service/shaped_buffer.h"
#include "tensorflow/compiler/xla/shape.h"
#include "tensorflow/compiler/xla/status.h"
#include "tensorflow/compiler/xla/statusor.h"

namespace xla {
namespace xla_python {

// Registers a 'fn_capsule' as a CPU custom call target.
// 'fn_capsule' is a void* pointer encapsulated in a PyCapsule object, with name
// "xla._CPU_CUSTOM_CALL_TARGET".
Status RegisterCpuCustomCallTarget(const std::string& fn_name,
                                   pybind11::capsule capsule);

// Initializes a local XLA client for `platform_name`. Returns an error if no
// such platform exists, or if the platform has no visible devices.
StatusOr<LocalClient*> GetLocalClient(const std::string& platform_name);

// Represents a reference to literals that live in a device-allocated buffer via
// XLA. Specifically, wraps a ScopedShapedBuffer produced by transferring a
// literal to device via the local client.
class LocalShapedBuffer {
 public:
  static StatusOr<LocalShapedBuffer> FromPython(
      const pybind11::object& argument, LocalClient* client,
      int device_ordinal);

  LocalShapedBuffer() = default;
  LocalShapedBuffer(ScopedShapedBuffer shaped_buffer, LocalClient* client);
  StatusOr<pybind11::object> ToPython() const;
  const Shape& shape() const;
  const ScopedShapedBuffer* shaped_buffer() const;

  // Transfers ownership of the encapsulated ShapedBuffer to the caller,
  // analogous to std::unique_ptr::release().
  ScopedShapedBuffer Release();

  void Delete() {
    shaped_buffer_ = absl::nullopt;
    client_ = nullptr;
  }

  // Destructures a tuple-valued LocalShapedBuffer into its constituent
  // elements in LocalShapedBufferTuple form.
  StatusOr<std::vector<LocalShapedBuffer>> DestructureTuple();

 private:
  absl::optional<ScopedShapedBuffer> shaped_buffer_;
  LocalClient* client_ = nullptr;
};

// Represents a compiled computation that can be executed given handles to
// device-allocated literals. Wraps an XLA LocalExecutable.
class LocalExecutableWrapper {
 public:
  // Compiles a computation to an executable.
  static StatusOr<std::unique_ptr<LocalExecutableWrapper>> Compile(
      const XlaComputation& computation,
      const std::vector<Shape>& argument_shapes,
      const ExecutableBuildOptions* build_options, LocalClient* client);

  LocalExecutableWrapper(std::unique_ptr<LocalExecutable> executable,
                         DeviceAssignment device_assignment,
                         LocalClient* client);

  int num_replicas() const {
    return executable_->build_options().num_replicas();
  }

  // Returns the device ordinals to which each replica is assigned.
  std::vector<int> DeviceOrdinals() const;

  const DeviceAssignment& device_assignment() const {
    return device_assignment_;
  }

  StatusOr<LocalShapedBuffer> Execute(
      absl::Span<LocalShapedBuffer* const> argument_handles);

  // Execute on many replicas. Takes a sequence of argument lists (one argument
  // list per replica) and returns a tuple of results (one result per replica).
  // The number of argument lists must be equal to the replica count.
  StatusOr<std::vector<LocalShapedBuffer>> ExecutePerReplica(
      absl::Span<const std::vector<LocalShapedBuffer*>> argument_handles);

  void Delete() { executable_ = nullptr; }

 private:
  std::unique_ptr<LocalExecutable> executable_;
  const DeviceAssignment device_assignment_;
  LocalClient* const client_;
};

// Converts a computation to a serialized HloModuleProto
StatusOr<pybind11::bytes> GetComputationSerializedProto(
    const XlaComputation& computation);

// Converts a computation to textual HLO form.
StatusOr<std::string> GetComputationHloText(const XlaComputation& computation);

// Converts a computation to HLO dot graph form.
StatusOr<std::string> GetComputationHloDotGraph(
    const XlaComputation& computation);

}  // namespace xla_python
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_PYTHON_LOCAL_CLIENT_H_
