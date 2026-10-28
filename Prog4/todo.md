# TODOs

## Needs Fixing

### **Server Replying**

- [ ] Server peers must never receive a reply to `SENDMSG, KEEPALIVE`, or anything unrecognised. Log a `DROP` and don't reply quiet.

### **Poll Timeout**

- [ ] `poll(..., -1)` waits forever. Can’t send `KEEPALIVE`, retry connections, or expire anything. Need a finite timeout computed from next scheduled event.

### **Blocked Accepted Sockets**

- [ ] Establish per-connection outgoing buffers and `POLLOUT` for `accept()`.

### **Outgoing Connection**

- [ ] Implement an outgoing `connect()` that uses a non-blocking connect, completed via `POLLOUT` plus `getsockopt(SO_ERROR)`.

### **Connection Count Cap**

- [ ] Cap the connection count and handle that failure path.

### **Client Identification**

- [ ] Client needs to identify itself in a way peers can’t fake, for example a first command carrying a secret read from a file.

### **Message Storage Limit**

- [ ] Cap total and per-recipient storage, and validate group IDs against `[A-Za-z0-9-]{1,12}` plus the `G_/I_` prefix.

## Logging

- Accepted frames become `RECV/SEND` via `log_event_bytes`
- Malformed or unknown frames become `DROP`
- Connection lifecycle becomes `ACCEPT/CONN/CLOSE`
- Everything else currently logged as `INFO/WARNING` (“holding 3 for G_x”, “giving up reconnecting to...”) becomes `log_note`.

## Needs Extending

### **Client**

- [ ] The `Client` struct needs: IP and port (as strings/ints for log_event), a kind (unknown, our client, or peer server), the claimed group ID, the peer’s listening port if known, an outbound buffer, a last-seen time, and whether `HELO` has been exchanged.

### **Command Set**

- [ ] In `protocol`, the commands `HELO, SERVERS, KEEPALIVE, GETMSGS, STATUSRESP`, and the 4-field `SENDMSG` with a timestamp need to be implemented and handled. Stored message currently drops the timestamp entirely.

### **`STATUSRESP` Prefix**

- [ ] Server-to-server status replies need the `STATUSRESP,`prefix.

### **Port Parsing**

- [ ] Minor, `from_chars` accepts 4044abc as 4044 and doesn’t range-check.

