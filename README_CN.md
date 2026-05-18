# PyXRLST

`PyXRLST` 是 `LibXR::LinuxSharedTopic` 的 Python 封装，目标是让 Python 侧程序直接接入 LibXR 的 Linux 共享内存 Topic。

## 功能概览

- 提供 `LinuxSharedTopic`、`Subscriber`、`SharedData`、`LinuxSharedTopicConfig` 等基础绑定。
- 支持发布 `bytes`。
- 支持发布 Python buffer 对象，如 `memoryview`、`bytearray`、`numpy.ndarray`。
- `SharedData` 支持 Python buffer protocol，可直接转成 `memoryview` 或 `numpy` 视图。
- 暴露 `ErrorCode`、`LinuxSharedSubscriberMode`、`MicrosecondTimestamp`。

## 零拷贝语义

- 订阅侧读取共享内存时，`SharedData` 本身直接持有 LibXR 的共享内存槽位。
- 当你对 `SharedData` 使用 `memoryview(shared_data)` 或 `numpy.asarray(shared_data)` 时，读取路径可以做到零拷贝视图。
- 当你调用 `SharedData.to_bytes()` 或访问会生成 Python `bytes` 的接口时，会发生一次拷贝。
- 发布侧 `publish_buffer(buffer)` 会把 Python buffer 直接拷贝到最终共享内存槽位，省掉中间临时 `bytes`/payload 对象的一次额外拷贝。
- 发布侧仍然不是“绝对零拷贝”，因为 Python 自己的内存并不是共享内存槽位本体。

## 生命周期约束

- `SharedData` 持有共享内存槽位。
- `memoryview(shared_data)` 或任何基于它创建的 NumPy 视图，不能在 `shared_data.reset()` 之后继续使用。
- 同样，若 `SharedData` 来自订阅者，则在 `Subscriber.release()` 之后，不应继续使用之前得到的视图。
- 如果继续使用，行为是未定义的，本质上等同于访问失效内存。

## 当前数据模型

当前绑定没有暴露任意 C++ 模板类型，而是固定成一个字节载荷模型：

```cpp
struct PyTopicPayload
{
  uint32_t size = 0;
  std::array<uint8_t, 4096> bytes = {};
};
```

- `MAX_PAYLOAD_SIZE == 4096`
- 有效负载长度保存在 `size`
- 原始字节存放在 `bytes`

## Python API

### `LinuxSharedTopicConfig`

字段：

- `slot_num`
- `subscriber_num`
- `queue_num`

### `LinuxSharedTopic`

构造：

- `LinuxSharedTopic(topic_name: str)`
- `LinuxSharedTopic(topic_name: str, config: LinuxSharedTopicConfig)`

方法：

- `valid() -> bool`
- `error() -> ErrorCode`
- `subscriber_num() -> int`
- `publish_failed_num() -> int`
- `create_data() -> tuple[ErrorCode, SharedData]`
- `publish(data: bytes) -> ErrorCode`
- `publish_buffer(buffer) -> ErrorCode`
- `remove(topic_name: str) -> ErrorCode`

### `Subscriber`

构造：

- `Subscriber()`
- `Subscriber(name: str, mode: LinuxSharedSubscriberMode = BROADCAST_FULL)`

方法：

- `valid() -> bool`
- `wait(timeout_ms: int = UINT32_MAX) -> tuple[ErrorCode, SharedData]`
- `release()`
- `reset()`
- `pending_num() -> int`
- `drop_num() -> int`
- `sequence() -> int`
- `timestamp() -> MicrosecondTimestamp`

### `SharedData`

方法：

- `valid() -> bool`
- `empty() -> bool`
- `sequence() -> int`
- `timestamp() -> MicrosecondTimestamp`
- `reset()`
- `copy_from(buffer)`
- `to_bytes() -> bytes`

属性：

- `payload: Payload`

buffer protocol:

- `memoryview(shared_data)` 直接映射到当前有效 payload 区域。
- `numpy.asarray(shared_data, dtype=np.uint8)` 可构造零拷贝视图。

### `Payload`

方法：

- `copy_from(buffer)`

属性：

- `data: bytes`
- `size: int`

## 示例

### 发布 `bytes`

```python
import pyxrlst

cfg = pyxrlst.LinuxSharedTopicConfig()
cfg.slot_num = 64
cfg.subscriber_num = 8
cfg.queue_num = 64

topic = pyxrlst.LinuxSharedTopic("demo_topic", cfg)
status = topic.publish(b"hello")
print(status)
```

### 发布 NumPy 数据

```python
import numpy as np
import pyxrlst

topic = pyxrlst.LinuxSharedTopic("demo_topic")
arr = np.arange(16, dtype=np.uint8)

status = topic.publish_buffer(arr)
print(status)
```

### 零拷贝订阅视图

```python
import numpy as np
import pyxrlst

sub = pyxrlst.Subscriber("demo_topic")
status, data = sub.wait(1000)

if status == pyxrlst.ErrorCode.OK:
    view = memoryview(data)
    arr = np.asarray(view, dtype=np.uint8)
    print(arr.shape, arr[:8])

    # 注意：release/reset 之后不能再使用 view/arr。
    data.reset()
```

## 构建说明

### 使用 `uv` 管理开发环境

仓库已经加入了 `pyproject.toml` 和开发依赖组。

Windows:

```powershell
uv venv .venv
uv sync --group dev --no-install-project
```

Linux / WSL:

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -U pip pybind11 scikit-build-core build
```

### 构建 wheel

```bash
python -m build
```

### 依赖要求

- C++20 编译器
- CMake >= 3.12
- Python 开发头文件
- `pybind11`
- `scikit-build-core`
