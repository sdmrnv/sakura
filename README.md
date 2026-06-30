### Sakura: High-Performance Database Proxy

**Sakura** is a lightweight, high-performance server written in pure C. It concurrently receives SQL commands via OS sockets and translates them directly to the Oracle RDBMS using the native Oracle Call Interface (OCI). 

### The Story Behind the Name

The name is a playful portmanteau derived from **SOC**ket and **ORA**cle: **SOCORA → SAKURA**. The project's philosophy is subtly inspired by the traditional Japanese concept of *mono-no aware* (the beautiful transience of things) — reflecting clean, ephemeral, and highly efficient resource utilization. 

### Architecture & Features

*   **Pure C Implementation:** Built on native Linux system calls and low-level OCI for maximum execution speed and zero overhead. 
*   **Socket Multiplexing:** Clients connect via standard sockets to execute SQL commands (DDL, DML, etc.). The proxy translates requests to the Oracle backend and streams responses back to the client. 
*   **Full Stack Provided:** Includes the core server, production Makefiles, system service scripts for start/stop management, a custom `lua-resty` module for the OpenResty framework, and sample client implementations written in both C and Lua. 

### License

This is an open-source project distributed under the **BSD 2-Clause License**. Feel free to clone, compile, and adapt it for your high-load infrastructure requirements.
