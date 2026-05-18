#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "libxr.hpp"

namespace py = pybind11;

namespace
{

constexpr std::size_t MAX_PAYLOAD_SIZE = 4096;

struct PyTopicPayload
{
  uint32_t size = 0;
  std::array<uint8_t, MAX_PAYLOAD_SIZE> bytes = {};
};

using Topic = LibXR::LinuxSharedTopic<PyTopicPayload>;
using SharedData = Topic::Data;
using Subscriber = Topic::SyncSubscriber;

PyTopicPayload MakePayload(py::bytes data)
{
  std::string buffer = data;
  if (buffer.size() > MAX_PAYLOAD_SIZE)
  {
    throw py::value_error("payload exceeds MAX_PAYLOAD_SIZE");
  }

  PyTopicPayload payload;
  payload.size = static_cast<uint32_t>(buffer.size());
  std::copy(buffer.begin(), buffer.end(), payload.bytes.begin());
  return payload;
}

PyTopicPayload MakePayloadFromBuffer(const py::buffer& buffer)
{
  py::buffer_info info = buffer.request();
  if (info.ndim < 1)
  {
    throw py::value_error("buffer must have at least one dimension");
  }
  if (info.itemsize <= 0)
  {
    throw py::value_error("buffer itemsize must be positive");
  }
  if (info.strides.empty())
  {
    throw py::value_error("buffer must expose strides");
  }
  if (info.strides.back() != info.itemsize)
  {
    throw py::value_error("buffer must be C-contiguous in the last dimension");
  }

  std::size_t byte_size = 1;
  for (py::ssize_t dim : info.shape)
  {
    if (dim < 0)
    {
      throw py::value_error("buffer shape must be non-negative");
    }
    byte_size *= static_cast<std::size_t>(dim);
  }
  byte_size *= static_cast<std::size_t>(info.itemsize);

  if (byte_size > MAX_PAYLOAD_SIZE)
  {
    throw py::value_error("payload exceeds MAX_PAYLOAD_SIZE");
  }

  PyTopicPayload payload;
  payload.size = static_cast<uint32_t>(byte_size);
  std::memcpy(payload.bytes.data(), info.ptr, byte_size);
  return payload;
}

py::bytes PayloadToBytes(const PyTopicPayload& payload)
{
  return py::bytes(reinterpret_cast<const char*>(payload.bytes.data()), payload.size);
}

py::buffer_info SharedDataBufferInfo(SharedData& self)
{
  auto* data = self.GetData();
  if (data == nullptr)
  {
    throw py::value_error("shared data is not bound to a payload");
  }

  return py::buffer_info(data->bytes.data(),
                         sizeof(uint8_t),
                         py::format_descriptor<uint8_t>::format(),
                         1,
                         {static_cast<py::ssize_t>(data->size)},
                         {static_cast<py::ssize_t>(sizeof(uint8_t))});
}

py::buffer_info PayloadBufferInfo(PyTopicPayload& self)
{
  return py::buffer_info(self.bytes.data(),
                         sizeof(uint8_t),
                         py::format_descriptor<uint8_t>::format(),
                         1,
                         {static_cast<py::ssize_t>(self.size)},
                         {static_cast<py::ssize_t>(sizeof(uint8_t))});
}

void CopyBufferToSharedData(SharedData& self, const py::buffer& buffer)
{
  auto* data = self.GetData();
  if (data == nullptr)
  {
    throw py::value_error("shared data is not bound to a payload");
  }

  py::buffer_info info = buffer.request();
  if (info.ndim < 1)
  {
    throw py::value_error("buffer must have at least one dimension");
  }
  if (info.itemsize <= 0)
  {
    throw py::value_error("buffer itemsize must be positive");
  }

  std::size_t byte_size = 1;
  for (py::ssize_t dim : info.shape)
  {
    if (dim < 0)
    {
      throw py::value_error("buffer shape must be non-negative");
    }
    byte_size *= static_cast<std::size_t>(dim);
  }
  byte_size *= static_cast<std::size_t>(info.itemsize);

  if (byte_size > MAX_PAYLOAD_SIZE)
  {
    throw py::value_error("payload exceeds MAX_PAYLOAD_SIZE");
  }

  py::array array = py::array::ensure(buffer);
  if (!array)
  {
    throw py::value_error("failed to access buffer data");
  }

  py::buffer_info contiguous = array.request();
  std::memcpy(data->bytes.data(), contiguous.ptr, byte_size);
  data->size = static_cast<uint32_t>(byte_size);
}

}  // namespace

