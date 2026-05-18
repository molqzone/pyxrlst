import os
import time
import uuid

import pytest

pyxrlst = pytest.importorskip("pyxrlst")


def _topic_name(prefix: str) -> str:
    return f"{prefix}_{os.getpid()}_{uuid.uuid4().hex[:8]}"


def _make_config():
    cfg = pyxrlst.LinuxSharedTopicConfig()
    cfg.slot_num = 8
    cfg.subscriber_num = 2
    cfg.queue_num = 8
    return cfg


@pytest.mark.skipif(os.name != "posix", reason="Linux shared topic tests require POSIX")
def test_publish_bytes_and_wait_roundtrip():
    topic_name = _topic_name("pyxrlst_roundtrip")
    assert pyxrlst.LinuxSharedTopic.remove(topic_name) == pyxrlst.ErrorCode.OK

    topic = pyxrlst.LinuxSharedTopic(topic_name, _make_config())
    subscriber = pyxrlst.Subscriber(topic_name)

    assert topic.valid()
    assert subscriber.valid()

    payload = b"hello from pytest"
    assert topic.publish(payload) == pyxrlst.ErrorCode.OK

    status, data = subscriber.wait(1000)
    assert status == pyxrlst.ErrorCode.OK
    assert data.valid()
    assert data.to_bytes() == payload
    assert data.sequence() > 0

    data.reset()
    subscriber.reset()
    assert pyxrlst.LinuxSharedTopic.remove(topic_name) == pyxrlst.ErrorCode.OK


@pytest.mark.skipif(os.name != "posix", reason="Linux shared topic tests require POSIX")
def test_publish_buffer_and_zero_copy_view():
    np = pytest.importorskip("numpy")

    topic_name = _topic_name("pyxrlst_buffer")
    assert pyxrlst.LinuxSharedTopic.remove(topic_name) == pyxrlst.ErrorCode.OK

    topic = pyxrlst.LinuxSharedTopic(topic_name, _make_config())
    subscriber = pyxrlst.Subscriber(topic_name)

    src = np.arange(32, dtype=np.uint8)
    assert topic.publish_buffer(src) == pyxrlst.ErrorCode.OK

    status, data = subscriber.wait(1000)
    assert status == pyxrlst.ErrorCode.OK

    view = memoryview(data)
    arr = np.asarray(view, dtype=np.uint8)
    assert arr.tolist() == src.tolist()

    arr[0] = 99
    assert data.to_bytes()[0] == 99

    data.reset()
    subscriber.reset()
    assert pyxrlst.LinuxSharedTopic.remove(topic_name) == pyxrlst.ErrorCode.OK


@pytest.mark.skipif(os.name != "posix", reason="Linux shared topic tests require POSIX")
def test_create_data_copy_from_publish():
    np = pytest.importorskip("numpy")

    topic_name = _topic_name("pyxrlst_createdata")
    assert pyxrlst.LinuxSharedTopic.remove(topic_name) == pyxrlst.ErrorCode.OK

    topic = pyxrlst.LinuxSharedTopic(topic_name, _make_config())
    subscriber = pyxrlst.Subscriber(topic_name)

    src = np.array([3, 1, 4, 1, 5, 9], dtype=np.uint8)
    status, data = topic.create_data()
    assert status == pyxrlst.ErrorCode.OK
    data.copy_from(src)
    assert memoryview(data).tobytes() == bytes(src)

    assert topic.publish_data(data) == pyxrlst.ErrorCode.OK

    status, received = subscriber.wait(1000)
    assert status == pyxrlst.ErrorCode.OK
    assert received.to_bytes() == bytes(src)

    received.reset()
    data.reset()
    subscriber.reset()
    assert pyxrlst.LinuxSharedTopic.remove(topic_name) == pyxrlst.ErrorCode.OK
