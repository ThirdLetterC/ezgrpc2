#!/usr/bin/env python3
"""Tests the FlatBuffers Greeter with the official Python FlatBuffers runtime.

The messages are encoded and decoded by hand with the `flatbuffers` package,
so no generated code (and no flatc) is needed. They follow greeter.fbs:

  table HelloRequest { name: string; count: int = 3; }   // fields 0, 1
  table HelloReply   { message: string; index: int; }     // fields 0, 1
"""
import sys

import flatbuffers
import grpc
from flatbuffers import encode, number_types, packer, table


def encode_hello_request(name, count=None):
    builder = flatbuffers.Builder(64)
    name_offset = builder.CreateString(name)
    builder.StartObject(2)
    builder.PrependUOffsetTRelativeSlot(0, name_offset, 0)
    if count is not None:
        builder.PrependInt32Slot(1, count, 3)
    builder.Finish(builder.EndObject())
    return bytes(builder.Output())


def decode_hello_reply(buf):
    tab = table.Table(buf, encode.Get(packer.uoffset, buf, 0))
    offset = tab.Offset(4)
    message = tab.String(offset + tab.Pos).decode() if offset else ""
    offset = tab.Offset(6)
    index = tab.Get(number_types.Int32Flags, offset + tab.Pos) if offset else 0
    return message, index


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else "localhost:19029"
    with grpc.insecure_channel(target) as channel:
        # no serializers: requests and replies are passed as raw FlatBuffers bytes
        say_hello = channel.unary_unary("/greeter.Greeter/SayHello")
        say_many_hellos = channel.unary_stream("/greeter.Greeter/SayManyHellos")

        print("[client] 1. SayHello (unary)")
        message, _ = decode_hello_reply(say_hello(encode_hello_request("FlatBuffers"), timeout=5))
        assert message == "Hello, FlatBuffers!", message
        print(f"[client]    {message}")

        print("[client] 2. SayManyHellos (server streaming)")
        replies = [decode_hello_reply(r) for r in
                   say_many_hellos(encode_hello_request("stream", 5), timeout=5)]
        assert replies == [(f"Hello #{i}, stream!", i) for i in range(5)], replies
        print(f"[client]    {len(replies)} replies")

        print("[client] 3. SayManyHellos with the schema default count")
        replies = list(say_many_hellos(encode_hello_request("default"), timeout=5))
        assert len(replies) == 3, len(replies)

        print("[client] 4. SayHello with gzip compression")
        message, _ = decode_hello_reply(say_hello(encode_hello_request("gzip"), timeout=5,
                                                  compression=grpc.Compression.Gzip))
        assert message == "Hello, gzip!", message

        print("[client] 5. SayHello with a payload that is not a FlatBuffer")
        try:
            say_hello(b"definitely not a flatbuffer", timeout=5)
            raise AssertionError("expected INVALID_ARGUMENT")
        except grpc.RpcError as err:
            assert err.code() == grpc.StatusCode.INVALID_ARGUMENT, err.code()
            print(f"[client]    rejected: {err.details()}")

        print("[client] 6. SayManyHellos with an out of range count")
        try:
            list(say_many_hellos(encode_hello_request("x", 1000), timeout=5))
            raise AssertionError("expected INVALID_ARGUMENT")
        except grpc.RpcError as err:
            assert err.code() == grpc.StatusCode.INVALID_ARGUMENT, err.code()

    print("[client] ALL FLATBUFFERS TESTS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
