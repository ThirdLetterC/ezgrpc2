#!/usr/bin/env python3
"""
Python gRPC Client for ezgrpc2 Greeter Service.
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

        # 1. Call SayHello
        name1 = "Python"
        print(f"\n[client] Sending SayHello request (name='{name1}')...")
        req1 = helloworld_pb2.HelloRequest(name=name1)
        resp1 = stub.SayHello(req1, timeout=5)
        print(f"[client] Received response: '{resp1.message}'")
        assert resp1.message == f"Hello, {name1}!", f"Unexpected reply: {resp1.message}"

        # 2. Call SayHelloAgain
        name2 = "ezgrpc2"
        print(f"\n[client] Sending SayHelloAgain request (name='{name2}')...")
        req2 = helloworld_pb2.HelloRequest(name=name2)
        resp2 = stub.SayHelloAgain(req2, timeout=5)
        print(f"[client] Received response: '{resp2.message}'")
        assert resp2.message == f"Hello again, {name2}!", f"Unexpected reply: {resp2.message}"

    print("\n[client] All RPC tests PASSED successfully!")
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
