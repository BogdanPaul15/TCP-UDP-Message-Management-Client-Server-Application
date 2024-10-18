## Project Description

My client-server architecture is divided into two main categories:

### Server:

- Disabled buffering for `stdout`.
- Parsed the arguments used to run the server (port number).
- Created a UDP socket (for communication with UDP clients) and a TCP socket (for communication with TCP clients).
- Disabled Nagle's algorithm to optimize data transmission.
- Initialized the server structure.
- Bound the two sockets to the server.
- Started listening for new TCP connections.
- Initialized the `pollfd` structure (initially containing 3 file descriptors: `STDIN`, `UDP`, and `TCP`).
- Started the polling process:
  - **STDIN:**
    - Handled the case when the server receives the `exit` command, closing all client connections and the server's own sockets (TCP and UDP).
    - Handled unrecognized commands.
  - **TCP:**
    - Accepted a new TCP connection.
    - Checked if the client ID attempting to connect already exists:
      - If not, it is considered a new client, added to the list of clients, and the connection is added to the list of file descriptors.
      - If it does exist:
        - If already connected, displayed the appropriate message and closed the new socket.
        - If not connected, marked the client as connected and displayed the corresponding message.
  - **UDP:**
    - Received a UDP message.
    - Created a structure to differentiate the message fields more easily.
    - Formatted the message according to the specifications.
    - Added the message length at the beginning.
    - Checked if the client was subscribed to the received topic and, if so, sent the message to the connected client.
  - **Existing Client:**
    - Identified the client by its ID.
    - Received a message from the client.
    - The first byte of the message represented the command (SUBSCRIBE, UNSUBSCRIBE, or EXIT):
      - **SUBSCRIBE:** Verified if the client was not already subscribed to the topic, and if not, added the topic to the client's list.
      - **UNSUBSCRIBE:** Checked if the client was subscribed to the topic, and if so, removed the topic.
      - **EXIT:** Changed the client status to DISCONNECTED and closed the corresponding file descriptor.

### Client:

- Disabled buffering for `stdout`.
- Parsed the arguments used to run the server (ID, IP address, port).
- Initialized the server structure.
- Created a TCP socket.
- Disabled Nagle's algorithm to optimize data transmission.
- Connected the socket to the server.
- Initialized the `pollfd` structure (containing only 2 file descriptors: `STDIN` and `TCP`).
- Started the polling process:
  - **STDIN:**
    - **Exit:** Sent the exit command to the server and closed the socket.
    - **Subscribe:** Sent the subscribe command along with the topic to the server.
    - **Unsubscribe:** Sent the unsubscribe command along with the topic to the server.
  - **TCP:** Received a TCP message from the server and displayed its content (already formatted).

### Message Framing

To implement an efficient data transmission protocol over TCP, I used message framing [1]. The first 4 bytes of each message represent the length, followed by the message content. This approach ensures precise message size detection, minimizing the risk of errors. I used `send_all` to guarantee that the entire packet content is sent, checking how much has been transmitted, and `recv_all`, which first receives the initial 4 bytes (message length) and then receives the rest of the content, returning only the actual content.

[1] [https://blog.stephencleary.com/2009/04/message-framing.html](https://blog.stephencleary.com/2009/04/message-framing.html)