PYBIND11_MODULE(pyxrlst, m)
{
  m.doc() = "LibXR LinuxSharedTopic wrapper";
  m.attr("MAX_PAYLOAD_SIZE") = py::int_(MAX_PAYLOAD_SIZE);

  py::enum_<LibXR::ErrorCode>(m, "ErrorCode")
      .value("PENDING", LibXR::ErrorCode::PENDING)
      .value("OK", LibXR::ErrorCode::OK)
      .value("FAILED", LibXR::ErrorCode::FAILED)
      .value("INIT_ERR", LibXR::ErrorCode::INIT_ERR)
      .value("ARG_ERR", LibXR::ErrorCode::ARG_ERR)
      .value("STATE_ERR", LibXR::ErrorCode::STATE_ERR)
      .value("SIZE_ERR", LibXR::ErrorCode::SIZE_ERR)
      .value("CHECK_ERR", LibXR::ErrorCode::CHECK_ERR)
      .value("NOT_SUPPORT", LibXR::ErrorCode::NOT_SUPPORT)
      .value("NOT_FOUND", LibXR::ErrorCode::NOT_FOUND)
      .value("NO_RESPONSE", LibXR::ErrorCode::NO_RESPONSE)
      .value("NO_MEM", LibXR::ErrorCode::NO_MEM)
      .value("NO_BUFF", LibXR::ErrorCode::NO_BUFF)
      .value("TIMEOUT", LibXR::ErrorCode::TIMEOUT)
      .value("EMPTY", LibXR::ErrorCode::EMPTY)
      .value("FULL", LibXR::ErrorCode::FULL)
      .value("BUSY", LibXR::ErrorCode::BUSY)
      .value("PTR_NULL", LibXR::ErrorCode::PTR_NULL)
      .value("OUT_OF_RANGE", LibXR::ErrorCode::OUT_OF_RANGE);

  py::enum_<LibXR::LinuxSharedSubscriberMode>(m, "LinuxSharedSubscriberMode")
      .value("BROADCAST_FULL", LibXR::LinuxSharedSubscriberMode::BROADCAST_FULL)
      .value("BROADCAST_DROP_OLD", LibXR::LinuxSharedSubscriberMode::BROADCAST_DROP_OLD)
      .value("BALANCE_RR", LibXR::LinuxSharedSubscriberMode::BALANCE_RR);

  py::class_<LibXR::MicrosecondTimestamp>(m, "MicrosecondTimestamp")
      .def(py::init<>())
      .def(py::init<uint64_t>())
      .def("__int__", [](const LibXR::MicrosecondTimestamp& self) {
        return static_cast<uint64_t>(self);
      });

  py::class_<LibXR::LinuxSharedTopicConfig>(m, "LinuxSharedTopicConfig")
      .def(py::init<>())
      .def_readwrite("slot_num", &LibXR::LinuxSharedTopicConfig::slot_num)
      .def_readwrite("subscriber_num", &LibXR::LinuxSharedTopicConfig::subscriber_num)
      .def_readwrite("queue_num", &LibXR::LinuxSharedTopicConfig::queue_num);

  py::class_<PyTopicPayload>(m, "Payload", py::buffer_protocol())
      .def(py::init<>())
      .def_buffer([](PyTopicPayload& self) { return PayloadBufferInfo(self); })
      .def_property("data",
                    [](const PyTopicPayload& self) { return PayloadToBytes(self); },
                    [](PyTopicPayload& self, py::bytes data) { self = MakePayload(data); })
      .def_property("size",
                    [](const PyTopicPayload& self) { return self.size; },
                    [](PyTopicPayload& self, uint32_t size) {
                      if (size > MAX_PAYLOAD_SIZE)
                      {
                        throw py::value_error("payload size exceeds MAX_PAYLOAD_SIZE");
                      }
                      self.size = size;
                    })
      .def("copy_from", [](PyTopicPayload& self, const py::buffer& buffer) {
        self = MakePayloadFromBuffer(buffer);
      });

  py::class_<SharedData>(m, "SharedData", py::buffer_protocol())
      .def(py::init<>())
      .def_buffer([](SharedData& self) { return SharedDataBufferInfo(self); })
      .def("valid", &SharedData::Valid)
      .def("empty", &SharedData::Empty)
      .def("sequence", &SharedData::GetSequence)
      .def("timestamp", &SharedData::GetTimestamp)
      .def("reset", &SharedData::Reset)
      .def("copy_from", [](SharedData& self, const py::buffer& buffer) {
        CopyBufferToSharedData(self, buffer);
      })
      .def("to_bytes", [](SharedData& self) {
        auto* data = self.GetData();
        if (data == nullptr)
        {
          return py::bytes();
        }
        return PayloadToBytes(*data);
      })
      .def_property(
          "payload",
          [](SharedData& self) {
            auto* data = self.GetData();
            if (data == nullptr)
            {
              return PyTopicPayload{};
            }
            return *data;
          },
          [](SharedData& self, const PyTopicPayload& payload) {
            auto* data = self.GetData();
            if (data == nullptr)
            {
              throw py::value_error("shared data is not bound to a payload");
            }
            *data = payload;
          });

  py::class_<Subscriber>(m, "Subscriber")
      .def(py::init<>())
      .def(py::init<const char*, LibXR::LinuxSharedSubscriberMode>(),
           py::arg("name"),
           py::arg("mode") = LibXR::LinuxSharedSubscriberMode::BROADCAST_FULL)
      .def("valid", &Subscriber::Valid)
      .def("wait",
           [](Subscriber& self, uint32_t timeout_ms) {
             SharedData data;
             const auto ans = self.Wait(data, timeout_ms);
             return py::make_tuple(ans, std::move(data));
           },
           py::arg("timeout_ms") = UINT32_MAX)
      .def("release", &Subscriber::Release)
      .def("reset", &Subscriber::Reset)
      .def("pending_num", &Subscriber::GetPendingNum)
      .def("drop_num", &Subscriber::GetDropNum)
      .def("sequence", &Subscriber::GetSequence)
      .def("timestamp", &Subscriber::GetTimestamp);

  py::class_<Topic>(m, "LinuxSharedTopic")
      .def(py::init<const char*>(), py::arg("topic_name"))
      .def(py::init<const char*, const LibXR::LinuxSharedTopicConfig&>(),
           py::arg("topic_name"),
           py::arg("config"))
      .def("valid", &Topic::Valid)
      .def("error", &Topic::GetError)
      .def("subscriber_num", &Topic::GetSubscriberNum)
      .def("publish_failed_num", &Topic::GetPublishFailedNum)
      .def("create_data", [](Topic& self) {
        SharedData data;
        return py::make_tuple(self.CreateData(data), std::move(data));
      })
      .def("publish_buffer",
           [](Topic& self, const py::buffer& buffer) {
             SharedData data;
             const auto status = self.CreateData(data);
             if (status != LibXR::ErrorCode::OK)
             {
               return status;
             }
             CopyBufferToSharedData(data, buffer);
             return self.Publish(data);
           })
      .def("publish_data", [](Topic& self, SharedData& data) { return self.Publish(data); })
      .def("publish",
           [](Topic& self, py::bytes data) {
              return self.Publish(MakePayload(data));
           })
      .def_static(
          "remove",
          static_cast<LibXR::ErrorCode (*)(const char*)>(&Topic::Remove),
          py::arg("topic_name"));
}
