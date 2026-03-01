import subprocess
import json
import struct
import sys
import time

# Ensure the executable path matches your compiled output location.
# Example: 'build/Release/jsonrpc.exe' or just 'jsonrpc.exe' if in the same folder.
EXE_PATH = r"path/to/jsonrpc.exe"

def run_test():
    print(f"Starting subprocess: {EXE_PATH}")
    process = subprocess.Popen(
        [EXE_PATH], # Use the variable defined at the top
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=sys.stderr, # Redirect C++ cerr to the current console
        bufsize=0          # Unbuffered I/O
    )

    def send_raw(data_bytes):
        process.stdin.write(data_bytes)
        process.stdin.flush()

    def make_packet(method, params, _id):
        # Construct JSON body
        msg = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params,
            "id": _id
        }
        json_str = json.dumps(msg)
        
        # Construct LSP-style Header: Content-Length: <len>\r\n\r\n<body>
        # Note: The C++ read_loop handles cases where the body is not followed by \r\n.
        packet = f"Content-Length: {len(json_str)}\r\n\r\n{json_str}".encode('utf-8')
        return packet

    def read_response():
        # 1. Read Header
        content_length = 0
        while True:
            line = process.stdout.readline()
            if not line: return None
            line = line.decode('utf-8', errors='ignore').strip()
            
            if line == "": 
                # Empty line detected, Header section finished
                break
            
            if line.startswith("Content-Length:"):
                content_length = int(line.split(":")[1])

        # 2. Read Body
        if content_length > 0:
            body = process.stdout.read(content_length)
            return json.loads(body)
        return None

    try:
        # --- Test 1: Standard Request ---
        print("\n[Test 1] Standard Request (add 1+2)")
        send_raw(make_packet("add", [1, 2], 1))
        res = read_response()
        print(f"Received: {res}")
        assert res['result'] == 3
        assert res['id'] == 1
        print("PASS")

        # --- Test 2: Sticky Packets ---
        # Simulate Emacs sending two commands instantly; 
        # data is concatenated in the pipe (one write, two messages).
        print("\n[Test 2] Sticky Packets (Two requests in one write)")
        packet1 = make_packet("add", [10, 20], 2)
        packet2 = make_packet("echo", {"msg": "hello"}, 3)
        send_raw(packet1 + packet2) # Write all at once
        
        res1 = read_response()
        print(f"Received 1: {res1}")
        assert res1['result'] == 30
        
        res2 = read_response()
        print(f"Received 2: {res2}")
        assert res2['result']['msg'] == "hello"
        print("PASS")

        # --- Test 3: Split Packets ---
        # Simulate network delay or system buffering; 
        # Header and Body are sent/received separately.
        print("\n[Test 3] Split Packets (Header... delay ... Body)")
        packet = make_packet("add", [100, 200], 4)
        split_point = len(packet) - 5 # Cut inside the Body
        
        send_raw(packet[:split_point])
        time.sleep(0.1) # Simulate delay
        send_raw(packet[split_point:])
        
        res = read_response()
        print(f"Received: {res}")
        assert res['result'] == 300
        print("PASS")
        
        print("\nALL TESTS PASSED!")

    except Exception as e:
        print(f"\nFAILED: {e}")
    finally:
        process.terminate()

if __name__ == "__main__":
    run_test()
