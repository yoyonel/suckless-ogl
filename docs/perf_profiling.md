# Profiling and Performance Monitoring

This document describes how to set up and use the profiling tools available in the `suckless-ogl` project.

## Profiling with perf

The `make perf` target uses the Linux `perf` tool to record and report performance data. Since the build environment often runs inside a container (like Distrobox), certain kernel security settings on the host must be adjusted.

### Resolving Access Permissions

If you encounter an error like `Access to performance monitoring and observability operations is limited`, you need to adjust the `perf_event_paranoid` setting.

#### 1. Temporary Fix (Immediate)
Run this command on your **host machine**:
```bash
sudo sysctl -w kernel.perf_event_paranoid=1
```

#### 2. Permanent Fix (Survives Reboot)
Add the setting to a sysctl configuration file:
```bash
echo "kernel.perf_event_paranoid = 1" | sudo tee /etc/sysctl.d/99-perf.conf
sudo sysctl --system
```

### Advanced Profiling Settings

#### Kernel Pointer Access
To see kernel symbols in your profiles (useful for identifying overhead in drivers or syscalls):
```bash
sudo sysctl -w kernel.kptr_restrict=0
```

## Usage

1.  **Build and Record:**
    ```bash
    make perf
    ```
    This will build the project with profiling symbols (`RelWithDebInfo`), run `perf record`, and then automatically open `perf report`.

2.  **View Report Manually:**
    If you want to view an existing `perf.data` file:
    ```bash
    distrobox enter <box-name> -- perf report
    ```

## Other Tools

-   **Apitrace:** Use `make apitrace` to capture OpenGL calls and `make qapitrace` to inspect them.
-   **Valgrind:** Use `make valgrind` for memory leak detection and basic profiling (though it is significantly slower).
