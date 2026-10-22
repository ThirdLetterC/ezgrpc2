#!/usr/bin/env python3
"""
Comprehensive Python gRPC Client for ezgrpc2 Greeter Service.
Tests:
  1. Unary RPC (SayHello) with custom binary metadata ('x-custom-bin') and trailing metadata verification.
  2. Unary RPC (SayHelloAgain).
  3. Server Streaming RPC (SayHelloStream).
  4. Client Streaming RPC (SayHelloClientStream).
  5. Bidirectional Streaming RPC (SayHelloBidiStream).
  6. Rich Error Model (SayHelloWithError) with 'grpc-status-details-bin' verification.
  7. Transparent Gzip Compression.
  8. Standard gRPC Health Check Protocol (/grpc.health.v1.Health/Check).
  9. Standard gRPC Server Reflection Protocol (/grpc.reflection.v1alpha.ServerReflection/ServerReflectionInfo).
"""

import sys
import base64
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

        # 4. Call SayHelloClientStream (Client Streaming)
        print(f"\n[client] 4. Calling SayHelloClientStream (client streaming)...")
        def request_generator():
            for n in ["Alice", "Bob", "Charlie"]:
                print(f"  [client -> server] Streaming item: {n}")
                yield helloworld_pb2.HelloRequest(name=n)

        resp_client_stream = stub.SayHelloClientStream(request_generator(), timeout=5)
        print(f"[client] Received response from client stream: '{resp_client_stream.message}'")
        assert "Alice" in resp_client_stream.message and "Bob" in resp_client_stream.message and "Charlie" in resp_client_stream.message
        assert "Total: 3" in resp_client_stream.message

        # 5. Call SayHelloBidiStream (Bidirectional Streaming)
        print(f"\n[client] 5. Calling SayHelloBidiStream (bidirectional streaming)...")
        bidi_names = ["Ping", "Pong", "Antigravity"]
        def bidi_generator():
            for n in bidi_names:
                yield helloworld_pb2.HelloRequest(name=n)

        bidi_replies = list(stub.SayHelloBidiStream(bidi_generator(), timeout=5))
        print(f"[client] Received {len(bidi_replies)} bidi replies:")
        for idx, r in enumerate(bidi_replies):
            print(f"  [{idx+1}] {r.message}")
            assert bidi_names[idx] in r.message
        assert len(bidi_replies) == 3

        # 6. Call SayHelloWithError (Rich Error Model)
        print(f"\n[client] 6. Calling SayHelloWithError (Rich Error Model)...")
        try:
            stub.SayHelloWithError(helloworld_pb2.HelloRequest(name=""), timeout=5)
            assert False, "Expected RPC error not raised"
        except grpc.RpcError as rpc_err:
            print(f"[client] Caught expected RPC error: code={rpc_err.code()}, details='{rpc_err.details()}'")
            assert rpc_err.code() == grpc.StatusCode.INVALID_ARGUMENT
            err_trailers = dict(rpc_err.trailing_metadata())
            print(f"[client] Error trailers: {err_trailers}")
            details_bin = err_trailers.get('grpc-status-details-bin')
            assert details_bin is not None, "Expected 'grpc-status-details-bin' in trailers"
            print(f"[client] Rich error details found: {details_bin} (length: {len(details_bin)} bytes)")
            # In grpc-python, binary metadata is returned as bytes
            if isinstance(details_bin, str):
                details_raw = base64.b64decode(details_bin)
            else:
                details_raw = details_bin
            assert b"ErrorInfo" in details_raw or b"INVALID_NAME" in details_raw
            print("[client] Rich Error Details successfully validated!")

        # 7. Call with Gzip Compression
        print(f"\n[client] 7. Testing Gzip compression...")
        resp_comp = stub.SayHello(
            helloworld_pb2.HelloRequest(name="CompressedClient"),
            compression=grpc.Compression.Gzip,
            timeout=5
        )
        print(f"[client] Received compressed response: '{resp_comp.message}'")
        assert resp_comp.message == "Hello, CompressedClient!"

        # 8. Standard gRPC Health Check Protocol
        print(f"\n[client] 8. Testing gRPC Health Check Protocol (/grpc.health.v1.Health/Check)...")
        health_check_fn = channel.unary_unary(
            '/grpc.health.v1.Health/Check',
            request_serializer=lambda x: x,
            response_deserializer=lambda x: x,
        )
        health_resp = health_check_fn(b'', timeout=5)
        print(f"[client] Health check raw response: {list(health_resp)}")
        assert len(health_resp) >= 2 and health_resp[0] == 0x08 and health_resp[1] == 0x01, \
            f"Expected SERVING (0x08, 0x01), got {health_resp}"
        print("[client] Health check status: SERVING (OK)")

        # 9. Standard gRPC Server Reflection Protocol
        print(f"\n[client] 9. Testing gRPC Server Reflection Protocol (list_services)...")
        refl_fn = channel.stream_stream(
            '/grpc.reflection.v1alpha.ServerReflection/ServerReflectionInfo',
            request_serializer=lambda x: x,
            response_deserializer=lambda x: x,
        )
        # ServerReflectionRequest { list_services = "" } -> Field 7 wire 2: tag 0x3A, len 0: b'\x3a\x00'
        refl_call = refl_fn(iter([b'\x3a\x00']))
        refl_resp = next(refl_call)
        print(f"[client] Reflection raw response: {len(refl_resp)} bytes")
        assert b"helloworld.Greeter" in refl_resp, "Expected helloworld.Greeter in reflection list"
        assert b"grpc.health.v1.Health" in refl_resp, "Expected grpc.health.v1.Health in reflection list"
        print("[client] Server Reflection successfully verified! Discovered services in reflection response.")

    print("\n[client] ALL TESTS (Metadata, Client Streaming, Bidi Streaming, Rich Errors, Compression, Health, Reflection) PASSED successfully!")
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except grpc.RpcError as e:
        print(f"[client] RPC failed with status code {e.code()}: {e.details()}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        import traceback
        traceback.print_exc()
        sys.exit(1)
