#!/usr/bin/env python3
"""Start an example server, run its integration client, then stop the server."""

import argparse
import os
import subprocess
import sys
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    parser.add_argument("--client", required=True)
    parser.add_argument("--python-path", default="")
    parser.add_argument("--cwd", required=True)
    parser.add_argument("--port", required=True)
    parser.add_argument("--client-arg", action="append", default=[])
    parser.add_argument("--tls", action="store_true")
    args = parser.parse_args()

    env = os.environ.copy()
    if args.python_path:
        env["PYTHONPATH"] = os.pathsep.join(
            filter(None, [args.python_path, env.get("PYTHONPATH", "")])
        )
    server_env = env.copy()
    if args.tls:
        server_env["USE_TLS"] = "1"
        server_env["TLS_CERT"] = "server.crt"

    server_log = tempfile.TemporaryFile(mode="w+t")
    server = subprocess.Popen(
        [args.server, args.port], cwd=args.cwd, env=server_env,
        stdout=server_log, stderr=subprocess.STDOUT
    )
    try:
        time.sleep(1)
        if server.poll() is not None:
            server_log.seek(0)
            sys.stderr.write(server_log.read())
            return server.returncode or 1
        client_env = env.copy()
        if args.tls:
            client_env["USE_TLS"] = "1"
            client_env["TLS_CERT"] = os.path.join(args.cwd, "server.crt")
        result = subprocess.run(
            [sys.executable, args.client, *args.client_arg,
             f"localhost:{args.port}", *( ["--tls"] if args.tls else [] )],
            cwd=args.cwd, env=client_env, check=False
        )
        return result.returncode
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
        if server.returncode not in (0, -15, 143):
            server_log.seek(0)
            sys.stderr.write(server_log.read())
        server_log.close()


if __name__ == "__main__":
    sys.exit(main())
