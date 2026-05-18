# PyXRLST

`PyXRLST` is a Python wrapper around `LibXR::LinuxSharedTopic`, intended to let Python applications integrate directly with LibXR's Linux shared-memory topic API.

## Features

- Exposes the core bindings: `LinuxSharedTopic`, `Subscriber`, `SharedData`, and `LinuxSharedTopicConfig`.
- Supports publishing `bytes`.
- Supports publishing Python buffer objects such as `memoryview`, `bytearray`, and `numpy.ndarray`.
- `SharedData` implements the Python buffer protocol, so it can be viewed directly via `memoryview` or NumPy.
- Exposes `ErrorCode`, `LinuxSharedSubscriberMode`, and `MicrosecondTimestamp`.

## Zero-Copy Semantics

- On the subscriber side, `SharedData` directly holds a LibXR shared-memory slot.
- When you use `memoryview(shared_data)` or `numpy.asarray(shared_data)`, reads can be zero-copy views.
- When you call `SharedData.to_bytes()` or any API that materializes Python `bytes`, a copy occurs.
- On the publisher side, `publish_buffer(buffer)` copies Python buffer data directly into the final shared-memory slot, avoiding an extra intermediate copy through a temporary payload object.
- Publisher-side transfer is still not "absolute zero-copy", because Python-owned memory is not itself the shared-memory slot.

## Lifetime Rules

- `SharedData` owns a shared-memory slot.
- A `memoryview(shared_data)` or any NumPy view derived from it must not outlive `shared_data.reset()`.
- Likewise, if the `SharedData` came from a subscriber, views created from it must not be used after `Subscriber.release()`.
- Using such a view afterward is undefined behavior, effectively a dangling-memory access.

## Current Data Model

The current binding does not expose arbitrary C++ template types. Instead, it uses a fixed byte-payload model:

```cpp
struct PyTopicPayload
{
  uint32_t size = 0;
  std::array<uint8_t, 4096> bytes = {};
};
```

- `MAX_PAYLOAD_SIZE == 4096`
- The valid payload length is stored in `size`
- Raw bytes live in `bytes`

## Python API

### `LinuxSharedTopicConfig`

Fields:

- `slot_num`
- `subscriber_num`
- `queue_num`

### `LinuxSharedTopic`

Constructors:

- `LinuxSharedTopic(topic_name: str)`
- `LinuxSharedTopic(topic_name: str, config: LinuxSharedTopicConfig)`

Methods:

- `valid() -> bool`
- `error() -> ErrorCode`
- `subscriber_num() -> int`
- `publish_failed_num() -> int`
- `create_data() -> tuple[ErrorCode, SharedData]`
- `publish(data: bytes) -> ErrorCode`
- `publish_buffer(buffer) -> ErrorCode`
- `remove(topic_name: str) -> ErrorCode`

### `Subscriber`

Constructors:

- `Subscriber()`
- `Subscriber(name: str, mode: LinuxSharedSubscriberMode = BROADCAST_FULL)`

Methods:

- `valid() -> bool`
- `wait(timeout_ms: int = UINT32_MAX) -> tuple[ErrorCode, SharedData]`
- `release()`
- `reset()`
- `pending_num() -> int`
- `drop_num() -> int`
- `sequence() -> int`
- `timestamp() -> MicrosecondTimestamp`

### `SharedData`

Methods:

- `valid() -> bool`
- `empty() -> bool`
- `sequence() -> int`
- `timestamp() -> MicrosecondTimestamp`
- `reset()`
- `copy_from(buffer)`
- `to_bytes() -> bytes`

Properties:

- `payload: Payload`

Buffer protocol:

- `memoryview(shared_data)` maps directly to the currently valid payload range.
- `numpy.asarray(shared_data, dtype=np.uint8)` can create a zero-copy view.

### `Payload`

Methods:

- `copy_from(buffer)`

Properties:

- `data: bytes`
- `size: int`

## Examples

### Publish `bytes`

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

### Publish NumPy Data

```python
import numpy as np
import pyxrlst

topic = pyxrlst.LinuxSharedTopic("demo_topic")
arr = np.arange(16, dtype=np.uint8)

status = topic.publish_buffer(arr)
print(status)
```

### Zero-Copy Subscriber View

```python
import numpy as np
import pyxrlst

sub = pyxrlst.Subscriber("demo_topic")
status, data = sub.wait(1000)

if status == pyxrlst.ErrorCode.OK:
    view = memoryview(data)
    arr = np.asarray(view, dtype=np.uint8)
    print(arr.shape, arr[:8])

    # Important: do not use view/arr after release/reset.
    data.reset()
```

## Build Notes

### Use `uv` for the Development Environment

The repository includes `pyproject.toml` and a dev dependency group.

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

### Build the Wheel

```bash
python -m build
```

### Requirements

- A C++20 compiler
- CMake >= 3.12
- Python development headers
- `pybind11`
- `scikit-build-core`
