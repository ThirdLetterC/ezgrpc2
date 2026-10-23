#!/usr/bin/env python3
"""Tests single_threaded.bin (poll mode) and multi_threaded.bin (replies from
worker threads) with raw-bytes gRPC calls.

  python3 test_examples.py single <port>
  python3 test_examples.py multi <port>
"""
import sys
from concurrent.futures import ThreadPoolExecutor

import grpc


def test_single(channel):
    echo = channel.unary_unary("/example.Echo/Echo")
    collect = channel.stream_unary("/example.Echo/Collect")
    assert echo(b"hello", timeout=5) == b"hello"
    assert echo(b"", timeout=5) == b""
    assert collect(iter([b"a", b"b", b"c"]), timeout=5) == b"3"
    assert collect(iter([]), timeout=5) == b"0"
    try:
        channel.unary_unary("/example.Echo/Missing")(b"x", timeout=5)
        raise AssertionError("expected UNIMPLEMENTED")
    except grpc.RpcError as err:
        assert err.code() == grpc.StatusCode.UNIMPLEMENTED, err.code()
    print("[test] poll mode: echo, client streaming and unknown methods OK")


def test_multi(channel):
    reverse = channel.unary_unary("/example.Work/Reverse")
    assert reverse(b"abc", timeout=5) == b"cba"
    # many calls at once, answered by the worker threads
    inputs = [f"message {i}".encode() for i in range(200)]
    with ThreadPoolExecutor(max_workers=32) as pool:
        outputs = list(pool.map(lambda m: reverse(m, timeout=10), inputs))
    assert outputs == [m[::-1] for m in inputs]
    print("[test] worker threads: 200 concurrent calls OK")


def main():
    which, port = sys.argv[1], sys.argv[2]
    with grpc.insecure_channel(f"localhost:{port}") as channel:
        grpc.channel_ready_future(channel).result(timeout=5)
        {"single": test_single, "multi": test_multi}[which](channel)
    return 0


if __name__ == "__main__":
    sys.exit(main())
