# Resource Simulator with Web UI
This program simulates the **Banker’s Algorithm** for deadlock avoidance in operating systems. It visualizes resource allocation and process states in real time via a built-in web server.

## How to Compile and Run (Windows MinGW)
Open a terminal in the project directory (where this README.md is located).
Compile with:

```cmd
gcc src/main.c mongoose.c -o sim -lws2_32 -liphlpapi
```

Run the simulator:
```cmd
./sim.exe
```

## How to Compile and Run (Linux)

Open a terminal in the project directory (where this README.md is located).
Compile with:

```bash
gcc src/main.c mongoose.c -o sim -pthread
```

Run the simulator:
```bash
./sim
```
> Note: If for any reason you can't compile this there is a precompiled executable in the `build` directory
> that might resolve the issue, it was built with MinGW toolchain and uses `UCRT`.

Open your browser and go to:
`http://localhost:8080`

### What to Expect

You’ll be prompted to enter:
- Number of processes (n)
- Number of resource types (m)
- Initial available resources
- Allocation and Max matrices (as in the Banker’s Algorithm)

The web UI will show:
- Real-time resource availability
- Process states (Waiting, Running, Finished)
- Current resource requests

The simulation runs once you enter all the inputs and until all processes complete, the UI updates 10 times per second.

> Note: Gmail does not allow arbiterary executables to be sent through emails
> as a consequence i can't provide a working executable
> i cannot provide a build script either as that is also
> blocked by gmail, you unfortunately need to build the project manualy

