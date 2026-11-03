"""Run in a disposable process: python native_smoke.py DLL DATA_DIR OUTPUT.png."""
import ctypes as c
import os
import sys
import time
from pathlib import Path

dll, data, output = map(lambda value: Path(value).resolve(), sys.argv[1:4])
os.chdir(data)
host = c.CDLL(str(dll))
host.FitdUnity_CopyFrame.argtypes = [c.c_void_p, c.c_int, c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_uint64)]
host.FitdUnity_GetError.argtypes = [c.c_char_p, c.c_int]
host.FitdUnity_SetInput.argtypes = [c.c_int, c.c_int, c.c_int, c.c_float, c.c_float, c.c_int]
assert host.FitdUnity_GetAbiVersion() == 1
assert host.FitdUnity_Start(640, 400) == 1
width, height, sequence = c.c_int(), c.c_int(), c.c_uint64()
buffer = (c.c_ubyte * (640 * 400 * 4))()
try:
    deadline = time.monotonic() + 45
    count = 0
    while time.monotonic() < deadline:
        state = host.FitdUnity_GetState()
        if state == -1:
            message = c.create_string_buffer(1024)
            host.FitdUnity_GetError(message, len(message))
            raise AssertionError(message.value.decode())
        copied = host.FitdUnity_CopyFrame(buffer, len(buffer), c.byref(width), c.byref(height), c.byref(sequence))
        if copied > 0:
            count += 1
            if count >= 4 and max(buffer[0::4]) > 20:
                break
        time.sleep(0.05)
    else:
        raise AssertionError("No non-black native frame arrived")
    assert (width.value, height.value) == (640, 400)
    from PIL import Image
    Image.frombytes("RGBA", (640, 400), bytes(buffer)).transpose(Image.Transpose.FLIP_TOP_BOTTOM).save(output)
    host.FitdUnity_SetPaused(1)
    time.sleep(0.5)  # drain already submitted frames
    host.FitdUnity_CopyFrame(buffer, len(buffer), c.byref(width), c.byref(height), c.byref(sequence))
    time.sleep(0.2)
    assert host.FitdUnity_CopyFrame(buffer, len(buffer), c.byref(width), c.byref(height), c.byref(sequence)) == 0
    assert host.FitdUnity_GetState() == 2
    assert host.FitdUnity_Start(640, 400) == 1  # Editor reattachment resumes existing session
    time.sleep(0.3)
    assert host.FitdUnity_GetState() == 1
    host.FitdUnity_SetInput(1, 0, 0, -1.0, -1.0, 0)
    time.sleep(0.1)
    host.FitdUnity_SetInput(0, 0, 0, -1.0, -1.0, 0)
finally:
    host.FitdUnity_SetPaused(1)
    start = time.monotonic()
    host.FitdUnity_Stop()
    assert time.monotonic() - start < 15, "Native shutdown stalled"
assert host.FitdUnity_GetState() == 3
assert host.FitdUnity_Start(640, 400) == 0
print("PASS: ABI, frame capture, pause, resume, input, stop, and restart rejection")
