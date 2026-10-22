#!/usr/bin/env python3
"""
Comprehensive Python gRPC Client for ezgrpc2 Greeter Service.
Tests:
  1. Unary RPC (SayHello) with custom binary metadata ('x-custom-bin') and trailing metadata verification.
  2. Unary RPC (SayHelloAgain).
  3. Server Streaming RPC (SayHelloStream).
  4. Compression (SayHello with gzip compression).
  5. Standard gRPC Health Check Protocol (/grpc.health.v1.Health/Check).
"""

import sys
import grpc
import helloworld_pb2
import helloworld_pb2_grpc


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else 'localhost:19009'
    print(f"[client] Connecting to gRPC server at {target}...")

    # Connect with an insecure channel (HTTP/2 cleartext / h2c)
    with grpc.insecure_channel(target) as channel:
        # Wait up to 5 seconds for channel connectivity
        grpc.channel_ready_future(channel).result(timeout=5)
        print("[client] Connected successfully!")

        stub = helloworld_pb2_grpc.GreeterStub(channel)

        # 1. Call SayHello with custom binary metadata
        name1 = "Python"
        print(f"\n[client] 1. Sending SayHello request (name='{name1}') with binary metadata...")
        req1 = helloworld_pb2.HelloRequest(name=name1)
        metadata = (
            ('x-custom-bin', b'\xde\xad\xbe\xef\x01\x02\x03'),
        )
        call = stub.SayHello.with_call(req1, metadata=metadata, timeout=5)
        resp1, call_obj = call
        print(f"[client] Received response: '{resp1.message}'")
        assert resp1.message == f"Hello, {name1}!", f"Unexpected reply: {resp1.message}"

        # Inspect trailing metadata
        trailers = dict(call_obj.trailing_metadata())
        print(f"[client] Trailing metadata received: {trailers}")
        assert trailers.get('x-custom-reply') == 'pong', f"Expected trailer x-custom-reply=pong, got {trailers}"

        # 2. Call SayHelloAgain
        name2 = "ezgrpc2"
        print(f"\n[client] 2. Sending SayHelloAgain request (name='{name2}')...")
        req2 = helloworld_pb2.HelloRequest(name=name2)
        resp2 = stub.SayHelloAgain(req2, timeout=5)
        print(f"[client] Received response: '{resp2.message}'")
        assert resp2.message == f"Hello again, {name2}!", f"Unexpected reply: {resp2.message}"

        # 3. Call SayHelloStream (Server Streaming)
        name3 = "StreamingClient"
        print(f"\n[client] 3. Calling SayHelloStream (server streaming)...")
        req3 = helloworld_pb2.HelloRequest(name=name3)
        stream_replies = list(stub.SayHelloStream(req3, timeout=5))
        print(f"[client] Received {len(stream_replies)} stream messages:")
        for idx, r in enumerate(stream_replies, start=1):
            print(f"  [{idx}] {r.message}")
        assert len(stream_replies) == 3, f"Expected 3 stream replies, got {len(stream_replies)}"
        assert "Stream message #1" in stream_replies[0].message
        assert "Stream message #3" in stream_replies[2].message

        # 4. Call with Gzip Compression
        print(f"\n[client] 4. Testing Gzip compression...")
        resp_comp = stub.SayHello(
            helloworld_pb2.HelloRequest(name="CompressedClient"),
            compression=grpc.Compression.Gzip,
            timeout=5
        )
        print(f"[client] Received compressed response: '{resp_comp.message}'")
        assert resp_comp.message == "Hello, CompressedClient!"

        # 5. Standard gRPC Health Check Protocol
        print(f"\n[client] 5. Testing gRPC Health Check Protocol (/grpc.health.v1.Health/Check)...")
        health_check_fn = channel.unary_unary(
            '/grpc.health.v1.Health/Check',
            request_serializer=lambda x: x,
            response_deserializer=lambda x: x,
        )
        # Empty request checks overall server health
        health_resp = health_check_fn(b'', timeout=5)
        print(f"[client] Health check raw response: {list(health_resp)}")
        # Protobuf wire format: Tag 0x08 (field 1 varint), Value 0x01 (SERVING)
        assert len(health_resp) >= 2 and health_resp[0] == 0x08 and health_resp[1] == 0x01, \
            f"Expected SERVING (0x08, 0x01), got {health_resp}"
        print("[client] Health check status: SERVING (OK)")

    print("\n[client] ALL TESTS (Metadata, Streaming, Compression, Health) PASSED successfully!")
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except grpc.RpcError as e:
        print(f"[client] RPC failed with status code {e.code()}: {e.details()}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"[client] Error: {e}", file=sys.stderr)
        sys.exit(1)
